#pragma once
//#include "GlobalHeader.h"
#include "../../../../Framework/ObjectEx.h"
#include "../../../../Framework/Address.h"
#include "../../../../DHT/DHT.h" // generic peer deffinition

class CTrackerClient : public QObjectEx
{
    Q_OBJECT

public:
	CTrackerClient(const QUrl& Url, const QByteArray& InfoHash, QObject* qObject = NULL);
	static CTrackerClient*	New(const QUrl& Url, const QByteArray& InfoHash, QObject* qObject = NULL);

	virtual bool			IsBusy()			{return m_NextAnnounce == -1;}
	virtual time_t			NextAnnounce();
	virtual bool			IntervalPassed()	{return m_NextAnnounce < GetTime();}

	virtual bool			ChkStarted()		{return m_Started;}
	virtual bool			ChkCompleted()		{return m_Completed;}
	virtual bool			ChkStopped()		{return m_Stopped;}

	enum EEvent
	{
		eNone		= 0,
		eCompleted	= 1,
		eStarted	= 2,
		eStopped	= 3
	};
	virtual void			Announce(EEvent Event, uint64 uUploaded, uint64 uDownloaded, uint64 uLeft);
	virtual void			Announce(EEvent Event) = 0;

	virtual QString			GetUrl()			{return m_Url.toString();}

	virtual bool			HasError()			{return !m_Error.isEmpty();}
	virtual const QString&	GetError()			{return m_Error;}
	virtual void			ClearError();

	virtual void			SetupSocket(const CAddress& IPv4) {}

signals:
	void					PeersFound(QByteArray InfoHash, TPeerList PeerList);

protected:
	virtual void			SetError(const QString& Error);

	QUrl					m_Url;
	QByteArray				m_InfoHash;
	uint64					m_uUploaded;
	uint64					m_uDownloaded;
	uint64					m_uLeft;

	bool					m_Started;
	bool					m_Completed;
	bool					m_Stopped;

	time_t					m_NextAnnounce;
	uint32					m_AnnounceInterval;
	QString					m_Error;
};

//////////////////////////////////////////////////////////////////////////////////////////////
//

class CHttpTrackerClient : public CTrackerClient
{
    Q_OBJECT

public:
    CHttpTrackerClient(const QUrl& Url, const QByteArray& InfoHash, QObject* qObject = NULL);
	virtual ~CHttpTrackerClient();

	virtual void			Announce(EEvent Event);

	virtual bool			IsBusy()	{return m_pReply != NULL;}

public slots:
	void					OnFinished();

protected:

	QNetworkReply*			m_pReply;
};
