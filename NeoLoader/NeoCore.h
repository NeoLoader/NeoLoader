#pragma once
//#include "GlobalHeader.h"

#include "../Framework/MT/ThreadEx.h"
#include "../Framework/Settings.h"

#ifdef CRAWLER
class CCrawlerManager;
class CFileArchiver;
#endif
#ifndef NO_HOSTERS
class CWebManager;
class CConnectorManager;
class CCrawlingManager;
class CLoginManager;
class CWebRepository;
class CArchiveDownloader;
class CArchiveUploader;
class CCaptchaManager;
#endif
class CHttpServer;
class CFileManager;
class CUploadManager;
class CDownloadManager;
class CKeywordSearchManager;
class CSearchManager;
class CFileGrabber;
class CTorrentManager;
class CMuleManager;
class CNeoManager;
class CNeoCore;
class CCoreServer;
class CInterfaceManager;
class CRequestManager;
class CHashingThread;
class QLogger;
class CSocketThread;
class CPinger;
class CIOManager;
class CPeerWatch;
class CMiniUPnP;
class CCaffeine;
class CCoreThread;


class CNeoCore: public CLoggerTmpl<QObjectEx>
{
	Q_OBJECT

public:
	CNeoCore(CSettings* Settings);
	~CNeoCore();

	void				Initialise();
	void				Process();
	void				Uninitialise();

	QString				GetIncomingDir(bool bCreate = true);
	QString				GetTempDir(bool bCreate = true);

	//bool				TestLogin(const QString &UserName, const QString &Password);
	bool				TestLogin(const QString &Password);
	QString				GetLoginToken();
	bool				CheckLogin(const QString &LoginToken);
	void				SaveLoginTokens();
	void				LoadLoginTokens();

	CSettings*			Cfg(bool bCore = true)		{return bCore ? m_CoreSettings : m_Settings;}
	QSettings*			Stats()						{return m_Stats;}

	CCoreServer*		m_Server;
	CInterfaceManager*	m_Interfaces;
	CFileManager*		m_FileManager;
	CDownloadManager*	m_DownloadManager;
	CUploadManager*		m_UploadManager;
	CHashingThread*		m_Hashing;
#ifdef CRAWLER
	CCrawlerManager*	m_CrawlerManager;
	CFileArchiver*		m_FileArchiver;
#endif
	CHttpServer*		m_HttpServer;
	CFileGrabber*		m_FileGrabber;
	CTorrentManager*	m_TorrentManager;
	CMuleManager*		m_MuleManager;
	CNeoManager*		m_NeoManager;
	CRequestManager*	m_RequestManager;
	CSettings*			m_CoreSettings;
	CSettings*			m_Settings;
	QSettings*			m_Stats;
	CSocketThread*		m_Network;
	CPinger*			m_Pinger;
	CIOManager*			m_IOManager;
	CPeerWatch*			m_PeerWatch;
	CMiniUPnP*			m_MiniUPnP;
	CCaffeine*			m_Caffeine;
	CSearchManager*		m_SearchManager;
#ifndef NO_HOSTERS
	CCrawlingManager*	m_CrawlingManager;
	CArchiveDownloader*	m_ArchiveDownloader;
	CArchiveUploader*	m_ArchiveUploader;
	CWebManager*		m_WebManager;
	CWebRepository*		m_WebRepository;
	CConnectorManager*	m_ConnectorManager;
	CLoginManager*		m_LoginManager;
	CCaptchaManager*	m_CaptchaManager;
#endif

	void				SetUpLimit(int UpLimit)		{m_UpLimit = UpLimit;}
	void				SetDownLimit(int DownLimit) {m_DownLimit = DownLimit;}

private slots:
	void				OnMessage(const QString& Message);

	void				OnRequest(QString Command, QVariant Parameters);

signals:
	void				Response(QString Command, QVariant Parameters);
	void				SplashMessage(QString Message);

protected:
	void				timerEvent(QTimerEvent* pEvent)
	{
		if(pEvent->timerId() == m_uTimerID)
			Process();
	}

	UINT				m_uTimerCounter;
	int					m_uTimerID;

	int					m_UpLimit;
	int					m_DownLimit;

	CCoreThread*		m_pThread;

	QMutex				m_Mutex;
	QMap<QString,time_t>m_Logins;

	QMap<QString, CSettings::SSetting>	GetDefaultCoreSettings();
};

extern CNeoCore* theCore;

class CCoreThread: public QThreadEx
{
	Q_OBJECT

public:
	CCoreThread(QObject* qObject = NULL)
	 : QThreadEx(qObject) {}

	void run()
	{
		theCore->Initialise();
		exec();
		theCore->Uninitialise();
	}
};
