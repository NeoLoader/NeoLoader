#include "GlobalHeader.h"
#include "SocketThread.h"
#include "ListenSocket.h"
#include "StreamSocket.h"
#include "../NeoCore.h"
#include "BandwidthControl/BandwidthManager.h"
#include "BandwidthControl/BandwidthLimit.h"
#include "UTPSocket.h"
#include <QNetworkInterface>

int _QList_QHostAddress_pType = qRegisterMetaType<QList<QHostAddress> >("QList<QHostAddress>");

CSocketThread::CSocketThread(QObject* qObject)
 : QThread(qObject), m_Worker(NULL)
{
	m_OpenSockets = 0;
	m_IntervalCounter = 0;
	m_Connectable = false;
	m_TransportLimiting = false;
	m_FrameOverhead = 0;
	m_MaxNewPer5Sec = 0;
	m_MaxConnections = 0;

	m_TransferStats.UploadedTotal = theCore->Stats()->value("Bandwidth/Uploaded").toULongLong();
	m_TransferStats.DownloadedTotal = theCore->Stats()->value("Bandwidth/Downloaded").toULongLong();

#ifdef NAFC
	m_NafcIndex = 1;
	m_NafcIn = -1;
	m_NafcOut = -1;
	m_NafcDown = new CBandwidthCounter(this);
	m_NafcUp = new CBandwidthCounter(this);
	m_uLastTick = 0;
#endif
}

CSocketThread::~CSocketThread()
{
	quit();
	wait();
}

void CSocketThread::UpdateIPs(const QString& NICName, bool bIPv6)
{
	CAddress IPv4;
	CAddress IPv6;

	foreach(QNetworkInterface NIC, QNetworkInterface::allInterfaces())
	{
		if ((NIC.flags().testFlag(QNetworkInterface::IsLoopBack) && !NIC.flags().testFlag(QNetworkInterface::IsPointToPoint)))
			continue;
		if(!NIC.flags().testFlag(QNetworkInterface::IsUp))
			continue;

		if(!NICName.isEmpty() && NICName != NIC.humanReadableName())
			continue;
		
		foreach(const QNetworkAddressEntry& IP, NIC.addressEntries())
		{
			switch(IP.ip().protocol())
			{
				case QAbstractSocket::IPv4Protocol:
				{
					// We try to select a public IPv4 but if that fais we sattel for the first local we find.
					if(IPv4.IsNull() || (IsLanIPv4(IPv4.ToIPv4()) && !IsLanIPv4(IP.ip().toIPv4Address())))
						IPv4 = IP.ip().toIPv4Address();
					break;
				}
				case QAbstractSocket::IPv6Protocol:
				{
					if(bIPv6 && IPv6.IsNull())
					{
						Q_IPV6ADDR ip = IP.ip().toIPv6Address();
						if(ip.c[0] == 0xFE && ip.c[1] == 0x80)
							continue; // IP that is constructed from MAC address, and is only available to machines on the same switch.
						if(ip.c[11] == 0xFF && ip.c[12] == 0xFE)
							continue; // IP that is constructed from MAC address and ISP provided subnet prefix. This could be seen as your static IP.
						// IP that is constructed from ISP provided subnet prefix and some random digits. 
						// This IP changes every time you power on your PC, and is the IP newer versions of Windows uses as the preferred source IP.
						IPv6 = IP.ip().toIPv6Address().c;
					}
					break;
				}
			}
		}
	}

	m_Address[CAddress::IPv4] = IPv4;
	m_Address[CAddress::IPv6] = IPv6;

	if(!NICName.isEmpty()) // VPN LockIn
	{
		m_IPv4 = IPv4;
		m_IPv6 = IPv6;
	}
	else if(!(!m_IPv4.IsNull() && (m_IPv6 == !m_IPv6.IsNull())))
	{
		m_IPv4 = CAddress(CAddress::IPv4); // any Address
		m_IPv6 = bIPv6 ? CAddress(CAddress::IPv6) : CAddress(); // any Address
	}

	m_Connectable = (m_IPv4.Type() != CAddress::None || m_IPv6.Type() != CAddress::None);
}

void CSocketThread::UpdateConfig()
{
	m_TransportLimiting = theCore->Cfg()->GetBool("Bandwidth/TransportLimiting");
	m_FrameOverhead = theCore->Cfg()->GetInt("Bandwidth/FrameOverhead");
	m_MaxNewPer5Sec = theCore->Cfg()->GetInt("Bandwidth/MaxNewPer5Sec");
	m_MaxConnections = theCore->Cfg()->GetInt("Bandwidth/MaxConnections");
}

void CSocketThread::StartThread()
{
	start();
	m_Lock.Lock();
}

void CSocketThread::run()
{
	CUtpTimer* pTimer = new CUtpTimer();

	m_Worker = new CSocketWorker();

	m_UpManager = new CBandwidthManager(CBandwidthLimiter::eUpChannel);
	m_UpLimit = new CBandwidthLimit();
	m_DownManager = new CBandwidthManager(CBandwidthLimiter::eDownChannel);
	m_DownLimit = new CBandwidthLimit();

	m_Lock.Release();
	exec();

	delete m_Worker;

	QMutexLocker Locker (&m_Mutex);
	foreach(CStreamServer* pServer, m_Servers)
		delete pServer;

	delete m_UpLimit;
	delete m_UpManager;
	delete m_DownLimit;
	delete m_DownManager;

	delete pTimer;
}

void CSocketThread::Process()
{
	int OpenSockets = 0;
	QMutexLocker Locker (&m_Mutex);
	foreach(CStreamServer* pServer, m_Servers)
	{
		pServer->Process();
		OpenSockets += pServer->GetSocketCount();
	}
	m_OpenSockets = OpenSockets;

	if(m_IntervalCounter > 10)
		m_IntervalCounter -= 10;

#ifdef NAFC
	uint64 uCurTick = GetCurTick();
	uint64 uInterval = uCurTick - m_uLastTick;
	m_uLastTick = uCurTick;

	ASSERT(uInterval < INT_MAX);

	uint32 NafcIn = m_NafcIn;
	uint32 NafcOut = m_NafcOut;
	if(ReadNafc())
	{
		if(NafcIn < m_NafcIn) // skip overflows
			m_NafcDown->CountBytes(m_NafcIn - NafcIn, CBandwidthCounter::eAll);
		if(NafcOut < m_NafcOut)
			m_NafcUp->CountBytes(m_NafcOut - NafcOut, CBandwidthCounter::eAll);
	}
	m_NafcDown->Process((int)uInterval);
	m_NafcUp->Process((int)uInterval);
#endif
}

void CSocketThread::AddServer(CStreamServer* pServer)
{
	pServer->moveToThread(this);

	QMutexLocker Locker (&m_Mutex);
	m_Servers.append(pServer);
}

bool CSocketThread::CanConnect()
{
	if(!m_Connectable)
		return false;

	if(m_IntervalCounter >= 10 * (5 * TICKS_PER_SEC))
		return false;

	return m_OpenSockets < m_MaxConnections;
}

void CSocketThread::CountConnect()
{
	if(int MaxNew = m_MaxNewPer5Sec)
		m_IntervalCounter += 10 * (5 * TICKS_PER_SEC) / MaxNew; // if one would set more than 5000 new cons / sec this would result in no limt at all
}

QList<QHostAddress> CSocketThread::GetAddressSample(int Count)
{
	QSet<QHostAddress> AddressesV4;
	QSet<QHostAddress> AddressesV6;
	for(int i=0; i < Count*10; i++)
	{
		if(AddressesV4.count() >= Count)
			return AddressesV4.toList();
		foreach(CStreamServer* pServer, m_Servers)
		{
			CAddress Address = pServer->GetRandomAddress();
			if(Address.IsNull())
				continue;

			if(Address.Type() == CAddress::IPv4)
				AddressesV4.insert(QHostAddress(Address.ToIPv4()));
			else if(AddressesV6.count() < Count)
				AddressesV6.insert(QHostAddress((quint8*)Address.Data()));
		}
	}
	return AddressesV4.isEmpty() ? AddressesV6.toList() : QList<QHostAddress>();
}

#ifdef NAFC
#include <Iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Mpr.lib")

PMIB_IPADDRTABLE GetAddrTable(){
	// enumerate all interfaces
	PMIB_IPADDRTABLE pIPAddrTable;
	DWORD dwSize = 0;

	// get address table size
	if (GetIpAddrTable(NULL, &dwSize, 0) != ERROR_INSUFFICIENT_BUFFER)
		return NULL;

	// get address table
	pIPAddrTable = (PMIB_IPADDRTABLE)new char[dwSize];
	if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) != NO_ERROR) 
	{
		delete [] pIPAddrTable;
		return NULL;
	}

	return pIPAddrTable;
}

uint32 SetAdapterIndex(DWORD dwIP = 0)
{
	// Prepare adapter choice
	DWORD wdMask = 0xFFFFFFFF; // Exact IP
	if(dwIP == 0) 
	{
		// Retrieve the default used IP (=> in case of multiple adapters)
		char hostName[256];
		if(gethostname(hostName, sizeof(hostName)) != 0)
			return 0;

		hostent* lphost = gethostbyname(hostName);
		if(lphost == NULL)
			return 0;

		dwIP = ((LPIN_ADDR)lphost->h_addr)->s_addr;
	}

	uint32 Index = 0;
	PMIB_IPADDRTABLE pIPAddrTable = GetAddrTable();
	if(pIPAddrTable == NULL)
		return 0;

	// Pick the interface matching the IP/Zone
	DWORD dwAddr;
	for(DWORD i = 0; i < pIPAddrTable->dwNumEntries; i++)
	{
		dwAddr = pIPAddrTable->table[i].dwAddr;
		if((dwIP & wdMask) == (dwAddr & wdMask))
		{
			if(pIPAddrTable->table[i].dwIndex > 1)
			{
				/*MIB_IFROW ifRow;
				ifRow.dwIndex = m_currentAdapterIndex;
				if(GetIfEntry(&ifRow) != NO_ERROR)
					return 0;
				m_iMSS = ifRow.dwMtu - 40; // get the MSS = MTU - 40
				ifRow.bDescr[ifRow.dwDescrLen] = 0;
				CString(ifRow.bDescr)*/
				LogLine(LOG_INFO, _T("NAFC: Selected adapter..."));
			}
			Index = pIPAddrTable->table[i].dwIndex;
		}
	}

	delete pIPAddrTable;
	return Index;
}

bool CSocketThread::ReadNafc()
{
	// Retrieve the network flow
	if(m_NafcIndex > 1)
	{
		MIB_IFROW ifRow;
		ifRow.dwIndex = m_NafcIndex;
		if(GetIfEntry(&ifRow) == NO_ERROR)
		{
			m_NafcIn = ifRow.dwInOctets;
			m_NafcOut = ifRow.dwOutOctets;
			return true;
		}
		else
		{
			LogLine(LOG_WARNING, _T("NAFC: Faild to get Data from Adapter, index may be wrong, trying to get new index..."));

			m_NafcIndex = SetAdapterIndex();
			if(m_NafcIndex <= 1)
				LogLine(LOG_WARNING, _T("NAFC: Faild to get new Adapter index, internet connection may be down..."));
		}
	}
	else
		m_NafcIndex = SetAdapterIndex();
	return false;
}
#endif