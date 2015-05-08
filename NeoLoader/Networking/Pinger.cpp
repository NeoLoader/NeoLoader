#include "GlobalHeader.h"
#include "../NeoCore.h"
#include "Pinger.h"
#include "SocketThread.h"
#include "../../Framework/Maths.h"


CPinger::CPinger(QObject* parent)
: QObject(parent)
{
	m_pFinder = NULL;

	m_pPinger = new QtPing(this);
	connect(m_pPinger, SIGNAL(PingResult(QtPingStatus)), this, SLOT(OnPingResult(QtPingStatus)));
	m_iFailCount = 0;
	m_PingTTL = 0;
	
	m_NeedLowest = 0;
	m_LowestPing = -1;
	m_AveragePing = 0;

	m_UploadSpeed = -1;
	m_CurrentUpload = 0;

	//m_DownloadSpeed = -1;
	//m_CurrentDownload = 0;

	m_Enabled = false;
}

CPinger::~CPinger()
{
}

void CPinger::SetEnabled(bool bSet)
{
	if(m_Enabled == bSet)
		return;

	m_Enabled = bSet;
	if(!m_Enabled)
	{
		if(m_pFinder)
		{
			m_pFinder->cancel();
			m_pFinder = NULL;
		}
		if(m_pPinger->IsPinging())
			m_pPinger->Stop();
	}
}

void CPinger::Process(UINT Tick)
{
	if(!m_Enabled)
		return;

	if(!IsPinging() && !IsFinding())
	{
		QString Host = theCore->Cfg()->GetString("Pinger/Host");
		if(Host.isEmpty())
			StartFinder();
		else
		{
			QHostAddress Address(Host);
			if(Address.isNull())
				QHostInfo::lookupHost(Host, this, SLOT(OnHostInfo(const QHostInfo&)));
			else
			{
				LogLine(LOG_DEBUG, tr("Pinger: using custom host: %1").arg(Address.toString()));
				StartPinger(Address);
			}
		}
	}
}

void CPinger::OnHostInfo(const QHostInfo& HostInfo)
{
	
	QList<QHostAddress> Addresses = HostInfo.addresses();
	if(!Addresses.isEmpty())
	{
		LogLine(LOG_DEBUG, tr("Pinger: using custom host: %1 (%2)").arg(HostInfo.hostName()).arg(Addresses.first().toString()));
		StartPinger(Addresses.first());
	}
	else
	{
		LogLine(LOG_DEBUG, tr("Pinger: custom host invalid, trying to find common router"));
		StartFinder();
	}
}

void CPinger::StartFinder()
{
	QList<QHostAddress> Addresses = theCore->m_Network->GetAddressSample(theCore->Cfg()->GetInt("Pinger/HostsToTrace"));
	if(Addresses.count() * 100 > theCore->Cfg()->GetInt("Pinger/HostsToTrace") * 25)
	{
		//qDebug() << "Got Adresses to trace:";
		//qDebug() << "----------------------------";
		//foreach (const QHostAddress& Address, Addresses)
		//	qDebug() << "IP:" << Address.toString();

		FindCommonRouter(Addresses);
	}
}

void CPinger::OnPingResult(QtPingStatus Status)
{
	uint32 CurrentPing = 0;

	bool bChanged; // did the topology changed?
	if((bChanged = m_PingAddress != Status.address) || !Status.success)
	{
		if(bChanged || m_iFailCount++ > theCore->Cfg()->GetInt("Pinger/MaxFails"))
		{
			m_pPinger->Stop();
			return;
		}
		CurrentPing = -1;
	}
	else
	{
		CurrentPing = Status.delay;
		m_iFailCount = 0;
	}

	ASSERT(CurrentPing);
	//qDebug() "Pinger: Response from" << Status.address.toString() << " delay " << CurrentPing;

	if(m_NeedLowest > 0)
	{
		m_NeedLowest--;

		if(CurrentPing < m_LowestPing)
			m_LowestPing = CurrentPing;

		if(m_NeedLowest == 0)
		{
			LogLine(LOG_DEBUG, tr("found lowest Ping: %1ms").arg(m_LowestPing));
			m_StartTime = GetCurTick();
		}

		return;
	}

	uint32 LowestPingAllowed = 20;
	if(CurrentPing < m_LowestPing && m_LowestPing > LowestPingAllowed) 
	{
		LogLine(LOG_DEBUG, tr("found new lowest Ping: %1ms").arg(Max(CurrentPing,LowestPingAllowed)));
		m_LowestPing = Max(CurrentPing, LowestPingAllowed);
	}

	sint64 PingTolerance;
	QString Tolerance = theCore->Cfg()->GetString("Pinger/ToleranceVal");
	if(Tolerance.right(1) == "%")
	{
		Tolerance.truncate(Tolerance.length() - 1);
		PingTolerance = m_LowestPing * (100 + Tolerance.trimmed().toInt()) / 100;
	}
	else if(Tolerance.right(2) == "ms")
	{
		Tolerance.truncate(Tolerance.length() - 2);
		PingTolerance = Tolerance.trimmed().toUInt(); 
	}
	else // NeoMules Tolerance mode
	{
		float Multi = 4.0f / log10f(8 + m_LowestPing);
		int ToleranceLevel = Tolerance.trimmed().toUInt(); 
		float BestPingUp = m_LowestPing * 13.65f / (log10f(m_LowestPing + 3)*log10f(m_LowestPing + 4));
		if (ToleranceLevel < 12)
			PingTolerance = BestPingUp - ((BestPingUp - (BestPingUp/Multi)) * (12.0f - ToleranceLevel)/11.0f);
		else if (ToleranceLevel > 12)
			PingTolerance = BestPingUp + (((BestPingUp*Multi) - BestPingUp) * (ToleranceLevel - 12.0f)/12.0f);
		else
			PingTolerance = BestPingUp;
		PingTolerance = PingTolerance/8;
	}

	if(CurrentPing == -1)
		CurrentPing = m_LowestPing + PingTolerance;

	m_PingAverage.append(CurrentPing);
	while(m_PingAverage.count() > theCore->Cfg()->GetInt("Pinger/Average") && m_PingAverage.size() > 1)
        m_PingAverage.remove(0);

	uint64 Summ = 0;
	uint64 Count = m_PingAverage.count();
	for (int i=0; i < Count; i++)
		Summ += m_PingAverage[i]; // * (i + 1);
	m_AveragePing = Summ / Count; //((Count * (Count - 1)) / 2)
	//m_AveragePing = Median(m_PingAverage.toStdVector());

	int NormalizedPing = m_AveragePing - m_LowestPing;
	
	AdjustSpeed(PingTolerance, NormalizedPing, m_UploadSpeed, m_CurrentUpload, KB2B(10));
	//AdjustSpeed(PingTolerance, NormalizedPing, m_DownloadSpeed, m_CurrentDownload, KB2B(40));
}

void CPinger::AdjustSpeed(sint64 PingTolerance, sint64 NormalizedPing, int& SetSpeed, int CurSpeed, int Mod)
{
	int UpDivider = theCore->Cfg()->GetUInt("Pinger/UpDivider");
	int DownDivider = theCore->Cfg()->GetUInt("Pinger/DownDivider");
	
	sint64 DividerMod = m_LowestPing * 10;
	if(DividerMod == 0)
		DividerMod = 10;

	uint64 uDuration = GetCurTick() - m_StartTime;
	if(uDuration < SEC2MS(20)) 
		DividerMod = DividerMod * 10 / 100;
	else if(uDuration < SEC2MS(30)) 
		DividerMod = DividerMod * 25 / 100;
	else if(uDuration < SEC2MS(40)) 
		DividerMod = DividerMod * 50 / 100;
	else if(uDuration < SEC2MS(60)) 
		DividerMod = DividerMod * 75 / 100;

	if(DownDivider == 0)
		DownDivider = 1;
	if(UpDivider == 0)
		UpDivider = 1;

	sint64 PingDeviation = PingTolerance - NormalizedPing;
	if(PingDeviation < 0) 
	{
		// slow down
		sint64 ulDiff = PingDeviation * Mod * 10 / (DownDivider * DividerMod);
		if(SetSpeed > -ulDiff)
			SetSpeed += ulDiff;
		else
			SetSpeed = 0;
	} 
	else if(PingDeviation > 0) 
	{
		if(CurSpeed + Mod * 3 > SetSpeed) 
		{
			// speed up
			uint64 ulDiff = PingDeviation * Mod * 10 / (UpDivider * DividerMod);
			if(GB2B(1) - SetSpeed > ulDiff)
				SetSpeed += ulDiff;
			else
				SetSpeed = GB2B(1);
		} 
	}
	
	int MaxSpeed = theCore->Cfg()->GetInt("Pinger/MaxSpeed");
	if(MaxSpeed > 0 && SetSpeed > MaxSpeed)
		SetSpeed = MaxSpeed;

	int MinSpeed = theCore->Cfg()->GetInt("Pinger/MinSpeed");
	if(SetSpeed < MinSpeed)
		SetSpeed = MinSpeed;
}

QList<QHostAddress> Trace(const QHostAddress& Address) {return QtPing().Trace(Address, 10, 1000);}

void CPinger::FindCommonRouter(const QList<QHostAddress>& Addresses)
{
	LogLine(LOG_DEBUG, tr("Pinger: starting trace route"));

	ASSERT(m_pFinder == NULL);
	m_pFinder = new QFutureWatcher<QList<QHostAddress> >(this);
	connect(m_pFinder, SIGNAL(resultReadyAt(int)), this, SLOT(OnRouteFound(int)));
	connect(m_pFinder, SIGNAL(finished()), this, SLOT(OnRoutesFound()));
	m_pFinder->setFuture(QtConcurrent::mapped(Addresses, Trace));
}

void CPinger::OnRouteFound(int Index)
{
	if(m_pFinder->progressValue() * 100 > m_pFinder->progressMaximum() * 75) // if we have successfully trace routed 75% of the addresses, we schedule an abbort
		QTimer::singleShot(500, this, SLOT(OnRoutesTick()));
}

void CPinger::OnRoutesFound()
{
	QFutureWatcher<QList<QHostAddress> >* pFinder = (QFutureWatcher<QList<QHostAddress> >*)sender();

	if(m_pFinder == pFinder)
		FindCommonRouter();

	pFinder->deleteLater();
}

void CPinger::OnRoutesTick()
{
	if(m_pFinder)
	{
		m_pFinder->cancel();

		FindCommonRouter();
	}
}

void CPinger::FindCommonRouter()
{
	QList<QList<QHostAddress> > Routes;
	for(int i=0; i < m_pFinder->future().resultCount(); i++)
	{
		//qDebug() << "";
		//qDebug() << "Tracing: " << Address;
		//qDebug() << "------------------------";
		
		QList<QHostAddress> Route = m_pFinder->future().resultAt(i);

		//foreach(const QHostAddress& Address, Route)
		//	qDebug() << "IP: " << Address.toString();

		if(Route.count() >= 2)
			Routes.append(Route);
	}

	m_pFinder = NULL;

	bool Success = true;
	QList<QHostAddress> CommonRoute;
	int CommonTTL = 0;
	for(; Success && CommonTTL <= 64; CommonTTL++)
	{
		QHostAddress CurRouter;
		bool Found = false;
		foreach(const QList<QHostAddress>& Route, Routes)
		{
			if(CommonTTL >= Route.count())
			{
				Success = false;
				break;
			}

			const QHostAddress& Address = Route.at(CommonTTL);
			if(Address.isNull())
			{
				CurRouter.clear();
				break;
			}

			if(CurRouter.isNull())
				CurRouter = Address;
			else if(CurRouter != Address)
			{
				Found = true;
				break;
			}
		}
		if(Found)
			break;
		else if(!CurRouter.isNull())
			CommonRoute.append(CurRouter);
	}
	if(CommonRoute.isEmpty())
		Success = false;

	//qDebug() << "Common Route:";
	//qDebug() << "------------------------";
	//foreach(const QHostAddress& Address, CommonRoute)
	//	qDebug() << "IP: " << Address.toString();
	
	if(Success)
	{
		LogLine(LOG_DEBUG, tr("Pinger: found router: %1").arg(CommonRoute.last().toString()));
		StartPinger(CommonRoute.last(), CommonTTL);
	}
	else
		LogLine(LOG_DEBUG, tr("Pinger: Couldnt find common router to ping"));
}

void CPinger::StartPinger(const QHostAddress& Address, int TTL)
{
	if(m_pPinger->IsPinging())
	{
		ASSERT(0);
		m_pPinger->Stop();
	}

	m_PingAddress = Address;
	m_PingTTL = TTL;

	m_NeedLowest = theCore->Cfg()->GetInt("Pinger/InitialPings");
	m_LowestPing = -1;
	m_AveragePing = 0;
	m_StartTime = 0;

	m_UploadSpeed = theCore->Cfg()->GetInt("Pinger/MinSpeed");
	//m_DownloadSpeed = theCore->Cfg()->GetInt("Pinger/MinSpeed");

	m_pPinger->Start(m_PingAddress, m_PingTTL);
}
