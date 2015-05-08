#include "GlobalHeader.h"
#include "ListenSocket.h"
#include "StreamSocket.h"
#include "TCPSocket.h"
#include "UTPSocket.h"
#include "../NeoCore.h"

//int _CStreamSocket_pType = qRegisterMetaType<CStreamSocket*>("CStreamSocket*");
int _CStreamSocket_pType = qRegisterMetaType<CStreamSocket*>("CStreamSocket*", (CStreamSocket**)-1);
int _CAddress_Type = qRegisterMetaType<CAddress>("CAddress");

///////////////////////////////////

CStreamServer::CStreamServer()
{
	m_UpLimit = new CBandwidthLimit(this);
	m_DownLimit = new CBandwidthLimit(this);

	m_IPv4 = CAddress(CAddress::IPv4);
	m_IPv6 = CAddress(CAddress::IPv6);

	m_TCPPort = 0;
	m_UTPPort = 0;
	m_FallbackIncr = 0;

	//m_ServerV4 = NULL;
	m_ServerV6 = NULL;
	m_ServerV4uTP = NULL;
	m_ServerV6uTP = NULL;

	m_Counter = 0;

	m_bIPv6 = true;
#ifdef Q_OS_WIN32
	switch(QSysInfo::windowsVersion())
	{
		case QSysInfo::WV_2000:
		case QSysInfo::WV_XP:
			m_bIPv6 = false;
			break;
		case QSysInfo::WV_VISTA:
		case QSysInfo::WV_WINDOWS7:
			m_bIPv6 = true;
			break;
	}
#endif

	m_ServerV4 = new CTcpListener(this);
	connect(m_ServerV4, SIGNAL(Connection(CStreamSocket*)), this, SIGNAL(Connection(CStreamSocket*)));

	if(m_bIPv6)
	{
		m_ServerV6 = new CTcpListener(this);
		connect(m_ServerV6, SIGNAL(Connection(CStreamSocket*)), this, SIGNAL(Connection(CStreamSocket*)));
	}
}

CStreamServer::~CStreamServer()
{
	foreach(CStreamSocket* pSocket, m_Sockets.keys())
	{
		pSocket->Dispose();
		delete pSocket;
	}
}

void CStreamServer::SetPorts(quint16 TCPPort, quint16 UTPPort)
{
	m_TCPPort = TCPPort;
	m_UTPPort = UTPPort;
}

void CStreamServer::SetIPs(const CAddress& IPv4, const CAddress& IPv6)
{
	// Note if IP.Type() is CAddress::None than the socket is disabled
	m_bIPv6 = IPv6.Type() != CAddress::None && m_ServerV6 != NULL;

	QMutexLocker Locker(&m_Mutex);

	m_IPv4 = IPv4;
	m_IPv6 = IPv6;
}

void CStreamServer::UpdateSockets()
{
	QMetaObject::invokeMethod(this, "SetupSockets", Qt::BlockingQueuedConnection);
}

#ifdef MSS
void AddUDPOverhead(CStreamServer* pServer, uint64 uSize, CAddress::EAF eAF, CBandwidthLimit* Limit1, CBandwidthLimit* Limit2)
{
	uint64 uHeader = 8; // UDP Header
	uint64 FrameOH = (eAF == CAddress::IPv6 ? 40 : 20); // IPv6 / IPv4 Header

	// Note: UDP frames can be fragmented, so that the resulting overhead if Frames * IP Header + UD PHeader
	uint64 uMSS = MSS - FrameOH;
	int iFrames = ((uHeader + uSize) + (uMSS-1))/uMSS; // count if IP frames

	FrameOH += theCore->m_Network->GetFrameOverhead(); // Ethernet
	Limit1->CountBytes(uHeader + (iFrames * FrameOH), CBandwidthCounter::eHeader);
	Limit2->CountBytes(uHeader + (iFrames * FrameOH), CBandwidthCounter::eHeader);
}
#endif

void CStreamServer::CountUpUDP(int iAmount, CAddress::EAF eAF)
{
	CBandwidthLimit* UpLimit = ((CSocketThread*)thread())->GetUpLimit();
	m_UpLimit->CountBytes(iAmount, CBandwidthCounter::eProtocol);
	UpLimit->CountBytes(iAmount, CBandwidthCounter::eProtocol);
#ifdef MSS
	if(eAF)
		AddUDPOverhead(this, iAmount, eAF, m_UpLimit, UpLimit);
#endif
}

void CStreamServer::CountDownUDP(int iAmount, CAddress::EAF eAF)
{
	CBandwidthLimit* DownLimit = ((CSocketThread*)thread())->GetDownLimit();
	m_DownLimit->CountBytes(iAmount, CBandwidthCounter::eProtocol);
	DownLimit->CountBytes(iAmount, CBandwidthCounter::eProtocol);
#ifdef MSS
	if(eAF)
		AddUDPOverhead(this, iAmount, eAF, m_DownLimit, DownLimit);
#endif
}

void CStreamServer::SetupSockets()
{
	QMutexLocker Locker(&m_Mutex);

	///////////////////////////////
	// Close old sockets if open

	if(m_ServerV4->IsValid())
		m_ServerV4->Close();
	if(m_ServerV6 && m_ServerV6->IsValid())
		m_ServerV6->Close();

	if(m_ServerV4uTP && m_ServerV4uTP->IsValid())
		m_ServerV4uTP->Close();
	if(m_ServerV6uTP && m_ServerV6uTP->IsValid())
		m_ServerV6uTP->Close();


	///////////////////////////////
	// Open new sockets

	int iErrorCount = 0;

	if(m_TCPPort != 0 && m_IPv4.Type() != CAddress::None)
	{
		if(!m_ServerV4->Listen(m_TCPPort + m_FallbackIncr, m_IPv4))
		{
			LogLine(LOG_ERROR, tr("Can not listen on TCP Port %1").arg(m_TCPPort + m_FallbackIncr));
			m_TCPPort = 0;
			iErrorCount++;
		}
		else
			LogLine(LOG_NOTE, tr("Listen on TCP Port %1").arg(m_TCPPort + m_FallbackIncr));
	}
	if(m_TCPPort != 0 && m_IPv6.Type() != CAddress::None && m_ServerV6)
	{
		if(!m_ServerV6->Listen(m_TCPPort + m_FallbackIncr, m_IPv6))
		{
			LogLine(LOG_ERROR, tr("Can not listen on TCPv6 Port %1").arg(m_TCPPort + m_FallbackIncr));
			m_TCPPort = 0;
			iErrorCount++;
		}
		else
			LogLine(LOG_NOTE, tr("Listen on TCPv6 Port %1").arg(m_TCPPort + m_FallbackIncr));
	}

	if(m_UTPPort != 0 && m_IPv4.Type() != CAddress::None && m_ServerV4uTP)
	{
		if(!m_ServerV4uTP->Bind(m_UTPPort + m_FallbackIncr, m_IPv4))
		{
			LogLine(LOG_ERROR, tr("Can not listen on UDP Port %1").arg(m_UTPPort + m_FallbackIncr));
			m_UTPPort = 0;
			iErrorCount++;
		}
		else
			LogLine(LOG_NOTE, tr("Listen on UDP Port %1").arg(m_UTPPort + m_FallbackIncr));
	}
	if(m_UTPPort != 0 && m_IPv6.Type() != CAddress::None && m_ServerV6uTP)
	{
		if(!m_ServerV6uTP->Bind(m_UTPPort + m_FallbackIncr, m_IPv6))
		{
			LogLine(LOG_ERROR, tr("Can not listen on UDPv6 Port %1").arg(m_UTPPort + m_FallbackIncr));
			m_UTPPort = 0;
			iErrorCount++;
		}
		else
			LogLine(LOG_NOTE, tr("Listen on UDPv6 Port %1").arg(m_UTPPort + m_FallbackIncr));
	}

	if(iErrorCount)
		m_FallbackIncr += 1;
	else
		m_FallbackIncr = 0;
}

void CStreamServer::AddSocket(CStreamSocket* pSocket)
{
	pSocket->moveToThread(thread());
	QMutexLocker Locker(&m_Mutex);
	m_Sockets.insert(pSocket, 0);
}

void CStreamServer::RemoveSocket(CStreamSocket* pSocket)
{
	QMutexLocker Locker(&m_Mutex);
	m_Sockets.remove(pSocket);
}

void CStreamServer::Process()
{
	m_ServerV4->Process();

	if(m_ServerV6)
		m_ServerV6->Process();

	if(m_ServerV4uTP)
		m_ServerV4uTP->Process();

	if(m_ServerV6uTP)
		m_ServerV6uTP->Process();

	QMutexLocker Locker(&m_Mutex);

	m_Counter ++;
	if(m_Counter > 32)
		m_Counter = 0;

	for(QMap<CStreamSocket*, int>::iterator I = m_Sockets.begin(); I != m_Sockets.end(); ++I)
	{
		int &Counter = I.value();
		if(Counter && (m_Counter % Counter) != 0)
			continue; // socket was idle dont recheck it to often

		if(I.key()->Process())
			Counter = 0;
		else if(Counter == 0)
			Counter = 1;
		else if(Counter < 32)
			Counter <<= 1; // *= 2;
	}
}

void CStreamServer::FreeSocket(CStreamSocket* pSocket)
{
	ASSERT(pSocket);
	if(pSocket)
		QMetaObject::invokeMethod(this, "OnFreeSocket", Qt::BlockingQueuedConnection, Q_ARG(CStreamSocket*, pSocket));	
}

void CStreamServer::OnFreeSocket(CStreamSocket* pSocket)
{
	RemoveSocket(pSocket);
	pSocket->Dispose();
	pSocket->deleteLater();
}

void CStreamServer::AddAddress(const CAddress& Address)
{
	SAddrStat& AddrStat = m_Address[Address.Type()];

	AddrStat.Addresses.append(Address);
	AddrStat.AddressCount[Address]++;
	while(AddrStat.Addresses.size() > 5)
		AddrStat.AddressCount[AddrStat.Addresses.takeFirst()]--;
	
	int Count = AddrStat.AddressCount[Address];
	if(Count > AddrStat.Addresses.count()/2)
		AddrStat.MyAddress = Address;
}

CTcpListener* CStreamServer::GetTCPListener(const CAddress& Address)
{
	if(Address.Type() == CAddress::IPv4 && m_ServerV4->IsValid())
		return m_ServerV4;
	if(Address.Type() == CAddress::IPv6 && m_ServerV6->IsValid())
		return m_ServerV6;
	return NULL;
}

CUtpListener* CStreamServer::GetUTPListener(const CAddress& Address)
{
	if(Address.Type() == CAddress::IPv4 && m_ServerV4uTP->IsValid())
		return m_ServerV4uTP;
	if(Address.Type() == CAddress::IPv6 && m_ServerV4uTP->IsValid())
		return m_ServerV6uTP;
	return NULL;
}

void CStreamServer::Init(CStreamSocket* pSocket, SOCKET Sock)
{
	CTcpSocket* pTcpSocket = new CTcpSocket(pSocket);
	if(Sock != INVALID_SOCKET)
		pTcpSocket->SetSocket(Sock, true);
	pSocket->Init(pTcpSocket, this);
}

void CStreamServer::InitUTP(CStreamSocket* pSocket, struct UTPSocket* pSock)
{
	CUtpSocket* pUtpSocket = new CUtpSocket(pSocket);
	if(pSock)
		pUtpSocket->SetSocket(pSock, true);
	pSocket->Init(pUtpSocket, this);
}

CAddress CStreamServer::GetRandomAddress()
{
	QMutexLocker Locker(&m_Mutex);

	if(m_Sockets.isEmpty())
		return CAddress();
	return m_Sockets.keys().at(qrand()%m_Sockets.count())->GetAddress();
}