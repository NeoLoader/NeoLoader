#pragma once
//#include "GlobalHeader.h"

#include "../../Framework/ObjectEx.h"
#include "../../Framework/Address.h"
#include "../../Framework/MT/ThreadEx.h"
#include "../../Framework/MT/ThreadLock.h"
#include "../FileTransfer/Transfer.h"

class CStreamServer;
class CSocketWorker;
class CBandwidthManager;
class CBandwidthLimit;
class CBandwidthCounter;

#define TICKS_PER_SEC 100

class CSocketThread: public QThread
{
	Q_OBJECT

public:
	CSocketThread(QObject* qObject = NULL);
	~CSocketThread();

	void						UpdateIPs(const QString& NICName, bool bIPv6 = true);
	void						UpdateConfig();

	__inline bool				UseTransportLimiting()			{return m_TransportLimiting;}	
	__inline int				GetFrameOverhead()				{return m_FrameOverhead;}

	const CAddress&				GetIPv4() const					{return m_IPv4;}
	const CAddress&				GetIPv6() const					{return m_IPv6;}
	const CAddress&				GetAddress(CAddress::EAF eAF)	{return m_Address[eAF];}

	void						StartThread();

	void						run();

	void						AddServer(CStreamServer* pServer);

	bool						IsConnectable()		{return m_Connectable;}
	bool						CanConnect();
	void						CountConnect();

	int							GetCount()			{return m_OpenSockets;}
	int							GetLimit()			{return m_MaxConnections;}

	CBandwidthManager*			GetUpManager()		{return m_UpManager;}
	CBandwidthManager*			GetDownManager()	{return m_DownManager;}

	CBandwidthLimit*			GetUpLimit()		{return m_UpLimit;}
	CBandwidthLimit*			GetDownLimit()		{return m_DownLimit;}

	QList<QHostAddress>			GetAddressSample(int Count);

	const STransferStats&		GetStats()			{return m_TransferStats;}

#ifdef NAFC
	CBandwidthCounter*			GetNafcUp()			{return m_NafcUp;}
	CBandwidthCounter*			GetNafcDown()		{return m_NafcDown;}
#endif

public slots:
	void 						OnBytesWritten(qint64 Bytes)		{m_TransferStats.AddUpload(Bytes);}
	void 						OnBytesReceived(qint64 Bytes)		{m_TransferStats.AddDownload(Bytes);}

protected:
	friend class CSocketWorker;
	void						Process();

	CSocketWorker*				m_Worker;
	CThreadLock					m_Lock;

	QMutex						m_Mutex;
	QList<CStreamServer*>		m_Servers;

	CBandwidthManager*			m_UpManager;
	CBandwidthManager*			m_DownManager;

	// GlobalLimit
	CBandwidthLimit*			m_UpLimit;
	CBandwidthLimit*			m_DownLimit;

	CAddress					m_IPv4;
	CAddress					m_IPv6;
	bool						m_Connectable;
	QMap<CAddress::EAF,CAddress>m_Address;

	volatile int				m_OpenSockets;
	volatile int				m_IntervalCounter;

	STransferStats				m_TransferStats;

	volatile bool				m_TransportLimiting;
	volatile int				m_FrameOverhead;
	volatile int				m_MaxNewPer5Sec;
	volatile int				m_MaxConnections;

#ifdef NAFC
	bool						ReadNafc();
	uint32						m_NafcIndex;
	uint32						m_NafcIn;
	uint32						m_NafcOut;
	CBandwidthCounter*			m_NafcUp;
	CBandwidthCounter*			m_NafcDown;
	uint64						m_uLastTick;
#endif
};

class CSocketWorker: public QObject
{
	Q_OBJECT

public:
	CSocketWorker(){
		m_uTimerID = startTimer(1000/TICKS_PER_SEC);
	}
	~CSocketWorker(){
		killTimer(m_uTimerID);
	}

protected:
	void						timerEvent(QTimerEvent* pEvent)	{
		if(pEvent->timerId() == m_uTimerID)
			((CSocketThread*)thread())->Process();
	}

	int							m_uTimerID;
};
