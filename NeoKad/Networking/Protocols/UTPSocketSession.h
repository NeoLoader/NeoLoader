#pragma once

#include "../SocketSession.h"

#ifndef WIN32
	// since Win32 also defines these, we create definitions for these on other platforms
   #define closesocket close
   #define WSAGetLastError() errno
   #define SOCKET int
   #define INVALID_SOCKET -1

   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
   #include <sys/socket.h>
   #include <arpa/inet.h>
   #include <fcntl.h>
#ifdef __APPLE__
    #include "errno.h"
#endif
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif

class CUTPSocketListner: public CSocketListner
{
public:
	CUTPSocketListner(CSmartSocket* pSocket);
	~CUTPSocketListner();

	virtual void					Close();
	virtual bool					Bind(uint16 Port, const CAddress& IP);

	virtual void					Process();

	virtual	CSocketSession*			CreateSession(const CSafeAddress& Address, bool bRendevouz = false, bool bEmpty = false);

	virtual CSafeAddress::EProtocol	GetProtocol()	{return m_Port == 0 ? CSafeAddress::eInvalid : (m_bIPv6 ? CSafeAddress::eUTP_IP6 : CSafeAddress::eUTP_IP4);}

protected:
	void							Recv(const byte* Buff, size_t uSize, const struct sockaddr* sa, socklen_t sa_len);

	virtual	bool					SendTo(const CBuffer& Packet, const CSafeAddress& Address);

	friend void got_incoming_connection(void *userdata, struct UTPSocket *socket);
	friend void send_to(void *userdata, const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen);
	void							Send(const byte* Buff, size_t uSize, const struct sockaddr* sa, socklen_t sa_len, uint8 Type);

	bool							m_bIPv6;
	uint16							m_Port;
	SOCKET							m_Socket;

	struct SPassKey
	{
		SPassKey(struct sockaddr* s)
		{
			if(s->sa_family == AF_INET6)
			{
				sa = (struct sockaddr*)new sockaddr_in6;
				memcpy(sa, s, sizeof(sockaddr_in6));
			}
			else if(s->sa_family == AF_INET)
			{
				sa = (struct sockaddr*)new sockaddr_in;
				memcpy(sa, s, sizeof(sockaddr_in));
			}
			else{
				ASSERT(0);
			}

			PassKey = 0;
			//bAck = false;
			LastActivity = 0;
		}
		~SPassKey() 
		{
			if(sa->sa_family == AF_INET6)
				delete ((sockaddr_in6*)sa);
			else if(sa->sa_family == AF_INET)
				delete ((sockaddr_in*)sa);
		}

		struct sockaddr*			sa;

		uint64						PassKey;
		//bool						bAck;
		uint64						LastActivity;
	};

	struct CmpSA // Less
	{
		bool operator()(struct sockaddr* l, struct sockaddr* r) const
		{
			if(l->sa_family != r->sa_family)
				return l->sa_family < r->sa_family;
			if(l->sa_family == AF_INET6)
			{
				if(int cmp = memcmp(&((sockaddr_in6*)l)->sin6_addr, &((sockaddr_in6*)r)->sin6_addr, sizeof(((sockaddr_in6*)l)->sin6_addr)))
					return cmp < 0;
				return ((sockaddr_in6*)l)->sin6_port < ((sockaddr_in6*)r)->sin6_port;
			}
			else if(l->sa_family == AF_INET)
			{
				if(int cmp = memcmp(&((sockaddr_in*)l)->sin_addr, &((sockaddr_in*)r)->sin_addr, sizeof(((sockaddr_in*)l)->sin_addr)))
					return cmp < 0;
				return ((sockaddr_in*)l)->sin_port < ((sockaddr_in*)r)->sin_port;
			}
			ASSERT(0);
			return false;
		}
	};

	typedef map<struct sockaddr*, SPassKey*, CmpSA> TKeyMap;
	TKeyMap							m_SendKeys;
	uint64							m_RecvKey;
	uint64							m_NextCleanUp;
#define CLEANUP_INTERVAL SEC2MS(100)
};

///////////////////////////////////////////////////////////////////////////////////////
//

class CUTPSocketSession: public CSocketSession
{
public:
	CUTPSocketSession(CSocketListner* pListener, struct UTPSocket* Socket, const CSafeAddress& Address);
	~CUTPSocketSession();
	
	virtual void					Process();

	virtual bool					IsValid() const		{return m_Socket != NULL;}
	virtual bool					IsConnected() const;
	virtual bool					IsDisconnected() const;
	virtual bool					IsBussy() const;
	virtual void					Close();
	virtual void					Swap(CSocketSession* pNew);

protected:
	friend void utp_error(void* userdata, int errcode);
	friend void utp_state(void* userdata, int state);
	friend void utp_read(void* userdata, const byte* bytes, size_t count);
	friend void utp_write(void* userdata, byte* bytes, size_t count);
	friend size_t utp_get_rb_size(void* userdata);
	friend class CUTPSocketListner;

	void							Connect(const struct sockaddr *sa, socklen_t sa_len);
	void							PushData(const byte* Buffer, size_t Recived);
	void							PullData(byte* Buff, size_t uSize);

	int								m_State;
	struct UTPSocket*				m_Socket;
};
