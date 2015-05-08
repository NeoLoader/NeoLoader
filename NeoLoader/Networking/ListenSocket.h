#pragma once
//#include "GlobalHeader.h"

#include "../../Framework/ObjectEx.h"
#include "../../Framework/Address.h"
#include "./BandwidthControl/BandwidthLimit.h"

class CStreamSocket;
class CTcpListener;
class CUtpListener;

#ifndef WIN32
   #define SOCKET int
   #define INVALID_SOCKET (-1)
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif

class CStreamServer: public QObject
{
	Q_OBJECT

public:
	CStreamServer();
	virtual ~CStreamServer();

	virtual	void				SetPorts(quint16 TCPPort, quint16 UTPPort);
	virtual	void				SetIPs(const CAddress& IPv4 = CAddress(CAddress::IPv4), const CAddress& IPv6 = CAddress(CAddress::IPv6));

	virtual	void				Process();

	virtual bool				IsExpected(const CAddress& Address)		{return false;}

	virtual	CStreamSocket*		AllocSocket(SOCKET Sock) = 0;
	virtual	CStreamSocket*		AllocUTPSocket(struct UTPSocket* pSock) = 0;
	virtual void				FreeSocket(CStreamSocket* pSocket);

	virtual	void				AddSocket(CStreamSocket* pSocket);
	virtual	void				RemoveSocket(CStreamSocket* pSocket);
	virtual	int					GetSocketCount()						{QMutexLocker Locker(&m_Mutex); return m_Sockets.size();}
	virtual	quint16				GetPort()								{return m_TCPPort;}
	virtual quint16				GetUTPPort()							{return m_UTPPort;}
	virtual bool				HasIPv6()								{return m_bIPv6;}

	virtual	CAddress			GetIPv4()								{QMutexLocker Locker(&m_Mutex); return m_IPv4;}
	virtual	CAddress			GetIPv6()								{QMutexLocker Locker(&m_Mutex); return m_IPv6;}
	virtual bool				HasSocket()								{QMutexLocker Locker(&m_Mutex); return m_IPv4.Type() != CAddress::None || m_IPv6.Type() != CAddress::None;}

	virtual	CTcpListener*		GetTCPListener(const CAddress& Address);
	virtual	CUtpListener*		GetUTPListener(const CAddress& Address);

	virtual	void				UpdateSockets();

	CBandwidthLimit*			GetUpLimit()							{return m_UpLimit;}
	CBandwidthLimit*			GetDownLimit()							{return m_DownLimit;}

	virtual void				CountUpUDP(int iAmount, CAddress::EAF eAF);
	virtual void				CountDownUDP(int iAmount, CAddress::EAF eAF);

	// Note: use this functions only form one trhead
	virtual	void				AddAddress(const CAddress& Address);
	virtual	const CAddress&		GetAddress(CAddress::EAF eAF) {return m_Address[eAF].MyAddress;}

	CAddress					GetRandomAddress();

signals:
	void						Connection(CStreamSocket* pSocket);

public slots:
	virtual	void				SetupSockets();

private slots:
	virtual	void				OnFreeSocket(CStreamSocket* pSocket);

protected:

	virtual void				Init(CStreamSocket* pSocket, SOCKET Sock);
	virtual void				InitUTP(CStreamSocket* pSocket, struct UTPSocket* pSock);

	CTcpListener*				m_ServerV4;
	CTcpListener*				m_ServerV6;
	CUtpListener*				m_ServerV4uTP;
	CUtpListener*				m_ServerV6uTP;

	quint16						m_TCPPort;
	quint16						m_UTPPort;
	CAddress					m_IPv4;
	bool						m_bIPv6;
	CAddress					m_IPv6;
	quint16						m_FallbackIncr;

	QMap<CStreamSocket*, int>	m_Sockets;

	int							m_Counter;

	struct SAddrStat
	{
		QList<CAddress>			Addresses;
		QMap<CAddress,int>		AddressCount;
		CAddress				MyAddress;
	};
	QMap<CAddress::EAF,SAddrStat>m_Address;

	QMutex						m_Mutex;

	CBandwidthLimit*			m_UpLimit;
	CBandwidthLimit*			m_DownLimit;
};
