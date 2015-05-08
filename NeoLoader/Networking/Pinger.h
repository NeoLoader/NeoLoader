#pragma once

#include <QFutureWatcher>
#include <QtConcurrent>
#include <QHostInfo>

#include "../../Framework/ObjectEx.h"
#include "../../Framework/Address.h"

#include "../../qtping/qtping.h"

class CPinger: public QObject
{
	Q_OBJECT

public:
	CPinger(QObject* parent = 0);
	virtual ~CPinger();

	void			Process(UINT Tick);

	bool			IsWorking()				{return m_NeedLowest == 0 && IsPinging();}
	bool			IsFinding()				{return m_pFinder != NULL;}
	bool			IsPinging()				{return m_pPinger->IsPinging();}

	bool			IsEnabled()				{return m_Enabled;}
	void			SetEnabled(bool bSet);

	int				GetUploadSpeed()		{return m_UploadSpeed;}
	void			SetUploadSpeed(int Speed) {m_CurrentUpload = Speed;}

	//int				GetDownloadSpeed()		{return m_DownloadSpeed;}
	//void			SetDownloadSpeed(int Speed) {m_CurrentDownload = Speed;}

	const QHostAddress& GetHost()			{return m_PingAddress;}
	int				GetTTL()				{return m_PingTTL;}

	uint32			GetAveragePing()		{return m_AveragePing;}
	uint32			GetLowestPing()			{return m_LowestPing != -1 ? m_LowestPing : 0;}

private slots:
	void			OnPingResult(QtPingStatus Status);
	void			OnRoutesFound();
	void			OnRouteFound(int Index);
	void			OnRoutesTick();
	void			OnHostInfo(const QHostInfo& HostInfo);

protected:
	void			StartFinder();
	void			FindCommonRouter(const QList<QHostAddress>& Addresses);
	void			FindCommonRouter();
	void			StartPinger(const QHostAddress& Address, int TTL = 64);

	void			AdjustSpeed(sint64 PingTolerance, sint64 NormalizedPing, int& SetSpeed, int CurSpeed, int Mod);

	QFutureWatcher<QList<QHostAddress> >* m_pFinder;

	QtPing*			m_pPinger;
	int				m_iFailCount;
	QHostAddress	m_PingAddress;
	int				m_PingTTL;

	int				m_NeedLowest;
	uint32			m_LowestPing;
	uint32			m_AveragePing;
	QVector<uint32> m_PingAverage;
	uint64			m_StartTime;

	int				m_UploadSpeed;
	int				m_CurrentUpload;

	//int				m_DownloadSpeed;
	//int				m_CurrentDownload;

	bool			m_Enabled;
};