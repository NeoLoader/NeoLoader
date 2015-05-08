#include "GlobalHeader.h"
#include "UTPSocket.h"
#include "SocketThread.h"
#include "../NeoCore.h"
#include "../../Framework/Exception.h"
#include "../../Framework/Buffer.h"
#include "ListenSocket.h"
#include "../../utp/utp.h"

#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <fcntl.h>
   #define SOCKET_ERROR (-1)
   #define closesocket close
   #define WSAGetLastError() errno
#ifdef __APPLE__
    #include "errno.h"
#endif
#endif

#define CHK_THREAD ASSERT(QThread::currentThread() == theCore->m_Network);

CUtpTimer::CUtpTimer(QObject* qObject) 
: QObject(qObject) 
{
	CHK_THREAD;

#ifdef WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);

	if (0 != WSAStartup(wVersionRequested, &wsaData))
		LogLine(LOG_ERROR, L"Couldn't initialise windows sockets");
#endif

	m_uTimerID = startTimer(100);
}

CUtpTimer::~CUtpTimer()
{
	CHK_THREAD;

	killTimer(m_uTimerID);

#ifdef WIN32
	WSACleanup();
#endif
}

void CUtpTimer::timerEvent(QTimerEvent* pEvent)	
{
	if(pEvent->timerId() == m_uTimerID)
	{
		CHK_THREAD;
		UTP_CheckTimeouts();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//

//int CUtpListener::m_Counter = 0;
//CUtpTimer* CUtpListener::m_pTimer = NULL;

CUtpListener::CUtpListener(QObject* qObject)
: QObject(qObject)
{
	m_Socket = INVALID_SOCKET;
}

CUtpListener::~CUtpListener()
{
	if(m_Socket != INVALID_SOCKET)
	{
		CHK_THREAD;

		closesocket(m_Socket);
		//m_Counter--;
		//if(m_Counter == 0)
		//	delete m_pTimer;
	}
}

bool CUtpListener::IsValid()
{
	return m_Socket != INVALID_SOCKET;
}

void CUtpListener::Close()
{
	CHK_THREAD;

	if(m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
}

bool CUtpListener::Bind(quint16 Port, const CAddress& IP)
{
	CHK_THREAD;
	ASSERT(m_Socket == INVALID_SOCKET);

	//if(m_Counter == 0)
	//{
	//	ASSERT(m_pTimer == NULL);
	//	m_pTimer = new CUtpTimer();
	//}
	//m_Counter++;

	sockaddr_in6 sa;
	int sa_len = 0;
	if(IP.AF() == AF_INET6)
	{
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
		Close();
		return false;
	}

	int iSize = 128 * 1024;
	setsockopt(m_Socket, SOL_SOCKET, SO_RCVBUF, (const char*)&iSize, sizeof(iSize));
	setsockopt(m_Socket, SOL_SOCKET, SO_SNDBUF, (const char*)&iSize, sizeof(iSize));

#ifdef WIN32
	u_long iMode = 1;
	ioctlsocket(m_Socket, FIONBIO, &iMode);
#else
	int iMode = fcntl(m_Socket, F_GETFL, 0);
	fcntl(m_Socket, F_SETFL, iMode | O_NONBLOCK);
#endif
	return true;
}

void send_to(void *userdata, const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen)
{
	CUtpListener* pListener = ((CUtpListener*)userdata);
	CAddress Receiver;
	quint16 ReceiverPort;
	Receiver.FromSA(to, tolen, &ReceiverPort);
	pListener->SendDatagram((const char*)p, len, Receiver, ReceiverPort);
}

void utp_read(void* userdata, const byte* bytes, size_t count)
{
	CUtpSocket* pSocket = ((CUtpSocket*)userdata);
	// data have been recived
	pSocket->m_ReadBuffer.AppendData(bytes, count);
	//emit pSocket->readyRead();
}

size_t utp_get_rb_size(void* userdata)
{
	CUtpSocket* pSocket = ((CUtpSocket*)userdata);
	size_t size = pSocket->m_ReadBuffer.GetSize();
	// Note: due to the way our BC works any set limit causes delayed read from the buffer
	//			and that causes in turn a speed reduction, so we hide up to the first few kb
#define HIDE_SIZE KB2B(64)
	size = size < HIDE_SIZE ? 0 : (size - HIDE_SIZE);
	return size;
}

void utp_write(void* userdata, byte* bytes, size_t count)
{
	CUtpSocket* pSocket = ((CUtpSocket*)userdata);
	// get data to be sent
	ASSERT(count <= pSocket->m_WriteBuffer.GetSize());
	memcpy(bytes, pSocket->m_WriteBuffer.GetBuffer(), count);
	pSocket->m_WriteBuffer.ShiftData(count);
	pSocket->m_Blocking = false;
	//emit pSocket->bytesWritten(count);
}

void utp_state(void* userdata, int state)
{
	CUtpSocket* pSocket = ((CUtpSocket*)userdata);

	switch(state)
	{
	case UTP_STATE_CONNECT:
		//pSocket->m_State = CStreamSocket::eConnected;
		emit pSocket->Connected();
		break;
	case UTP_STATE_WRITABLE:
		pSocket->m_Blocking = false;
		//emit pSocket->bytesWritten(0);
		break;
	case UTP_STATE_EOF:
		//pSocket->m_State = CStreamSocket::eDisconnected;
		ASSERT(pSocket->m_iClosed == 0);
		pSocket->m_iClosed = -1; // -1 socket gracefully closed
		UTP_Close(pSocket->m_Socket);
		break;
	case UTP_STATE_DESTROYING:
		pSocket->m_Socket = NULL;
		emit pSocket->Disconnected(pSocket->m_iClosed > 0 ? pSocket->m_iClosed : 0);
		break;
	}
}

void utp_error(void* userdata, int errcode)
{
	CUtpSocket* pSocket = ((CUtpSocket*)userdata);

	int Error;
	switch(errcode)
	{
	case ECONNREFUSED:	Error = SOCK_ERR_REFUSED;	break;
	case ECONNRESET:	Error = SOCK_ERR_RESET;		break;
	case ETIMEDOUT:		Error = SOCK_ERR_RESET;		break;
	default:			Error = SOCK_ERR_NETWORK;	break;
	}
	pSocket->DisconnectFromHost(Error);
}

CBandwidthCounter::EType UtpType(int type)
{
	switch (type)
	{
	case 4: return CBandwidthCounter::eHeader;
	case 3: return CBandwidthCounter::eAck;
	default: return CBandwidthCounter::eAll;
	}
}

void utp_overhead(void *userdata, bool send, size_t count, int type)
{
	count += theCore->m_Network->GetFrameOverhead(); // Ethernet
	CUtpSocket* pSocket = ((CUtpSocket*)userdata);
	ASSERT(type);
	pSocket->GetStream()->CountBandwidth(send ? CBandwidthLimiter::eUpChannel: CBandwidthLimiter::eDownChannel, (int)count, UtpType(type));
}

void got_incoming_connection(void *userdata, struct UTPSocket *socket)
{
	CUtpListener* pListener = ((CUtpListener*)userdata);

	CStreamServer* pServer = ((CStreamServer*)pListener->parent());
	int MaxCon = theCore->Cfg()->GetInt("Bandwidth/MaxConnections");

	if(theCore->m_Network->GetCount() > MaxCon)
	{
		sockaddr_in6 sa; // sockaddr_in is smaller
		socklen_t sa_len = sizeof(sa);
		UTP_GetPeerName(socket, (sockaddr*)&sa, &sa_len);
		CAddress Address;
		Address.FromSA((const struct sockaddr*)&sa, sa_len);
		if(!pServer->IsExpected(Address))
		{
			UTP_Close(socket);
			return;
		}
	}

	CStreamSocket* pSocket = pServer->AllocUTPSocket(socket);
	emit pListener->Connection(pSocket);
}

void utp_overhead_2(void *userdata, bool send, size_t count, int type)
{
	count += theCore->m_Network->GetFrameOverhead(); // Ethernet
	CUtpListener* pListener = ((CUtpListener*)userdata);
	CStreamServer* pServer = ((CStreamServer*)pListener->parent());
	ASSERT(type);
	if(send)
	{
		((CSocketThread*)pServer->thread())->GetUpLimit()->CountBytes(count, UtpType(type));
		pServer->GetUpLimit()->CountBytes(count, UtpType(type));
	}
	else
	{
		((CSocketThread*)pServer->thread())->GetDownLimit()->CountBytes(count, UtpType(type));
		pServer->GetDownLimit()->CountBytes(count, UtpType(type));
	}
}

void CUtpListener::SendDatagram(const char *data, qint64 len, const CAddress &host, quint16 port)
{
	if(m_Socket == INVALID_SOCKET)
		return;

	sockaddr_in6 sa;
	int sa_len = sizeof(sockaddr_in6);
	host.ToSA((struct sockaddr*)&sa, &sa_len, port);

	sendto(m_Socket, data, len, 0, (struct sockaddr*)&sa, sa_len);
}

void CUtpListener::ReciveDatagram(const char *data, qint64 len, const CAddress &host, quint16 port)
{
	sockaddr_in6 sa;
	int sa_len = sizeof(sockaddr_in6);
	host.ToSA((struct sockaddr*)&sa, &sa_len, port);

	UTP_IsIncomingUTP(&got_incoming_connection, &send_to, this, (const byte*)data, len, (const struct sockaddr*)&sa, sa_len, &utp_overhead_2);
}

void CUtpListener::Process()
{
	if(m_Socket == INVALID_SOCKET)
		return;

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

		CAddress Address;
		quint16 Port;
		Address.FromSA((const struct sockaddr*)&sa, sa_len, &Port);
		ReciveDatagram((const char*)buffer, len, Address, Port);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//

CUtpSocket::CUtpSocket(CStreamSocket* parent)
 : CAbstractSocket(parent) 
{
	m_Socket = NULL;
	m_iClosed = -1;
	//m_State = CStreamSocket::eNotConnected;

	m_ReadBuffer.AllocBuffer(KB2B(64), false, false); // Note: UTP can push more data into the buffer than expected
	m_WriteBuffer.AllocBuffer(KB2B(16));
}

CUtpSocket::~CUtpSocket()
{
	CHK_THREAD;
	if(m_Socket)
	{
		UTP_SetCallbacks(m_Socket, NULL, NULL); // make sure this will not be refered anymore
		m_Socket = NULL;
	}
}

void CUtpSocket::SetSocket(struct UTPSocket* Socket, bool Connected)
{
	CHK_THREAD;
	ASSERT(m_Socket == NULL);
	ASSERT(Socket != NULL);

	m_Socket = Socket;
	m_iClosed = 0;
	
	UTP_SetSockopt(m_Socket, SO_SNDBUF, (int)m_WriteBuffer.GetLength());
	UTP_SetSockopt(m_Socket, SO_RCVBUF, (int)m_ReadBuffer.GetLength());

	UTPFunctionTable utp_callbacks = {&utp_read, &utp_write, &utp_get_rb_size, &utp_state, &utp_error, &utp_overhead};
	UTP_SetCallbacks(m_Socket, &utp_callbacks, this);

	if(!Connected)
	{
		//m_State = CStreamSocket::eConnecting;
		UTP_Connect(m_Socket);
	}
	//else
		//m_State = CStreamSocket::eConnected;
}

void CUtpSocket::ConnectToHost(const CAddress& Address, quint16 Port)
{
	CHK_THREAD;
	if(m_Socket)
	{
		//ASSERT(0);
		return;
	}

	CUtpListener* pListener = GetStream()->GetServer()->GetUTPListener(Address);
	if(!pListener)
	{
		emit Disconnected(SOCK_ERR_NETWORK);
		return;
	}

	sockaddr_in6 sa;
	int sa_len = sizeof(sockaddr_in6);
	Address.ToSA((struct sockaddr*)&sa, &sa_len, Port);

	struct UTPSocket* Socket = UTP_Create(&send_to, pListener, (struct sockaddr*)&sa, sa_len, &utp_overhead_2);
	SetSocket(Socket, false);
}

void CUtpSocket::DisconnectFromHost(int Error)
{
	CHK_THREAD;
	if(m_Socket /*&& (m_State == CStreamSocket::eConnected || m_State == CStreamSocket::eConnecting)*/)
	{
		//m_State = CStreamSocket::eDisconnected;
		// Note: we have to issue close and wait for UTP to destroy the socket, than we can say disconnected
		if(m_iClosed == 0)
			UTP_Close(m_Socket);
		m_iClosed = Error ? Error : -1; // > 0 error, -1 proper close
	}
	else
		emit Disconnected(Error);
}

CAddress CUtpSocket::GetAddress(quint16* pPort) const
{
	CHK_THREAD;
	sockaddr_in6 sa; // sockaddr_in is smaller
	socklen_t sa_len = sizeof(sa);
	UTP_GetPeerName(m_Socket, (sockaddr*)&sa, &sa_len);

	CAddress Address;
	Address.FromSA((const struct sockaddr*)&sa, sa_len, pPort);
	return Address;
}

bool CUtpSocket::Process()
{
	CHK_THREAD;
	if(m_Socket && m_WriteBuffer.GetSize() != 0)
		UTP_Write(m_Socket, m_WriteBuffer.GetSize());
	return false;
}

qint64 CUtpSocket::Recv(char *data, qint64 maxlen)
{
	if(!IsValid())
		return -1;
	size_t ToGo = Min(m_ReadBuffer.GetSize(), (size_t)maxlen);
	if(ToGo > 0)
	{
		memcpy(data, m_ReadBuffer.GetBuffer(), ToGo);
		m_ReadBuffer.ShiftData(ToGo);
	}
	return ToGo;
}

qint64 CUtpSocket::Send(const char *data, qint64 len)
{
	if(!IsValid())
		return -1;
	size_t ToGo = Min(m_WriteBuffer.GetLength() - m_WriteBuffer.GetSize(), (size_t)len);
	if(ToGo > 0)
		m_WriteBuffer.AppendData(data, ToGo);
	else
		m_Blocking = true;
	return ToGo;
}