#include "GlobalHeader.h"

#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif

#include "UDTSocketSession.h"
#include "../SmartSocket.h"

int CUDTSocketListner::m_Count = 0;

CUDTSocketListner::CUDTSocketListner(CSmartSocket* pSocket, bool bIPv6, uint16 Port)
 : CSocketListner(pSocket)
{
	if(m_Count++ == 0)
		UDT::startup();

	m_bIPv6 = bIPv6;
	m_Port = 0;

	if(m_bIPv6)
	{
		m_sa = (sockaddr*)new sockaddr_in6;
		memset(m_sa, 0, sizeof(sockaddr_in6));
		((sockaddr_in6*)m_sa)->sin6_family = AF_INET6;
		((sockaddr_in6*)m_sa)->sin6_addr = in6addr_any;
		((sockaddr_in6*)m_sa)->sin6_port = htons((u_short)Port);
	}
	else
	{
		m_sa = (sockaddr*)new sockaddr_in;
		memset(m_sa, 0, sizeof(sockaddr_in));
		((sockaddr_in*)m_sa)->sin_family = AF_INET;
		((sockaddr_in*)m_sa)->sin_addr.s_addr = INADDR_ANY;	
		((sockaddr_in*)m_sa)->sin_port = htons((u_short)Port);
	}

	m_Server = UDT::socket(m_bIPv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_IP);

	if(uint64 RecvKey = pSocket->GetRecvKey())
	{
		UDT::setsockopt(m_Server, 0, UDT_RECVKEY, &RecvKey, sizeof(RecvKey));
		bool Obfuscate = true;
		UDT::setsockopt(m_Server, 0, UDT_OBFUSCATE, &Obfuscate, sizeof(Obfuscate));
	}

	bool DirectUDP = true;
	UDT::setsockopt(m_Server, 0, UDT_DIRECT, &DirectUDP, sizeof(DirectUDP));

	bool reuse_addr = true; // Note: this is true by default anyways
	UDT::setsockopt(m_Server, 0, UDT_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
	if (UDT::ERROR == UDT::bind(m_Server, m_sa, m_bIPv6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in)))
	{
		LogLine(LOG_ERROR, L"bind: %S", UDT::getlasterror().getErrorMessage());
		return;
	}
	if (UDT::ERROR == UDT::listen(m_Server, 1024))
	{
		LogLine(LOG_ERROR, L"listen: %S", UDT::getlasterror().getErrorMessage());
		return;
	}

	m_Port = Port;
	LogLine(LOG_SUCCESS, L"%s Socket is listening at port %d", m_bIPv6 ? L"UDTv6" : L"UDT", m_Port);

	bool blockng = false;
	UDT::setsockopt(m_Server, 0, UDT_RCVSYN, &blockng, sizeof(blockng));
}

CUDTSocketListner::~CUDTSocketListner()
{
	if(m_Port)
		UDT::close(m_Server);

	if(m_bIPv6)
		delete ((sockaddr_in6*)m_sa);
	else
		delete ((sockaddr_in*)m_sa);

	if(--m_Count == 0)
		UDT::cleanup();
}

void CUDTSocketListner::ConfigSocket(UDTSOCKET Client)
{
	//uint64_t MaxBW = KB2B(10);
	//UDT::setsockopt(Client, 0, UDT_MAXBW, &MaxBW, sizeof(MaxBW));

	//int FC = 32; // Maximum number of packets in flight from the peer side
	//UDT::setsockopt(Client, 0, UDT_FC, &FC, sizeof(FC));

	int SndBuf = 1500;
	UDT::setsockopt(Client, 0, UDT_SNDBUF, &SndBuf, sizeof(SndBuf));

	bool blocking = false;
	UDT::setsockopt(Client, 0, UDT_RCVSYN, &blocking, sizeof(blocking));
	UDT::setsockopt(Client, 0, UDT_SNDSYN, &blocking, sizeof(blocking));
}

void CUDTSocketListner::Process()
{
	for(;;) // repeat untill all pending connections are accepted
	{
		/*timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		UDT::UDSET readfds;
		UD_ZERO(&readfds);
		UD_SET(m_Server, &readfds);
		int res	= UDT::select(0, &readfds, NULL, NULL, &tv);
		if (!((res != UDT::ERROR) && (UD_ISSET(m_Server, &readfds))))
			break;*/
		
		sockaddr_in6 sa; // sockaddr_in is smaller
		int sa_len = sizeof(sa); 
		UDTSOCKET Client = UDT::accept(m_Server, (sockaddr*)&sa, &sa_len);
		if (UDT::INVALID_SOCK == Client)
		{
			LogLine(LOG_ERROR, L"accept: %S", UDT::getlasterror().getErrorMessage());
			break;
		}
		else if(Client == NULL)
			break;
		
		ConfigSocket(Client);

		uint64_t SendKey = 0;
		int KeySize = sizeof(SendKey);
		UDT::getsockopt(Client, 0, UDT_SENDKEY, &SendKey, &KeySize);

		CSafeAddress Address((sockaddr*)&sa, sa_len, sa_len == sizeof(sockaddr_in) ? CSafeAddress::eUDT_IP4 : CSafeAddress::eUDT_IP6);
		Address.SetPassKey(SendKey);
		GetParent<CSmartSocket>()->AddSessions(Address, new CUDTSocketSession(this, Client, Address));
	}

	const uint64 Size = 0xFFFF;
	char Buffer[Size];
	for(;;) // repeat untill all data is read
	{
		sockaddr_in6 sa; // sockaddr_in is smaller
		int sa_len = sizeof(sa); 
		uint64_t RecvKey = 0;
		int Recived = UDT::recvfrom(m_Server, Buffer, Size, (sockaddr*)&sa, &sa_len, &RecvKey);
		if (UDT::ERROR == Recived)
		{
			LogLine(LOG_ERROR, L"recvfrom: %S", UDT::getlasterror().getErrorMessage());
			break;
		}
		else if(Recived == 0)
			break; // nothing more to be recived
		CBuffer Packet(Buffer, Recived, true);

		CSafeAddress Address((sockaddr*)&sa, sa_len, sa_len == sizeof(sockaddr_in) ? CSafeAddress::eUDT_IP4 : CSafeAddress::eUDT_IP6);
		Address.SetPassKey(RecvKey);
		ReceiveFrom(Packet, Address);
	}
}

bool CUDTSocketListner::SendTo(const CBuffer& Packet, const CSafeAddress& Address)
{
	if(Packet.GetSize() > 0xFFFF - (60 + 20 + 24)) // Max UDP Datagram Size = SHORT_MAX - IP header size (20 to 60) - UDP header (20) - UDT header Size (16, or 24 with obfuscation)
	{
		ASSERT(0);
		return false;
	}

	sockaddr_in6 sa; // sockaddr_in is smaller
	int sa_len = sizeof(sa);
	Address.ToSA((sockaddr*)&sa, &sa_len);
	int Sent = UDT::sendto(m_Server, (char*)Packet.GetBuffer(), Packet.GetSize(), (sockaddr*)&sa, sa_len, Address.GetPassKey());
	if (UDT::ERROR == Sent)
	{
		LogLine(LOG_ERROR, L"sendto: %S", UDT::getlasterror().getErrorMessage());
		return false;
	}
	else if(Sent == 0)
		return false;
	return true;
}

CSocketSession* CUDTSocketListner::CreateSession(const CSafeAddress& Address, bool bRendevouz, bool bEmpty)
{
	if(bEmpty)
		return new CUDTSocketSession(this, UDT::INVALID_SOCK, Address);

	sockaddr_in6 sa; // sockaddr_in is smaller
	int sa_len = sizeof(sa);
	Address.ToSA((sockaddr*)&sa, &sa_len);

	UDTSOCKET Client = UDT::socket(m_bIPv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_IP);

	CSmartSocket* pSocket = GetParent<CSmartSocket>();
	if(uint64 RecvKey = pSocket->GetRecvKey())
	{
		UDT::setsockopt(Client, 0, UDT_RECVKEY, &RecvKey, sizeof(RecvKey));
		bool Obfuscate = true;
		UDT::setsockopt(Client, 0, UDT_OBFUSCATE, &Obfuscate, sizeof(Obfuscate));
	}
	if(uint64 SendKey = Address.GetPassKey())
		UDT::setsockopt(Client, 0, UDT_SENDKEY, &SendKey, sizeof(SendKey));

	ConfigSocket(Client);
	
	bool reuse_addr = true; // Note: this is true by default anyways
	UDT::setsockopt(Client, 0, UDT_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
	if(bRendevouz)
	{
		bool value = true;
		UDT::setsockopt(Client, 0, UDT_RENDEZVOUS, &value, sizeof(value));
	}
	if (UDT::ERROR == UDT::bind(Client, m_sa, m_bIPv6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in)))
	{
		LogLine(LOG_ERROR, L"bind: %S", UDT::getlasterror().getErrorMessage());
		return NULL;
	}
	if (UDT::ERROR == UDT::connect(Client, (sockaddr*)&sa, sa_len))
	{
		LogLine(LOG_ERROR, L"connect: %S", UDT::getlasterror().getErrorMessage());
		return NULL;
	}

	return new CUDTSocketSession(this, Client, Address);
}

///////////////////////////////////////////////////////////////////////////////////////
//

CUDTSocketSession::CUDTSocketSession(CSocketListner* pListener, UDTSOCKET Client, const CSafeAddress& Address)
: CSocketSession(pListener, Address)
{
	m_Client = Client;
}

CUDTSocketSession::~CUDTSocketSession()
{
	if(IsValid())
		UDT::close(m_Client);
}

void CUDTSocketSession::Process()
{
	if(!IsValid())
		return;

	UDTSTATUS Value = UDT::getsockstate(m_Client);
	if(Value == CONNECTING)
		return;
	else if(Value != CONNECTED)
	{
		m_LastActivity = 0; // timeout now
		return;
	}

	for(;;) // repeat untill all data is read
	{
		/*timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		UDT::UDSET readfds;
		UD_ZERO(&readfds);
		UD_SET(m_Client, &readfds);
		int res	= UDT::select(0, &readfds, NULL, NULL, &tv); // this is soooo slow
		if (!((res != UDT::ERROR) && (UD_ISSET(m_Client, &readfds))))
			break;*/

		const uint64 Size = KB2B(16);
		char Buffer[Size];
		int Recived = UDT::recv(m_Client, (char*)Buffer, Size, 0);
		if (UDT::ERROR == Recived)
		{
			LogLine(LOG_ERROR, L"recv: %S", UDT::getlasterror().getErrorMessage());
			break;
		}
		else if(Recived == 0)
			break; // hothing (more) to be read

		StreamIn((byte*)Buffer, Recived);

		m_LastActivity = GetCurTick();
	}

	CSocketSession::Process();

	for(;m_Sending.GetSize() > 0;) // repeat untill all data is sent
	{
		/*timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		UDT::UDSET writefds;
		UD_ZERO(&writefds);
		UD_SET(m_Client, &writefds);
		int res	= UDT::select(0, NULL, &writefds, NULL, &tv);
		if (!((res != UDT::ERROR) && (UD_ISSET(m_Client, &writefds))))
			break;*/

		/*uint32_t InBuffer = 0;
		int InSize = sizeof(InBuffer);
		UDT::getsockopt(m_Client, 0, UDT_SNDDATA, &InBuffer, &InSize);
		if(InBuffer > 0)
			break;*/

		int ToGo = m_Sending.GetSize();
		/*if(ToGo > KB2B(10))
			ToGo = KB2B(10);*/
		int Sent = UDT::send(m_Client, (char*)m_Sending.GetBuffer(), ToGo, 0);
		if (UDT::ERROR == Sent)
		{
			LogLine(LOG_ERROR, L"recv: %S", UDT::getlasterror().getErrorMessage());
			break;
		}

		m_Sending.ShiftData(Sent);

		m_LastActivity = GetCurTick();

		if(Sent < ToGo)
			break;
	}
}

bool CUDTSocketSession::IsConnected()
{
	UDTSTATUS Value = UDT::getsockstate(m_Client);
	return Value == CONNECTED;
}

void CUDTSocketSession::Swap(CSocketSession* pNew)
{
	ASSERT(m_Client == UDT::INVALID_SOCK);

	m_Client = ((CUDTSocketSession*)pNew)->m_Client;
	m_LastActivity = GetCurTick();

	((CUDTSocketSession*)pNew)->m_Client = UDT::INVALID_SOCK;
	delete pNew;
}
