#pragma once

#include "../SocketSession.h"
#include "../../../udt/src/udt.h"

class CSmartSocket;
union sockaddr_u;

class CUDTSocketListner: public CSocketListner
{
public:
	CUDTSocketListner(CSmartSocket* pSocket, bool bIPv6, uint16 Port);
	~CUDTSocketListner();

	virtual void					Process();

	virtual	CSocketSession*			CreateSession(const CSafeAddress& Address, bool bRendevouz = false, bool bEmpty = false);

	virtual CSafeAddress::EProtocol	GetProtocol()	{return m_Port == 0 ? CSafeAddress::eInvalid : (m_bIPv6 ? CSafeAddress::eUDT_IP6 : CSafeAddress::eUDT_IP4);}

protected:
	virtual	bool					SendTo(const CBuffer& Packet, const CSafeAddress& Address);

	void							ConfigSocket(UDTSOCKET Client);

	bool							m_bIPv6;
	uint16							m_Port;
	UDTSOCKET						m_Server;
	struct sockaddr*				m_sa;

private:

	static int						m_Count;
};

///////////////////////////////////////////////////////////////////////////////////////
//

class CUDTSocketSession: public CSocketSession
{
public:
	CUDTSocketSession(CSocketListner* pListener, UDTSOCKET Client, const CSafeAddress& Address);
	~CUDTSocketSession();
	
	virtual void					Process();

	virtual bool					IsValid()		{return m_Client != UDT::INVALID_SOCK;}
	virtual bool					IsConnected();
	virtual void					Swap(CSocketSession* pNew);

protected:
	UDTSOCKET						m_Client;
};
