#pragma once

#include "../SocketSession.h"

class CTCPSocketListner: public CSocketListner
{
public:
	CTCPSocketListner(CSmartSocket* pSocket, bool bIPv6, uint16 Port);
	~CTCPSocketListner();

	virtual void					Process();

	virtual	CSocketSession*			CreateSession(const CSafeAddress& Address, bool bRendevouz = false, bool bEmpty = false);

	virtual CSafeAddress::EProtocol		GetProtocol()	{return m_bIPv6 ? CSafeAddress::eTCP_IP6 : CSafeAddress::eTCP_IP4;}

protected:
	virtual	bool					SendTo(const CBuffer& Packet, const CSafeAddress& Address);

	bool							m_bIPv6;
	uint16							m_Port;
	SOCKET							m_Server;
	SOCKET							m_Socket;
	struct sockaddr*				m_sa;
};

///////////////////////////////////////////////////////////////////////////////////////
//

class CTCPSocketSession: public CSocketSession
{
public:
	CTCPSocketSession(CSocketListner* pListener, SOCKET Client, const CSafeAddress& Address);
	~CTCPSocketSession();
	
	virtual void					Process();

	virtual bool					IsValid()		{return m_Client != -1;}
	virtual void					Swap(CSocketSession* pNew);

protected:
	SOCKET							m_Client;
};
