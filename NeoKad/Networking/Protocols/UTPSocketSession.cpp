#include "GlobalHeader.h"

#include "UTPSocketSession.h"
#include "../SmartSocket.h"
#include "../../../utp/utp.h"
#include "../BandwidthControl/BandwidthLimit.h"


CUTPSocketListner::CUTPSocketListner(CSmartSocket* pSocket)
 : CSocketListner(pSocket)
{
	m_bIPv6 = false;
	m_Port = 0;

#ifdef WIN32
	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(ret != 0)
	{
		LogLine(LOG_ERROR, L"WSAStartup() failed, err: %1", ret);
		return;
	}
#endif

	m_Socket = INVALID_SOCKET;
	m_RecvKey = pSocket->GetRecvKey();
	m_NextCleanUp = 0;
}

CUTPSocketListner::~CUTPSocketListner()
{
	Close();

#ifdef WIN32
	WSACleanup();
#endif

	for(TKeyMap::iterator I = m_SendKeys.begin(); I != m_SendKeys.end();)
	{
		SPassKey* pSendKey = I->second;
		I = m_SendKeys.erase(I);
		delete pSendKey;
	}

	const list<CObject*>& Children = GetChildren();
	for(list<CObject*>::const_iterator I = Children.begin(); I != Children.end(); I++)
	{
		ASSERT((*I)->Inherits(CUTPSocketSession::StaticName()));
		CUTPSocketSession* pSession = (CUTPSocketSession*)*I;
		pSession->Close();
	}
}

void CUTPSocketListner::Close()
{
	if(m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
}

bool CUTPSocketListner::Bind(uint16 Port, const CAddress& IP)
{
	ASSERT(m_Socket == INVALID_SOCKET);

	sockaddr_in6 sa;
	int sa_len = 0;
	if(IP.AF() == AF_INET6)
	{
		m_bIPv6 = true;

		sa_len = sizeof(sockaddr_in6);
		if(!IP.IsNull())
			IP.ToSA((sockaddr*)&sa, &sa_len, Port);
		else
		{
			memset(&sa, 0, sa_len);
			((sockaddr_in6*)&sa)->sin6_family = AF_INET6;
			((sockaddr_in6*)&sa)->sin6_addr = in6addr_any;
			((sockaddr_in6*)&sa)->sin6_port = htons((u_short)Port);
		}
	}
	else
	{
		m_bIPv6 = false;

		sa_len = sizeof(sockaddr_in);
		if(!IP.IsNull())
			IP.ToSA((sockaddr*)&sa, &sa_len, Port);
		else
		{
			memset(&sa, 0, sizeof(sockaddr_in));
			((sockaddr_in*)&sa)->sin_family = AF_INET;
			((sockaddr_in*)&sa)->sin_addr.s_addr = INADDR_ANY;	
			((sockaddr_in*)&sa)->sin_port = htons((u_short)Port);
		}
	}

	m_Socket = socket(IP.AF(), SOCK_DGRAM, IPPROTO_IP);
	if (::bind(m_Socket, (sockaddr*)&sa, sa_len) < 0)
	{
		LogLine(LOG_ERROR, L"UTP - UDP port bind(%d) failed: %d %S\n", m_Port, errno, strerror(errno));
		return false;
	}

	// Mark to hold a couple of megabytes
	int iSize = 2 * 1024 * 1024;
	if (setsockopt(m_Socket, SOL_SOCKET, SO_RCVBUF, (const char*)&iSize, sizeof(iSize)) < 0)
		LogLine(LOG_ERROR, L"UTP - UDP setsockopt(SO_RCVBUF, %d) failed: %d %S\n", iSize, errno, strerror(errno));
	if (setsockopt(m_Socket, SOL_SOCKET, SO_SNDBUF, (const char*)&iSize, sizeof(iSize)) < 0)
		LogLine(LOG_ERROR, L"UTP - UDP setsockopt(SO_SNDBUF, %d) failed: %d %S\n", iSize, errno, strerror(errno));

	// make socket non blocking
#ifdef _WIN32
	u_long iMode = 1;
	ioctlsocket(m_Socket, FIONBIO, &iMode);
#else
	int iMode = fcntl(m_Socket, F_GETFL, 0);
	fcntl(m_Socket, F_SETFL, iMode | O_NONBLOCK);
#endif

	m_Port = Port;
	LogLine(LOG_SUCCESS, L"%s Socket is listening at port %d", m_bIPv6 ? L"UTPv6" : L"UTP", m_Port);
	return true;
}

void send_to(void *userdata, const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen)
{
	CUTPSocketListner* pListner = ((CUTPSocketListner*)userdata);
	pListner->Send(p, len, to, tolen, 1);
}

void utp_read(void* userdata, const byte* bytes, size_t count)
{
	CUTPSocketSession* pSession = ((CUTPSocketSession*)userdata);
	// data have been recived
	pSession->PushData(bytes, count);
}

void utp_write(void* userdata, byte* bytes, size_t count)
{
	CUTPSocketSession* pSession = ((CUTPSocketSession*)userdata);
	// get data to be sent
	pSession->PullData(bytes, count);
}

size_t utp_get_rb_size(void* userdata)
{
	CUTPSocketSession* pSession = ((CUTPSocketSession*)userdata);
	return pSession->m_Receiving.GetSize();
}

void utp_state(void* userdata, int state)
{
	CUTPSocketSession* pSession = ((CUTPSocketSession*)userdata);
	if (state == UTP_STATE_EOF)
		pSession->Close();
	else
		pSession->m_State = state;
	/*else if (state == UTP_STATE_DESTROYING) 
		closed done;*/
}

void utp_error(void* userdata, int errcode)
{
	CUTPSocketSession* pSession = ((CUTPSocketSession*)userdata);
	pSession->Close();
}

void utp_overhead(void *userdata, bool send, size_t count, int type)
{
	CUTPSocketSession* pSession = ((CUTPSocketSession*)userdata);
	if(type)
		pSession->CountBandwidth(send ? CBandwidthLimiter::eUpChannel : CBandwidthLimiter::eDownChannel , (int)count); 
}

void got_incoming_connection(void *userdata, struct UTPSocket *socket)
{
	CUTPSocketListner* pListner = ((CUTPSocketListner*)userdata);
	
	sockaddr_in6 sa; // sockaddr_in is smaller
	socklen_t sa_len = sizeof(sa);
	UTP_GetPeerName(socket, (sockaddr*)&sa, &sa_len);

	CUTPSocketListner::TKeyMap::iterator I = pListner->m_SendKeys.find((struct sockaddr*)&sa);
	if(I == pListner->m_SendKeys.end())
	{
		UTP_Close(socket);
		return;
	}
	ASSERT(I->second->PassKey);

	CSafeAddress Address((sockaddr*)&sa, sa_len, sa_len == sizeof(sockaddr_in) ? CSafeAddress::eUTP_IP4 : CSafeAddress::eUTP_IP6);
	Address.SetPassKey(I->second->PassKey);

	CSmartSocket* pSocket = pListner->GetParent<CSmartSocket>();
	CUTPSocketSession* pSession = new CUTPSocketSession(pListner, socket, Address);
	
	UTPFunctionTable utp_callbacks = {&utp_read, &utp_write, &utp_get_rb_size, &utp_state, &utp_error, &utp_overhead};
	UTP_SetCallbacks(socket, &utp_callbacks, pSession);

	pSocket->InsertSession(pSession);
}

void utp_overhead_2(void *userdata, bool send, size_t count, int type)
{
	CUTPSocketListner* pListner = ((CUTPSocketListner*)userdata);
	CSmartSocket* pSocket = pListner->GetParent<CSmartSocket>();
	if(send)
		pSocket->GetUpLimit()->CountBytes(count);
	else
		pSocket->GetDownLimit()->CountBytes(count);
}

void CUTPSocketListner::Process()
{
	/*int microsec = 0;
	struct timeval tv = {microsec / 1000000, microsec % 1000000};
	fd_set r, e;
	FD_ZERO(&r);
	FD_ZERO(&e);
	FD_SET(m_Socket, &r);
	FD_SET(m_Socket, &e);
	int ret = ::select(m_Socket + 1, &r, 0, &e, &tv);
	if (ret == 0) 
		return;

	if (ret < 0) 
	{
		LogLine(LOG_ERROR, L"UTP - UDP select() failed: %S\n", strerror(errno));
		return;
	}

	if (FD_ISSET(m_Socket, &r))*/
	{
		byte buffer[0xFFFF];
		sockaddr_in6 sa; // sockaddr_in is smaller
		socklen_t sa_len = sizeof(sa);

		for (;;) 
		{
			int len = recvfrom(m_Socket, (char*)buffer, sizeof(buffer), 0, (struct sockaddr*)&sa, &sa_len);
			if (len < 0) 
			{
				int err = WSAGetLastError();
				// ECONNRESET - On a UDP-datagram socket
				// this error indicates a previous send operation
				// resulted in an ICMP Port Unreachable message.
				if (err == ECONNRESET) 
					continue;
				// EMSGSIZE - The message was too large to fit into
				// the buffer pointed to by the buf parameter and was
				// truncated.
				if (err == EMSGSIZE) 
					continue;
				// any other error (such as EWOULDBLOCK) results in breaking the loop
				break;
			}

			Recv(buffer, len, (struct sockaddr*)&sa, sa_len);
		}
	}
	/*if (FD_ISSET(m_Socket, &e)) 
		LogLine(LOG_ERROR, L"UTP - UDP socket error!\n");*/

	UTP_CheckTimeouts();

	uint64 uNow = GetCurTick();
	if(m_NextCleanUp < uNow) // cleanup interval once per minute
	{
		m_NextCleanUp = uNow + CLEANUP_INTERVAL;
		TKeyMap SendKeys = m_SendKeys;

		const list<CObject*>& Children = GetChildren();
		for(list<CObject*>::const_iterator I = Children.begin(); I != Children.end(); I++)
		{
			ASSERT((*I)->Inherits(CUTPSocketSession::StaticName()));
			CUTPSocketSession* pSession = (CUTPSocketSession*)*I;

			sockaddr_in6 sa; // sockaddr_in is smaller
			int sa_len = sizeof(sa);
			pSession->GetAddress().ToSA((sockaddr*)&sa, &sa_len);
		
			TKeyMap::iterator J = SendKeys.find((struct sockaddr*)&sa);
			if(J != SendKeys.end())
				SendKeys.erase(J);
		}

		for(TKeyMap::iterator J = SendKeys.begin(); J != SendKeys.end(); J++)
		{
			TKeyMap::iterator K = m_SendKeys.find(J->first);
			if(K != m_SendKeys.end() && K->second->LastActivity + CLEANUP_INTERVAL/2 < uNow)
			{
				SPassKey* pSendKey = K->second;
				m_SendKeys.erase(K);
				delete pSendKey;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          Radom Seed                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ <- from here on obfuscation starts
//   | Type  |       |   Reserved    |       |Discard|P| Padding Len |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ <- from here additional key stream is discarded
//   |                           (PassKey)                           |
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                                                               |
//   ~                            Payload                            ~
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ <- from here Padding Len Padding starts up to 127 bytes
//   |                         (Magic Value)                         |		Note: we put new optinal fields into the footer as this way we maintain compatybility
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
//   ~						  Random Padding                         ~		Note: we limit the actual padding to max 63 bytes to keep space for footer extensions
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Discard: amount of key bytes to be discarded in multiple of 256 in addition to the first 256
//
//	 nibble Type:
//		0: RAW
//		1: UTP v1
//		2: undefined
//		...
//		14: undefined
//		15: NAT-T
//
//   bit P:
//      0: PassKey not available
//      1: PassKey Present
//
//   Padding Len:
//		7 bit padding length 0 - 127
//
//

union UHdr
{
	uint32	Bits;
	struct SHdr
	{
		uint32	
		Type:		4,
		Reserved:	16,
		Discard:	4,
		HasKey:		1,
		PadLen:		7;
	}		Fields;
};

#define NEO_MAGIC "TLO1" // Transport Layer Obfuscation 1

void CUTPSocketListner::Recv(const byte* Buffer, size_t uSize, const struct sockaddr* sa, socklen_t sa_len)
{
	byte* pBuffer = (byte*)Buffer;
	size_t uPos = 0;

	if(uSize < 8)
		return;

	uint32 Rand;
	memcpy(&Rand, pBuffer, sizeof(uint32));						uPos += 4;

	CryptoPP::Weak1::MD5 md5;
	md5.Update((byte*)&Rand, sizeof(uint32));
	md5.Update((byte*)&m_RecvKey, sizeof(uint64));
	byte Hash[16];
	md5.Final(Hash);

//#ifdef _DEBUG
//	LogLine(LOG_INFO | LOG_DEBUG, L"Recv from %s PassKey %I64u -> %s", 
//		CSafeAddress(sa, sa_len, sa_len == sizeof(sockaddr_in) ? CSafeAddress::eUTP_IP4 : CSafeAddress::eUTP_IP6).ToString().c_str(),
//		m_RecvKey, ToHex(Hash, 16).c_str());
//#endif

	CryptoPP::Weak::ARC4::Encryption RC4;
	RC4.SetKey(Hash, 16);
	RC4.DiscardBytes(256);

	RC4.ProcessData(pBuffer + 4, pBuffer + 4, 4);
	UHdr Hdr;
	memcpy(&Hdr.Bits, pBuffer + uPos, 4);						uPos += 4;

	if(Hdr.Fields.Discard)
		RC4.DiscardBytes(Hdr.Fields.Discard * 256);

	RC4.ProcessData(pBuffer + uPos, pBuffer + uPos, uSize - uPos);

	uint64 PassKey = 0;
	if(Hdr.Fields.HasKey)
	{
		if(uSize - uPos < 8)
			return;
		memcpy(&PassKey, pBuffer + uPos, sizeof(uint64));		uPos += 8;
	}

	if(uSize - uPos < Hdr.Fields.PadLen)
		return;
	uSize -= Hdr.Fields.PadLen;
	
	if(Hdr.Fields.Reserved != 0) // if we want to use the reserverd bits we must set the magic value
	{
		if(memcmp(pBuffer + uSize, NEO_MAGIC, 4) != 0)
			return;
	}

	TKeyMap::iterator I = m_SendKeys.find((struct sockaddr*)sa);
	if(I == m_SendKeys.end())
	{
		if(PassKey == 0)
			return; // drop this packet as wen cant answer it

		SPassKey* pSendKey = new SPassKey((struct sockaddr*)sa);
		I = m_SendKeys.insert(TKeyMap::value_type(pSendKey->sa, pSendKey)).first;
	}
	if(PassKey != 0)
		I->second->PassKey = PassKey;
	//I->second->bAck = true; // if we got a packet it means that the other site knows out passkey and those we dont have to send it
	I->second->LastActivity = GetCurTick();


	switch(Hdr.Fields.Type)
	{
	case 0:
		{
			CBuffer Packet(pBuffer + uPos, uSize - uPos, true);
			CSafeAddress Address((sockaddr*)sa, sa_len, sa_len == sizeof(sockaddr_in) ? CSafeAddress::eUTP_IP4 : CSafeAddress::eUTP_IP6);
			Address.SetPassKey(I->second->PassKey);
			ReceiveFrom(Packet, Address);
			break;
		}
	case 1:
		// Lookup the right UTP socket that can handle this message
		UTP_IsIncomingUTP(&got_incoming_connection, &send_to, this, pBuffer + uPos, uSize - uPos, (const struct sockaddr*)sa, sa_len, &utp_overhead_2);
		break;
	case 15:
		{
			if(m_RecvKey < I->second->PassKey) // we have to decide who does the Sync, we do that in a trivial way
				Send(NULL, 0, sa, sa_len, 15);
			else
			{
				CSafeAddress Address(sa, sa_len, sa_len == sizeof(sockaddr_in) ? CSafeAddress::eUTP_IP4 : CSafeAddress::eUTP_IP6);
				Address.SetPassKey(I->second->PassKey);
				CSmartSocket* pSocket = GetParent<CSmartSocket>();
				CUTPSocketSession* pSession = pSocket->NewChannel(Address)->Cast<CUTPSocketSession>();
				if(pSession && !pSession->IsValid())
					pSession->Connect(sa, sa_len);
			}
			break;
		}
	}
}

union UUtpHdr
{
	uint32 Bits;
	struct SUtpHdr
	{
		uint32
		ver:	4,
		type:	4,
		ext:	8,
		connid:	16;
	} Fields;
};

void CUTPSocketListner::Send(const byte* Buff, size_t uSize, const struct sockaddr* sa, socklen_t sa_len, uint8 Type)
{
	TKeyMap::iterator I = m_SendKeys.find((struct sockaddr*)sa);
	if(I == m_SendKeys.end())
		return;
	I->second->LastActivity = GetCurTick();
	ASSERT(I->second->PassKey);

	if(uSize > 0xFFFF - 0x80)
	{
		ASSERT(0);
		return;
	}

	bool SendKey = false;
	int PadLen = 16;
	if(Type == 1) // if its a UTP packet lets be a bit smart
	{
		ASSERT(uSize >= 4);
		UUtpHdr UtpHdr;
		UtpHdr.Bits = *((uint32*)Buff);
		
		if(UtpHdr.Fields.type == 4) // ST_SYN
			SendKey = true; // we always send the passked on y UTP Sync packet

		if(UtpHdr.Fields.type == 0) // ST_DATA
			PadLen = 0; // we dont needto padd data frames as tahy may already on thair own have a random size
	}
	else //if(!I->second->bAck) // if we are not talking UDT lets always send the key just to be sure
		SendKey = true;


	char Buffer[0xFFFF];
	byte* pBuffer = (byte*)Buffer;
	size_t uLength = 0;

	CryptoPP::AutoSeededRandomPool rng;
	uint32 Rand = 0;
	rng.GenerateBlock((byte*)&Rand, sizeof(uint32));
	UHdr Hdr;
	Hdr.Fields.Type = Type;
	Hdr.Fields.Reserved = 0;
	Hdr.Fields.Discard = 0; // total drop ist (n+1)*256
	if(uSize >= 10 * 1024)
	   Hdr.Fields.Discard = 3; // discard only 1024 key bytes in total for large packets
	Hdr.Fields.HasKey = SendKey;
	if(PadLen)
		PadLen = (rand() % (PadLen + 1)) & 0x3F; // ensure 64 bytes are available for optional footer entries
	Hdr.Fields.PadLen = PadLen;
	//if(Hdr.Fields.Reserved != 0)
	//	Hdr.Fields.PadLen += 4;


	memcpy(pBuffer + uLength, &Rand, sizeof(uint32));			uLength += 4;
	memcpy(pBuffer + uLength, &Hdr.Bits, sizeof(uint32));		uLength += 4;
	if(Hdr.Fields.HasKey) {
		memcpy(pBuffer + uLength, &m_RecvKey, sizeof(uint64));	uLength += 8; }
	
	memcpy(pBuffer + uLength, Buff, uSize);						uLength += uSize;

	//if(Hdr.Fields.Reserved != 0) {
	//	memcpy(pBuffer + uLength, NEO_MAGIC, 4);				uLength += 4; }
	if(PadLen > 0) {
		rng.GenerateBlock(pBuffer + uLength, PadLen);			uLength += PadLen; }

	CryptoPP::Weak1::MD5 md5;
	md5.Update((byte*)&Rand, sizeof(Rand));
	md5.Update((byte*)&I->second->PassKey, sizeof(uint64));
	byte Hash[16];
	md5.Final(Hash);

//#ifdef _DEBUG
//	LogLine(LOG_INFO | LOG_DEBUG, L"Send to %s PassKey %I64u -> %s", 
//		CSafeAddress(sa, sa_len, sa_len == sizeof(sockaddr_in) ? CSafeAddress::eUTP_IP4 : CSafeAddress::eUTP_IP6).ToString().c_str(),
//		I->second->PassKey, ToHex(Hash, 16).c_str());
//#endif

	CryptoPP::Weak::ARC4::Encryption RC4;
	RC4.SetKey(Hash, 16);
	RC4.DiscardBytes(256);

	RC4.ProcessData(pBuffer + 4, pBuffer + 4, 4);
	if(Hdr.Fields.Discard)
		RC4.DiscardBytes(Hdr.Fields.Discard * 256);

	RC4.ProcessData(pBuffer + 8, pBuffer + 8, uLength - 8);

	sendto(m_Socket, Buffer, (int)uLength, 0, (struct sockaddr*)sa, sa_len);
}

bool CUTPSocketListner::SendTo(const CBuffer& Packet, const CSafeAddress& Address)
{
	sockaddr_in6 sa; // sockaddr_in is smaller
	int sa_len = sizeof(sa);
	Address.ToSA((sockaddr*)&sa, &sa_len);

	TKeyMap::iterator I = m_SendKeys.find((struct sockaddr*)&sa);
	if(I == m_SendKeys.end())
	{
		SPassKey* pSendKey = new SPassKey((struct sockaddr*)&sa);
		I = m_SendKeys.insert(TKeyMap::value_type(pSendKey->sa, pSendKey)).first;
	}
	ASSERT(Address.GetPassKey());
	I->second->PassKey = Address.GetPassKey();
	//I->second->bAck = false; // reset just in case
	I->second->LastActivity = GetCurTick();

	Send(Packet.GetBuffer(), Packet.GetSize(), (struct sockaddr*)&sa, sa_len, 0);
	return true;
}

CSocketSession* CUTPSocketListner::CreateSession(const CSafeAddress& Address, bool bRendevouz, bool bEmpty)
{
	if(bEmpty)
		return new CUTPSocketSession(this, NULL, Address);

	sockaddr_in6 sa; // sockaddr_in is smaller
	int sa_len = sizeof(sa);
	Address.ToSA((sockaddr*)&sa, &sa_len);

	TKeyMap::iterator I = m_SendKeys.find((struct sockaddr*)&sa);
	if(I == m_SendKeys.end())
	{
		SPassKey* pSendKey = new SPassKey((struct sockaddr*)&sa);
		I = m_SendKeys.insert(TKeyMap::value_type(pSendKey->sa, pSendKey)).first;
	}
	ASSERT(Address.GetPassKey());
	I->second->PassKey = Address.GetPassKey();
	//I->second->bAck = false; // reset just in case
	I->second->LastActivity = GetCurTick();

	CUTPSocketSession* pSession = new CUTPSocketSession(this, NULL, Address);
	if(bRendevouz)
		Send(NULL, 0, (const struct sockaddr*)&sa, sa_len, 15);
	else
		pSession->Connect((const struct sockaddr*)&sa, sa_len);
	return pSession;
}

///////////////////////////////////////////////////////////////////////////////////////
//

CUTPSocketSession::CUTPSocketSession(CSocketListner* pListener, struct UTPSocket* Socket, const CSafeAddress& Address)
: CSocketSession(pListener, Address)
{
	m_Socket = Socket;
	m_State = 0;
}

CUTPSocketSession::~CUTPSocketSession()
{
	if(IsValid())
		Close();
}

void CUTPSocketSession::Connect(const struct sockaddr *sa, socklen_t sa_len)
{
	ASSERT(!IsValid());

	m_Socket = UTP_Create(&send_to, GetParent<CUTPSocketListner>(), sa, sa_len, &utp_overhead_2);
	//UTP_SetSockopt(m_Socket, SO_SNDBUF, 100*300);

	UTPFunctionTable utp_callbacks = {&utp_read, &utp_write, &utp_get_rb_size, &utp_state, &utp_error, &utp_overhead};
	UTP_SetCallbacks(m_Socket, &utp_callbacks, this);
	UTP_Connect(m_Socket);
}

void CUTPSocketSession::Close()
{
	if(m_Socket)
	{
		UTP_Close(m_Socket);
		UTP_SetCallbacks(m_Socket, NULL, NULL); // make sure this will not be refered anymore
	}
	m_Socket = NULL;
	m_State = UTP_STATE_EOF;
}

void CUTPSocketSession::PushData(const byte* Buffer, size_t Recived)
{
	StreamIn((byte*)Buffer, Recived);
	
	m_LastActivity = GetCurTick();
}

void CUTPSocketSession::Process()
{
	CSocketSession::Process();

	if(!IsValid())
		return;

	size_t ToGo = m_Sending.GetSize();
	if(ToGo)
		UTP_Write(m_Socket, ToGo);
}

void CUTPSocketSession::PullData(byte* Buff, size_t uSize)
{
	ASSERT(uSize <= m_Sending.GetSize());

	memcpy(Buff, m_Sending.GetBuffer(), uSize);
	m_Sending.ShiftData(uSize);
	
	m_LastActivity = GetCurTick();
}

bool CUTPSocketSession::IsConnected() const
{
	return IsValid() && (m_State == UTP_STATE_CONNECT || m_State == UTP_STATE_WRITABLE);
}

bool CUTPSocketSession::IsBussy() const
{
	if(!IsValid())
		return false;

	if(m_PacketQueue.GetSize() > 0)
		return true;
	if(m_Sending.GetSize() > 0)
		return true;
	if(m_Receiving.GetSize() > 0)
		return true;

	return false;
}

bool CUTPSocketSession::IsDisconnected() const
{
	return !IsValid();
}

void CUTPSocketSession::Swap(CSocketSession* pNew)
{
	ASSERT(m_Socket == NULL);

	m_Socket = ((CUTPSocketSession*)pNew)->m_Socket;
	((CUTPSocketSession*)pNew)->m_Socket = NULL;
	m_LastActivity = GetCurTick();

	ASSERT(m_Socket);
	UTPFunctionTable utp_callbacks = {&utp_read, &utp_write, &utp_get_rb_size, &utp_state, &utp_error, &utp_overhead};
	UTP_SetCallbacks(m_Socket, &utp_callbacks, this);

	delete pNew;
}
