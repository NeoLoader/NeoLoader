#include "MiniUPnP.h"
#include "upnp-impl.h"
#include "natpmp-impl.h"

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

CMiniUPnP::CMiniUPnP(QObject* parent)
 : QThread(parent)
{
#ifdef WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    WSAStartup(wVersionRequested, &wsaData);
#endif

	m_Running = true;
	start();
}

CMiniUPnP::~CMiniUPnP()
{
	m_Running = false;
	wait();
}

void CMiniUPnP::run()
{
	while(m_Running)
	{
		msleep(333);

		closeOld();

		QMutexLocker Locker(&m_Mutex);
		foreach(const QString& Name, m_Ports.keys())
		{
			SPort* Port = m_Ports.value(Name);
			if(!Port)
				continue;

			if(Port->Countdown != 0)
			{
				Port->Countdown--;
				continue;
			}

			Locker.unlock();

			DoForwarding(Port);
			
			Locker.relock();
		}
	}

	// close ports on quit
	QMutexLocker Locker(&m_Mutex);
	foreach(QString Name, m_Ports.keys())
		m_OldPorts.append(m_Ports.take(Name));
	Locker.unlock();

	closeOld();
}

void CMiniUPnP::closeOld()
{
	QMutexLocker Locker(&m_Mutex);
	while(!m_OldPorts.isEmpty())
	{
		SPort* Port = m_OldPorts.takeFirst();

		Port->Enabled = false;

		Locker.unlock();
		
		DoForwarding(Port);

		upnpClose(Port->pUPnP);
		natpmpClose(Port->pNatPMP);

		Locker.relock();
	}
}

bool CMiniUPnP::StartForwarding(const QString& Name, int Port, const QString& ProtoStr)
{
	StopForwarding(Name);
	
	QMutexLocker Locker(&m_Mutex);

	int Proto = 0;
	if(ProtoStr.compare("TCP", Qt::CaseInsensitive) == 0)
		Proto = PORT_TCP;
	else if(ProtoStr.compare("UDP", Qt::CaseInsensitive) == 0)
		Proto = PORT_UDP;
	else
		return false;

	SPort* &pPort = m_Ports[Name];
	Q_ASSERT(!pPort);
	pPort = new SPort;

	pPort->Name = Name;
	pPort->Port = Port;
	pPort->Proto = Proto;
	pPort->Enabled = true;
	pPort->Check = false;

	pPort->Countdown = 0;
	pPort->Status = 0;

	pPort->pUPnP = upnpInit();

	pPort->pNatPMP = natpmpInit();

	return true;
}

void CMiniUPnP::StopForwarding(const QString& Name)
{
	QMutexLocker Locker(&m_Mutex);

	if(m_Ports.contains(Name))
		m_OldPorts.append(m_Ports.take(Name));
}

void CMiniUPnP::DoForwarding(SPort* pPort)
{
	int UPnPStatus = upnpPulse(pPort->pUPnP, pPort->Port, pPort->Proto, pPort->Enabled, pPort->Enabled && pPort->Check, pPort->Name.toStdString().c_str());

	int NatPMPStatus = natpmpPulse(pPort->pNatPMP, pPort->Port, pPort->Proto, pPort->Enabled);

	pPort->Status = UPnPStatus > NatPMPStatus ? UPnPStatus : NatPMPStatus;

	pPort->Check = (pPort->Status == PORT_MAPPED);
	if(pPort->Status == PORT_ERROR)
		pPort->Countdown = 60*333/1000;
}

int CMiniUPnP::GetStaus(const QString& Name, int* pPort, QString* pProtoStr)
{
	QMutexLocker Locker(&m_Mutex);

	SPort* Port = m_Ports.value(Name);
	if(!Port)
		return -1;

	if(pPort)
		*pPort = Port->Port;
	if(pProtoStr)
	{
		switch(Port->Proto)
		{
		case PORT_TCP: *pProtoStr = "TCP"; break;
		case PORT_UDP: *pProtoStr = "UDP"; break;
		}
	}

	return Port->Status;
}