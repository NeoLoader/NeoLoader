#include "GlobalHeader.h"
#include "TCPSocket.h"
#include "../NeoCore.h"
#include "SocketThread.h"

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
   #include <sys/ioctl.h>
   #define SOCKET_ERROR (-1)
   #define closesocket close
   #define WSAGetLastError() errno
   #define WSAEWOULDBLOCK EWOULDBLOCK
   #define WSAECONNREFUSED ECONNREFUSED
   #define WSAECONNRESET ECONNRESET
   #define WSAETIMEDOUT ETIMEDOUT

#ifdef __APPLE__
    #include "errno.h"
#endif
#endif

#define CHK_THREAD ASSERT(QThread::currentThread() == theCore->m_Network);

CTcpListener::CTcpListener(QObject* qObject)
: QObject(qObject) 
{
	m_Socket = INVALID_SOCKET;

	m_sa = NULL;
	m_sa_len = 0;
}

CTcpListener::~CTcpListener()
{
	Close();
}

bool CTcpListener::IsValid()
{
	return m_Socket != INVALID_SOCKET;
}

void CTcpListener::Close()
{
	CHK_THREAD

	if(m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}

	if(m_sa)
	{
		delete m_sa;
		m_sa = NULL;
	}
	m_sa_len = 0;
}

bool CTcpListener::Listen(quint16 Port, const CAddress& IP)
{
	CHK_THREAD
	ASSERT(m_Socket == INVALID_SOCKET);

	if(IP.AF() == AF_INET6)
	{
		m_sa_len = sizeof(sockaddr_in6);
		m_sa = (sockaddr*)new sockaddr_in6;
		if(!IP.IsNull())
			IP.ToSA(m_sa, &m_sa_len, Port);
		else
		{
			memset(m_sa, 0, m_sa_len);
			((sockaddr_in6*)m_sa)->sin6_family = AF_INET6;
			((sockaddr_in6*)m_sa)->sin6_addr = in6addr_any;
			((sockaddr_in6*)m_sa)->sin6_port = htons((u_short)Port);
		}
	}
	else
	{
		m_sa_len = sizeof(sockaddr_in);
		m_sa = (sockaddr*)new sockaddr_in;
		if(!IP.IsNull())
			IP.ToSA(m_sa, &m_sa_len, Port);
		else
		{
			memset(m_sa, 0, m_sa_len);
			((sockaddr_in*)m_sa)->sin_family = AF_INET;
			((sockaddr_in*)m_sa)->sin_addr.s_addr = INADDR_ANY;	
			((sockaddr_in*)m_sa)->sin_port = htons((u_short)Port);
		}
	}

	m_Socket = socket(IP.AF(), SOCK_STREAM, IPPROTO_IP);
	if (::bind(m_Socket, m_sa, m_sa_len) < 0)
	{
		Close();
		return false;
	}

	int MaxCon = theCore->Cfg()->GetInt("Bandwidth/MaxConnections");
	if (::listen(m_Socket, Max(10, MaxCon/10)) < 0)
	{
		Close();
		return false;
	}

	if(IP.IsNull()) // if we dont bind we dont need this
	{
		delete m_sa;
		m_sa = NULL;
	}
	else if(IP.AF() == AF_INET6)
		((sockaddr_in6*)m_sa)->sin6_port = 0;
	else
		((sockaddr_in*)m_sa)->sin_port = 0;

#ifdef WIN32
	u_long iMode = 1;
	ioctlsocket(m_Socket, FIONBIO, &iMode);
#else
	int iMode = fcntl(m_Socket, F_GETFL, 0);
	fcntl(m_Socket, F_SETFL, iMode | O_NONBLOCK);
#endif
	return true;
}

void CTcpListener::Process()
{
	if(m_Socket == INVALID_SOCKET)
		return;

	CStreamServer* pServer = ((CStreamServer*)parent());
	int MaxCon = theCore->Cfg()->GetInt("Bandwidth/MaxConnections");

	for(;;) // repeat untill all pending connections are accepted
	{
		sockaddr_in6 sa; // sockaddr_in is smaller
		socklen_t sa_len = sizeof(sa);
		SOCKET Socket = accept(m_Socket, (sockaddr*)&sa, &sa_len);
		if (Socket == INVALID_SOCKET)
			break;

		if(theCore->m_Network->GetCount() > MaxCon)
		{
			CAddress Address;
			Address.FromSA((sockaddr*)&sa, sa_len);
			if(!pServer->IsExpected(Address))
			{
				closesocket(Socket);
				continue;
			}
		}

		CStreamSocket* pSocket = pServer->AllocSocket(Socket);
		emit Connection(pSocket);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//

CTcpSocket::CTcpSocket(CStreamSocket* parent)
 : CAbstractSocket(parent) 
{
	m_Socket = INVALID_SOCKET;
	//m_State = CStreamSocket::eNotConnected;
	m_Connected = false;

#ifdef MSS
	m_FrameOH = 0;
#endif
}

CTcpSocket::~CTcpSocket()
{
	CHK_THREAD;
	if(m_Socket != INVALID_SOCKET)
		ClearSocket();
}

void CTcpSocket::SetSocket(SOCKET Socket, bool Connected)
{
	CHK_THREAD;
	ASSERT(m_Socket == INVALID_SOCKET);
	ASSERT(Socket != INVALID_SOCKET);

	m_Socket = Socket;
	m_Connected = Connected;
	
	int iSendSize = KB2B(16);
	setsockopt(m_Socket, SOL_SOCKET, SO_SNDBUF, (const char*)&iSendSize, sizeof(iSendSize));

	int iRecvSize = KB2B(64);
	setsockopt(m_Socket, SOL_SOCKET, SO_RCVBUF, (const char*)&iRecvSize, sizeof(iRecvSize));

#ifdef WIN32
	u_long iMode = 1;
	ioctlsocket(m_Socket, FIONBIO, &iMode);
#else
	int iMode = fcntl(m_Socket, F_GETFL, 0);
	fcntl(m_Socket, F_SETFL, iMode | O_NONBLOCK);
#endif

	//if(Connected)
	//	m_State = CStreamSocket::eConnected;

#ifdef MSS
	CStreamSocket* pSocket = GetStream(); // the overhead depands on the IP version
	m_FrameOH = (pSocket->GetAddress().Type() == CAddress::IPv6 ? 40 : 20); // IPv6 / IPv4 Header;
	AddSynOH(Connected);
#endif
}

void CTcpSocket::ClearSocket(bool Closed)
{
	CHK_THREAD;
	ASSERT(m_Socket != INVALID_SOCKET);

	closesocket(m_Socket);
	m_Socket = INVALID_SOCKET;

#ifdef MSS
	AddSynOH(Closed); // fin
#endif
}

void CTcpSocket::ConnectToHost(const CAddress& Address, quint16 Port)
{
	CHK_THREAD;
	if(m_Socket != INVALID_SOCKET)
	{
		ASSERT(0);
		return;
	}

	CTcpListener* pListener = GetStream()->GetServer()->GetTCPListener(Address);
	if(!pListener)
	{
		emit Disconnected(SOCK_ERR_NETWORK);
		return;
	}

	bool bIPv6 = pListener->m_sa_len == sizeof(sockaddr_in6);
	SOCKET Socket = socket(bIPv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_IP);

	if(pListener->m_sa)
	{
		if (::bind(Socket, pListener->m_sa, pListener->m_sa_len) < 0)
		{
			closesocket(Socket);
			//int Error = WSAGetLastError();
			emit Disconnected(SOCK_ERR_NETWORK);
			return;
		}
	}

	SetSocket(Socket, false);

	sockaddr_in6 sa;
	int sa_len = sizeof(sockaddr_in6);
	Address.ToSA((struct sockaddr*)&sa, &sa_len, Port);
	if(::connect(Socket, (struct sockaddr*)&sa, sa_len) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			DisconnectFromHost(SOCK_ERR_NETWORK);
	}
}

void CTcpSocket::DisconnectFromHost(int Error)
{
	CHK_THREAD;
	if(m_Socket != INVALID_SOCKET /*&& (m_State == CStreamSocket::eConnected || m_State == CStreamSocket::eConnecting)*/)
	{
		//m_State = CStreamSocket::eDisconnected;
		ClearSocket(Error ? true : false);
	}

	emit Disconnected(Error);
}

CAddress CTcpSocket::GetAddress(quint16* pPort) const
{
	CHK_THREAD;
	sockaddr_in6 sa; // sockaddr_in is smaller
	socklen_t sa_len = sizeof(sa);
	::getpeername(m_Socket, (sockaddr*)&sa, &sa_len);

	CAddress Address;
	Address.FromSA((const struct sockaddr*)&sa, sa_len, pPort);
	return Address;
}

bool CTcpSocket::Process()
{
	CHK_THREAD;
	if(m_Socket == INVALID_SOCKET)
		return false;

    struct timeval tv;
    fd_set writefds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&writefds);
	FD_SET(m_Socket, &writefds);
    int ret = select(m_Socket + 1, NULL, &writefds, NULL, &tv);
	if (ret == SOCKET_ERROR)
	{
		DisconnectFromHost(m_Connected ? SOCK_ERR_RESET : SOCK_ERR_REFUSED);
		return false;
	}

	int so_error;
    socklen_t len = sizeof so_error;
    getsockopt(m_Socket, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
	if(so_error)
	{
		int Error;
		switch(so_error)
		{
			case WSAECONNREFUSED:	Error = SOCK_ERR_REFUSED;	break;
			case WSAECONNRESET:		Error = SOCK_ERR_RESET;		break;
			case WSAETIMEDOUT:		Error = SOCK_ERR_RESET;		break;
			default:				Error = SOCK_ERR_NETWORK;	break;
		}
		DisconnectFromHost(Error);
		return false;
	}

	if(!m_Connected)
	{
		if(!FD_ISSET(m_Socket, &writefds))
			return true;
		m_Connected = true;
		emit Connected();
	}
	else
	{
		if(FD_ISSET(m_Socket, &writefds))
			m_Blocking = false;
	}

	return false;
}

qint64 CTcpSocket::Recv(char *data, qint64 maxlen)
{
	if(!m_Connected)
		return 0;
	int Recived = recv(m_Socket, data, maxlen, 0);
	if (Recived == SOCKET_ERROR)
	{
		uint32 Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
		{
			DisconnectFromHost(SOCK_ERR_RESET);
			return -1;
		}
		return 0;
	}
#ifdef MSS
	AddFrameOH(Recived, CBandwidthCounter::eHeader, CBandwidthCounter::eAck);
#endif
	return Recived;
}

qint64 CTcpSocket::Send(const char *data, qint64 len)
{
	if(!m_Connected)
		return 0;
	int Sent = send(m_Socket, data, len, 0);
	if (Sent == SOCKET_ERROR)
	{
		uint32 Error = WSAGetLastError();
		if (Error != WSAEWOULDBLOCK)
		{
			DisconnectFromHost(SOCK_ERR_RESET);
			return -1;
		}
		else
			m_Blocking = true;
		return 0;
	}
#ifdef MSS
	AddFrameOH(Sent, CBandwidthCounter::eAck, CBandwidthCounter::eHeader);
#endif
	return Sent;
}

qint64 CTcpSocket::RecvPending() const
{
	u_long uBytes;
#ifdef WIN32
	ioctlsocket(m_Socket, FIONREAD, &uBytes);
#else
	fcntl(m_Socket, FIONREAD, &uBytes);
#endif
	return uBytes;
}

#ifdef MSS
void CTcpSocket::AddFrameOH(int Size, CBandwidthCounter::EType TypeDown, CBandwidthCounter::EType TypeUp)
{
	CStreamSocket* pSocket = GetStream();

	// Note: TCP does not fragment frames, so sach frame is IP Header + TCP Header
	uint64 FrameOH = m_FrameOH + 20; // IP Header + TCP Header
	
	uint64 uMSS = MSS - FrameOH;
	int iFrames = (Size + (uMSS-1))/uMSS; // count if IP frames

	FrameOH += theCore->m_Network->GetFrameOverhead(); // Ethernet
	pSocket->CountBandwidth(CBandwidthLimiter::eDownChannel, iFrames * FrameOH, TypeDown);
	pSocket->CountBandwidth(CBandwidthLimiter::eUpChannel, iFrames * FrameOH, TypeUp);
}

void CTcpSocket::AddSynOH(bool In)
{
	CStreamSocket* pSocket = GetStream(); // TCP sync is a 3 way handshake

	uint64 FrameOH = m_FrameOH + 20; // IP Header + TCP Header

	FrameOH += theCore->m_Network->GetFrameOverhead(); // Ethernet
	pSocket->CountBandwidth(In ? CBandwidthLimiter::eDownChannel : CBandwidthLimiter::eUpChannel, 2 * FrameOH, CBandwidthCounter::eAll); // SYN + ACK (2 packets)
	pSocket->CountBandwidth(In ? CBandwidthLimiter::eUpChannel : CBandwidthLimiter::eDownChannel, FrameOH, CBandwidthCounter::eAll); // SYN / ACK (1 packet)
}
#endif
