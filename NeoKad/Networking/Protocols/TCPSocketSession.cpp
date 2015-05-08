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

/*void GetPrimaryIp(char* buffer, size_t buflen) 
{
    assert(buflen >= 16);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock != -1);

    const char* kGoogleDnsIp = "8.8.8.8";
    uint16 kDnsPort = 53;
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(kGoogleDnsIp);
    serv.sin_port = htons(kDnsPort);

    int err = connect(sock, (const sockaddr*) &serv, sizeof(serv));
    assert(err != -1);

    sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (sockaddr*) &name, &namelen);
    assert(err != -1);

    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, buflen);
    assert(p);

    closesocket(sock);
}*/

#include "TCPSocketSession.h"
#include "../SmartSocket.h"

union sockaddr_u
{
	sockaddr_in		v4;
	sockaddr_in6	v6;
};

CTCPSocketListner::CTCPSocketListner(CSmartSocket* pSocket, bool bIPv6, uint16 Port)
 : CSocketListner(pSocket)
{
	m_bIPv6 = bIPv6;
	m_Port = Port;

	if(m_bIPv6)
	{
		m_sa = (sockaddr*)new sockaddr_in6;
		((sockaddr_in6*)m_sa)->sin6_family = AF_INET6;
		((sockaddr_in6*)m_sa)->sin6_addr = in6addr_any;
		((sockaddr_in6*)m_sa)->sin6_port = htons((u_short)m_Port);
	}
	else
	{
		m_sa = (sockaddr*)new sockaddr_in;
		((sockaddr_in*)m_sa)->sin_family = AF_INET;
		((sockaddr_in*)m_sa)->sin_addr.s_addr = INADDR_ANY;	
		((sockaddr_in*)m_sa)->sin_port = htons((u_short)m_Port);
	}

	m_Server = socket(m_bIPv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (SOCKET_ERROR == bind(m_Server, m_sa, m_bIPv6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in)))
	{
		LogLine(LOG_ERROR, L"bind: %d", WSAGetLastError());
		return;
	}
	if (SOCKET_ERROR == listen(m_Server, 1024))
	{
		LogLine(LOG_ERROR, L"listen: %d", WSAGetLastError());
		return;
	}

	m_Socket = socket(m_bIPv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (SOCKET_ERROR == bind(m_Socket, m_sa, m_bIPv6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in)))
	{
		LogLine(LOG_ERROR, L"bind: %d", WSAGetLastError());
		return;
	}

	LogLine(LOG_SUCCESS, L"server is ready, listening at %d", Port);

	u_long iMode=1;
	ioctlsocket(m_Server,FIONBIO,&iMode);
}

CTCPSocketListner::~CTCPSocketListner()
{
	closesocket(m_Server);
	closesocket(m_Socket);

	if(m_bIPv6)
		delete ((sockaddr_in6*)m_sa);
	else
		delete ((sockaddr_in*)m_sa);
}

void CTCPSocketListner::Process()
{
	for(;;) // repeat untill all pending connections are accepted
	{
		sockaddr_in6 sa; // sockaddr_in is smaller
		int sa_len = sizeof(sa);
		SOCKET Client = accept(m_Server, (sockaddr*)&sa, &sa_len);
		if (INVALID_SOCKET == Client)
			break;

		CSafeAddress Address((sockaddr*)&sa, sa_len, sa_len == sizeof(sockaddr_in) ? CSafeAddress::eTCP_IP4 : CSafeAddress::eTCP_IP6);
		CSmartSocket* pSocket = GetParent<CSmartSocket>();
		pSocket->AddSessions(Address, new CTCPSocketSession(this, Client, Address));
	}

	const uint64 Size = 0xFFFF;
	char Buffer[Size];
	for(;;) // repeat untill all data is read
	{
		sockaddr_in6 sa; // sockaddr_in is smaller
		int sa_len = sizeof(sa); 
		int Recived = recvfrom(m_Socket, Buffer, Size, 0, (sockaddr*)&sa, &sa_len);
		if(Recived == 0)
			break;
		if (SOCKET_ERROR == Recived)
		{
			uint32 error = GetLastError();
			if (error != WSAEWOULDBLOCK)
				LogLine(LOG_ERROR, L"recvfrom: %d", WSAGetLastError());
			break;
		}
		CBuffer Packet(Buffer, Recived, true);

		CSafeAddress Address((sockaddr*)&sa, sa_len, sa_len == sizeof(sockaddr_in) ? CSafeAddress::eTCP_IP4 : CSafeAddress::eTCP_IP6);
		ReceiveFrom(Packet, Address);
	}
}

bool CTCPSocketListner::SendTo(const CBuffer& Packet, const CSafeAddress& Address)
{
	if(Packet.GetSize() > 0xFFFF - (60 + 20)) // Max UDP Datagram Size
	{
		ASSERT(0);
		return false;
	}

	sockaddr_in6 sa; // sockaddr_in is smaller
	int sa_len = sizeof(sa);
	Address.ToSA((sockaddr*)&sa, &sa_len);
	int Sent = sendto(m_Socket, (char*)Packet.GetBuffer(), Packet.GetSize(), 0, (sockaddr*)&sa, sa_len);
	if (SOCKET_ERROR == Sent)
	{
		uint32 error = GetLastError();
		if (error != WSAEWOULDBLOCK)
			LogLine(LOG_ERROR, L"sendto: %d", WSAGetLastError());
		return false;
	}
	else if(Sent == 0)
		return false;
	return true;
}

CSocketSession* CTCPSocketListner::CreateSession(const CSafeAddress& Address, bool bRendevouz, bool bEmpty)
{
	if(bEmpty)
		return new CTCPSocketSession(this, -1, Address);

	if(bRendevouz)
		return NULL;

	sockaddr_in6 sa; // sockaddr_in is smaller
	int sa_len = sizeof(sa);
	Address.ToSA((sockaddr*)&sa, &sa_len);

	SOCKET Client = socket(m_bIPv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (SOCKET_ERROR == connect(Client, (sockaddr*)&sa, sa_len))
	{
		LogLine(LOG_ERROR, L"connect: %d", WSAGetLastError());
		return NULL;
	}

	CSmartSocket* pSocket = GetParent<CSmartSocket>();
	return new CTCPSocketSession(this, Client, Address);
}

///////////////////////////////////////////////////////////////////////////////////////
//

CTCPSocketSession::CTCPSocketSession(CSocketListner* pListener, SOCKET Client, const CSafeAddress& Address)
: CSocketSession(pListener, Address)
{
	m_Client = Client;

	u_long iMode=1;
	ioctlsocket(m_Client,FIONBIO,&iMode);
}

CTCPSocketSession::~CTCPSocketSession()
{
	if(m_Client != -1)
		closesocket(m_Client);
}

void CTCPSocketSession::Process()
{
	for(;;) // repeat untill all data is read
	{
		const uint64 Size = KB2B(16);
		char Buffer[Size];
		int Recived = recv(m_Client, (char*)Buffer, Size, 0);
		if(Recived == 0)
			break;
		if (SOCKET_ERROR == Recived)
		{
			uint32 error = GetLastError();
			if (error != WSAEWOULDBLOCK)
				LogLine(LOG_ERROR, L"recv: %d", WSAGetLastError());
			break;
		}

		m_Receiving.SetData(-1, Buffer, Recived);

		m_LastActivity = GetCurTick();
	}

	CSocketSession::Process();

	for(;m_Sending.GetSize() > 0;) // repeat untill all data is sent
	{
		int Sent = send(m_Client, (char*)m_Sending.GetBuffer(), m_Sending.GetSize(), 0);
		if(Sent == 0)
			break;
		if (SOCKET_ERROR == Sent)
		{
			uint32 error = GetLastError();
			if (error != WSAEWOULDBLOCK)
				LogLine(LOG_ERROR, L"recv: %d", WSAGetLastError());
			break;
		}

		m_Sending.ShiftData(Sent);

		m_LastActivity = GetCurTick();
	}
}

void CTCPSocketSession::Swap(CSocketSession* pNew)
{
	ASSERT(m_Client == -1);

	m_Client = ((CTCPSocketSession*)pNew)->m_Client;
	m_LastActivity = GetCurTick();

	((CTCPSocketSession*)pNew)->m_Client = -1;
	delete pNew;
}
