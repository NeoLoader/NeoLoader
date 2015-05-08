#include "GlobalHeader.h"
#include "NeoCore.h"
#ifdef CRAWLER
#include "./FileTransfer/HosterTransfer/CrawlerManager.h"
#include "./FileList/Archiving/FileArchiver.h"
#endif
#ifndef NO_HOSTERS
#include "./FileTransfer/HosterTransfer/WebManager.h"
#include "./FileTransfer/HosterTransfer/ConnectorManager.h"
#include "./FileTransfer/HosterTransfer/CaptchaSolver.h"
#include "./FileTransfer/HosterTransfer/LoginManager.h"
#include "./FileTransfer/HosterTransfer/ArchiveDownloader.h"
#include "./FileTransfer/HosterTransfer/ArchiveUploader.h"
#include "./FileTransfer/HosterTransfer/LinkGrabber.h"
#include "./FileTransfer/HosterTransfer/WebRepository.h"
#include "./FileSearch/SiteCrawling/CrawlingManager.h"
#endif
#include "./FileList/FileManager.h"
#include "./FileTransfer/FileGrabber.h"
#include "./FileTransfer/BitTorrent/TorrentManager.h"
#include "./FileTransfer/BitTorrent/TorrentServer.h"
#include "./FileTransfer/ed2kMule/MuleManager.h"
#include "./FileTransfer/ed2kMule/MuleKad.h"
#include "./FileTransfer/ed2kMule/MuleServer.h"
#include "./FileTransfer/NeoShare/NeoManager.h"
#include "./FileTransfer/NeoShare/NeoKad.h"
#include "./FileTransfer/PeerWatch.h"
#include "./FileTransfer/DownloadManager.h"
#include "./FileTransfer/UploadManager.h"
#include "./GUI/NeoLoader.h"
#include "../Framework/HttpServer/HttpServer.h"
#include "./Interface/CoreServer.h"
#include "./Interface/InterfaceManager.h"
#include "../v8/include/v8.h"
#include "../Framework/RequestManager.h"
#include "./FileList/Hashing/HashingThread.h"
#include "./Networking/SocketThread.h"
#include "./Networking/Pinger.h"
#include "./Networking/BandwidthControl/BandwidthLimit.h"
#include "./FileList/IOManager.h"
#include "../Framework/Functions.h"
#include "../MiniUPnP/MiniUPnP.h"
#include "./FileSearch/SearchManager.h"
#include "../Framework/OtherFunctions.h"
#include "./Common/Caffeine/Caffeine.h"
#include "./Common/MemInfo.h"


CNeoCore* theCore = NULL;

CNeoCore::CNeoCore(CSettings* Settings)
: CLoggerTmpl<QObjectEx>("NeoCore")
{
	m_uTimerCounter = 0;

	theCore = this;

	m_Settings = Settings;
	m_CoreSettings = NULL;
	m_Stats = new QSettings(m_Settings->GetSettingsDir().append("/Statistics.ini"), QSettings::IniFormat, this);

#ifdef CRAWLER
	m_CrawlerManager = NULL;
	m_FileArchiver = NULL;
#endif
	m_Hashing = NULL;
	m_FileManager = NULL;
	m_DownloadManager = NULL;
	m_UploadManager = NULL;
	m_FileGrabber = NULL;
	m_TorrentManager = NULL;
	m_MuleManager = NULL;
	m_NeoManager = NULL;
	m_RequestManager = NULL;
	m_Network = NULL;
	m_Pinger = NULL;
	m_IOManager = NULL;
	m_PeerWatch = NULL;
	m_MiniUPnP = NULL;
	m_Caffeine = NULL;
	m_SearchManager = NULL;
#ifndef NO_HOSTERS
	m_WebManager = NULL;
	m_WebRepository = NULL;
	m_ConnectorManager = NULL;
	m_HttpServer = NULL;
	m_LoginManager = NULL;
	m_CrawlingManager = NULL;
	m_CaptchaManager = NULL;
#endif

	m_UpLimit = 0;
	m_DownLimit = 0;

	m_Server = NULL;
	m_Interfaces = NULL;

	m_uTimerID = 0;

	//QThreadPool::globalInstance()->setMaxThreadCount(3);

	m_pThread = new CCoreThread();
	moveToThread(m_pThread);
	m_pThread->start();
}

CNeoCore::~CNeoCore()
{
	m_pThread->exit();
	m_pThread->wait();
	delete m_pThread;
	theCore = NULL;

	/*static bool bDisposed = false;
	if(!bDisposed)
	{
		v8::V8::Dispose(); // clean up everything for memory leak detection
		bDisposed = true;
	}*/
}

void CNeoCore::Initialise()	
{
	qsrand(QTime::currentTime().msec()); // needs to be done in every thread we use random numbers

	m_CoreSettings = new CSettings("NeoCore", GetDefaultCoreSettings(), this);

	SetLogLimit(Cfg()->GetInt("Log/Limit"));
	if(Cfg()->GetBool("Log/Store"))
		SetLogPath(Cfg()->GetSettingsDir() + "/Logs");

	LoadLoginTokens();

	emit SplashMessage(tr("Initialising NeoCore"));
	m_Interfaces = new CInterfaceManager(this);
	m_RequestManager = new CRequestManager(theCore->Cfg()->GetInt("Browser/TimeoutSecs"), this);
	m_HttpServer = new CHttpServer(theCore->Cfg(false)->GetUInt("HttpServer/Port"),this);
	m_HttpServer->SetKeepAlive(theCore->Cfg(false)->GetUInt("HttpServer/KeepAlive"));
	m_HttpServer->SetTransferBufferSize(theCore->Cfg(false)->GetUInt("HttpServer/TransferBuffer"));
	m_IOManager = new CIOManager(this);
	m_PeerWatch = new CPeerWatch(this);
	m_MiniUPnP = new CMiniUPnP(this);
	m_Caffeine = new CCaffeine(this);
	m_SearchManager = new CSearchManager(this);
#ifdef CRAWLER
	m_CrawlingManager = new CCrawlingManager(this);
	m_FileArchiver = new CFileArchiver(this);
#endif
	m_Hashing = new CHashingThread(this);
	m_Network = new CSocketThread(this);
	m_Network->StartThread();
	m_Pinger = new CPinger(this);
	m_TorrentManager = new CTorrentManager(this);
	m_MuleManager = new CMuleManager(this);
	m_NeoManager = new CNeoManager(this);
	m_FileManager = new CFileManager(this);
	m_DownloadManager = new CDownloadManager(this);
	m_UploadManager = new CUploadManager(this);
	m_FileGrabber = new CFileGrabber(this);
#ifndef NO_HOSTERS
	m_ArchiveDownloader = new CArchiveDownloader(this);
	m_ArchiveUploader = new CArchiveUploader(this);
	m_WebManager = new CWebManager(this);
	m_WebManager->LoadScripts();
	m_WebRepository = new CWebRepository(this);
	m_ConnectorManager = new CConnectorManager(this);
	m_LoginManager = new CLoginManager(this);
	m_CaptchaManager = new CCaptchaManager(this);
#endif

	emit SplashMessage(tr("Loading FileList"));
	m_FileManager->Resume();
	m_SearchManager->LoadFromFile();
	m_FileGrabber->LoadFromFile();
	m_Hashing->start();

#ifdef CRAWLER
	m_CrawlerManager = new CCrawlerManager(this);
#endif

	m_Server = new CCoreServer(this);

	LogLine(LOG_INFO,tr("NeoLoader Core initialised"));

	m_uTimerID = startTimer(10);
}

void CNeoCore::Uninitialise()
{
	killTimer(m_uTimerID);

	LogLine(LOG_INFO,tr("NeoLoader Core closing"));

	QString LogPath = Cfg()->GetSettingsDir() + "/Logs/";
	CreateDir(LogPath);

	emit SplashMessage(tr("Saving FileList"));
	m_IOManager->Stop(); // this ends all IO operations
	m_FileManager->Suspend(); // this closes all IO
	if(theCore->Cfg()->GetBool("Content/SaveSearch"))
	{
		m_SearchManager->StoreToFile();
		m_FileGrabber->StoreToFile();
	}
	else
	{
		QFile::remove(theCore->Cfg()->GetSettingsDir() + "/SearchLists.xml");
		QFile::remove(theCore->Cfg()->GetSettingsDir() + "/GrabberList.xml");
	}
	m_Hashing->Stop(); // this stops hashing, doe to cloded IO this should be done instantly
	

	// Note: we can not use delete later here, as this thread is about to terminate and no events will be handled in time
	emit SplashMessage(tr("Ininitialising NeoCore"));
	delete m_RequestManager;
	m_RequestManager = NULL;
	delete m_Server;
	m_Server = NULL;
#ifdef CRAWLER
	delete m_CrawlerManager;
	m_CrawlerManager = NULL;
#endif
	delete m_FileManager;
	m_FileManager = NULL;
	delete m_DownloadManager;
	m_DownloadManager = NULL;
	delete m_UploadManager;
	m_UploadManager = NULL;
	delete m_Hashing;
	m_Hashing = NULL;
	delete m_FileGrabber;
	m_FileGrabber = NULL;
	delete m_SearchManager;
	m_SearchManager = NULL;
	delete m_TorrentManager;
	m_TorrentManager = NULL;
	delete m_MuleManager;
	m_MuleManager = NULL;
	delete m_NeoManager;
	m_NeoManager = NULL;
#ifdef CRAWLER
	delete m_CrawlingManager;
	m_CrawlingManager = NULL;
	delete m_FileArchiver;
	m_FileArchiver = NULL;
#endif
	delete m_Interfaces;
	m_Interfaces = NULL;
	delete m_HttpServer;
	m_HttpServer = NULL;
	delete m_Network;
	m_Network = NULL;
	delete m_Pinger;
	m_Pinger = NULL;
	delete m_IOManager;
	m_IOManager = NULL;
	delete m_PeerWatch;
	m_PeerWatch = NULL;
	delete m_MiniUPnP;
	m_MiniUPnP = NULL;
	delete m_Caffeine;
	m_Caffeine = NULL;
#ifndef NO_HOSTERS
	delete m_ArchiveDownloader;
	m_ArchiveDownloader = NULL;
	delete m_ArchiveUploader;
	m_ArchiveUploader = NULL;
	delete m_WebManager;
	m_WebManager = NULL;
	delete m_WebRepository;
	m_WebRepository = NULL;
	m_LoginManager->sync();
	delete m_LoginManager;
	m_LoginManager = NULL;
	delete m_CaptchaManager;
	m_CaptchaManager = NULL;
	delete m_ConnectorManager;
	m_ConnectorManager = NULL;
#endif

	m_CoreSettings->sync();
	delete m_CoreSettings;
	m_CoreSettings = NULL;
}

// gets called every 10 ms
void CNeoCore::Process()
{
	UINT Tick = MkTick(m_uTimerCounter);

	m_NeoManager->Process(); // Handle routes and pseudo sockets

	if(Tick & E10PerSec) // every 100 ms
	{
		m_Interfaces->Process();

		m_HttpServer->Process();

#ifdef CRAWLER
		m_CrawlerManager->Process(Tick);
#endif

		m_FileManager->Process(Tick);
		m_FileGrabber->Process(Tick);
		m_TorrentManager->Process(Tick);
		m_MuleManager->Process(Tick);
		m_NeoManager->Process(Tick);

		m_SearchManager->Process(Tick);
#ifndef NO_HOSTERS
		m_WebManager->Process(Tick);
#endif

#ifdef CRAWLER
		m_CrawlingManager->Process(Tick);
		m_FileArchiver->Process(Tick);
#endif
	}

	if(Tick & E4PerSec) // every 250 ms
	{
		m_DownloadManager->Process(Tick);
		m_UploadManager->Process(Tick);
#ifndef NO_HOSTERS
		m_ArchiveDownloader->Process(Tick);
		m_ArchiveUploader->Process(Tick);
#endif
	}

	if(Tick & EPerSec) // every s
	{
		m_Pinger->Process(Tick);

		m_RequestManager->Process(Tick);

#ifndef NO_HOSTERS
		m_ConnectorManager->Process(Tick);
		m_LoginManager->Process(Tick);

		m_CaptchaManager->Process(Tick);
#endif
	}

	if(Tick & EPerSec)
	{
		int Upload = m_UpLimit;
		if(Upload == 0)
			Upload = theCore->Cfg()->GetInt("Bandwidth/Upload");
		int Download = m_DownLimit;
		if(Download == 0)
			Download = theCore->Cfg()->GetInt("Bandwidth/Download");

		if((Upload == -1 /*|| Download == -1*/) && !theCore->Cfg()->GetString("Pinger/ToleranceVal").isEmpty())
		{
			if(!m_Pinger->IsEnabled())
				m_Pinger->SetEnabled(true);

			m_Pinger->SetUploadSpeed(m_Network->GetUpLimit()->GetRate());
			//m_Pinger->SetDownloadSpeed(m_Network->GetDownLimit()->GetRate());
		}
		else if(m_Pinger->IsEnabled())
			m_Pinger->SetEnabled(false);

		if(Upload == -1)
			Upload = m_Pinger->GetUploadSpeed();
		m_Network->GetUpLimit()->SetLimit(Upload);

		//if(Download == -1)
		//	Download = m_Pinger->GetDownloadSpeed();
		m_Network->GetDownLimit()->SetLimit(Download);

		if ((Tick & E100PerSec) == 0)
		{
			theCore->Stats()->setValue("Bandwidth/Uploaded", m_Network->GetStats().UploadedTotal);
			theCore->Stats()->setValue("Bandwidth/Downloaded", m_Network->GetStats().DownloadedTotal);
		}

		m_Network->UpdateIPs(theCore->Cfg()->GetString("Bandwidth/DefaultNIC"), theCore->Cfg()->GetBool("Bandwidth/UseIPv6"));
		m_Network->UpdateConfig();

		m_PeerWatch->Process(Tick);

		if(theCore->Cfg()->GetBool("Bandwidth/UseUPnP"))
		{
			int ServePort = 0;
			if(m_MiniUPnP->GetStaus("NeoLoaderServer", &ServePort) == -1 || ServePort != m_Server->GetPort())
				m_MiniUPnP->StartForwarding("NeoLoaderServer", m_Server->GetPort(), "TCP");

			int WebPort = 0;
			if(m_MiniUPnP->GetStaus("NeoLaoderWebUI", &WebPort) == -1 || WebPort != m_HttpServer->GetPort())
				m_MiniUPnP->StartForwarding("NeoLaoderWebUI", m_HttpServer->GetPort(), "TCP");
		}
		else
		{
			if(m_MiniUPnP->GetStaus("NeoLoaderServer") != -1)
				m_MiniUPnP->StopForwarding("NeoLoaderServer");

			if(m_MiniUPnP->GetStaus("NeoLaoderWebUI") != -1)
				m_MiniUPnP->StopForwarding("NeoLaoderWebUI");
		}

		if(theCore->Cfg()->GetBool("Other/PreventStandby"))
		{
			if(!m_Caffeine->IsRunning())
			{
				LogLine(LOG_INFO, tr("Standby is inhibited"));
				m_Caffeine->Start();
			}
		}
		else if(m_Caffeine->IsRunning())
			m_Caffeine->Stop();
	}

	if(Tick & EPer10Sec)
		CLogger::Instance()->Process();
}

void CNeoCore::OnMessage(const QString& Message)
{
	if(Message == "neo://quit")
		QCoreApplication::instance()->quit();
}

void CNeoCore::OnRequest(QString Command, QVariant Parameters)
{
	if(m_Server)
		emit Response(Command,m_Server->ProcessRequest(Command, Parameters));
}

QString CNeoCore::GetIncomingDir(bool bCreate)
{
	QString Path = Cfg()->GetString("Content/Incoming");
	if(Path.isEmpty())
	{
		if(Cfg()->IsPortable())
			Path = Cfg()->GetAppDir();
		else
			QDir::homePath() + "/Downloads/NeoLoader";
		Path += "/Incoming/";
	}
	if(Path.right(1) != "/")
		Path.append("/");
	if(bCreate)
		CreateDir(Path);
	return Path;
}

QString CNeoCore::GetTempDir(bool bCreate)
{
	QString Path = Cfg()->GetString("Content/Temp");
	if(Path.isEmpty())
	{
		if(Cfg()->IsPortable())
			Path = Cfg()->GetAppDir();
		else
			QDir::homePath() + "/Downloads/NeoLoader";
		Path += "/Temp/";
	}
	if(Path.right(1) != "/")
		Path.append("/");
	if(bCreate)
		CreateDir(Path);
	return Path;
}

//bool CNeoCore::TestLogin(const QString &UserName, const QString &Password)
bool CNeoCore::TestLogin(const QString &Password)
{
	//return (Cfg(false)->GetString("Core/UserName") == UserName &6 Cfg(false)->GetString("Core/Password") == Password);
	return Cfg(false)->GetString("Core/Password") == Password;
}

QString CNeoCore::GetLoginToken()
{
	// Threading Note: this function may be invoked form one different *Thread*, the GUI thread
	QMutexLocker Locker(&m_Mutex);

	QString LoginToken = GetRand64Str(true);
	m_Logins.insert(LoginToken, GetTime());
	Locker.unlock();
	SaveLoginTokens();
	return LoginToken;
}

bool CNeoCore::CheckLogin(const QString &LoginToken)
{
	QMutexLocker Locker(&m_Mutex);

	//if(Cfg(false)->GetString("Core/UserName").isEmpty() && Cfg(false)->GetString("Core/Password").isEmpty())
	if(Cfg(false)->GetString("Core/Password").isEmpty())
		return true;
	if(m_Logins.contains(LoginToken))
	{
		if(m_Logins.value(LoginToken) + Cfg(false)->GetUInt("Core/TockenTimeOut") > GetTime()
		 || m_Server->HasLoginToken(LoginToken)) // in this order the slower server sheck will be done selfom if at all
		{
			m_Logins[LoginToken] = GetTime();
			return true;
		}
		m_Logins.remove(LoginToken);
		Locker.unlock();
		SaveLoginTokens();
	}
	return false;
}

void CNeoCore::SaveLoginTokens()
{
	QMutexLocker Locker(&m_Mutex);

	QStringList Tokens;
	foreach(const QString& Token, m_Logins.keys())
	{
		if(m_Server->HasLoginToken(Token)) // don't save internal login tockens
			continue;
		Tokens.append(Token + ":" + QDateTime::fromTime_t(m_Logins.value(Token)).toString());
	}
	Cfg(false)->SetSetting("Core/ValidTockens", Tokens);
}

void CNeoCore::LoadLoginTokens()
{
	QMutexLocker Locker(&m_Mutex);

	time_t uNow = GetTime();
	time_t TimeOut = Cfg(false)->GetUInt("Core/TockenTimeOut");
	foreach(const QString& Token, Cfg(false)->GetStringList("Core/ValidTockens"))
	{
		StrPair TD = Split2(Token,":");
		time_t Time = QDateTime::fromString(TD.second).toTime_t();
		if(Time + TimeOut < uNow)
			continue;
		m_Logins.insert(TD.first, Time);
	}
}

QMap<QString, CSettings::SSetting> CNeoCore::GetDefaultCoreSettings()
{
	QMap<QString, CSettings::SSetting> Settings;

	// For values with limits is Default, Minimum, Maximum

	Settings.insert("Log/Merge", CSettings::SSetting(true));
	Settings.insert("Log/Store", CSettings::SSetting(false));
	Settings.insert("Log/Limit", CSettings::SSetting(10000,1000,100000));
	Settings.insert("Log/Level", CSettings::SSetting(1)); // 0 basic, 1 normal, 2 verbose

#ifdef _DEBUG
	Settings.insert("Modules/AutoStart",CSettings::SSetting(0));
#else
	Settings.insert("Modules/AutoStart",CSettings::SSetting(1));
#endif
	Settings.insert("Modules/NeoKad",CSettings::SSetting("NeoKad"));
	Settings.insert("Modules/MuleKad",CSettings::SSetting("MuleKad"));

	Settings.insert("Content/SaveInterval", CSettings::SSetting(MIN2S(5)));
	Settings.insert("Content/Temp", CSettings::SSetting(""));
	Settings.insert("Content/Incoming", CSettings::SSetting(""));
	Settings.insert("Content/UnifyedDirs", CSettings::SSetting(0));
	Settings.insert("Content/Shared", CSettings::SSetting(QStringList("")));
	Settings.insert("Content/VerifyTime", CSettings::SSetting(30));
	Settings.insert("Content/VerifySize", CSettings::SSetting(MB2B(5)));
	Settings.insert("Content/CacheLimit", CSettings::SSetting(MB2B(256), MB2B(128), MB2B(1024)));
	Settings.insert("Content/AddPaused", CSettings::SSetting(false));
	Settings.insert("Content/ShareNew", CSettings::SSetting(true));
	//Settings.insert("Content/ShowTemp", CSettings::SSetting(false));
	Settings.insert("Content/MagnetDomain", CSettings::SSetting("http://link.neoloader.to/"));
	Settings.insert("Content/MagnetRetry", CSettings::SSetting(MIN2S(10)));
#ifdef DECODER
	Settings.insert("Content/MagnetTimeLimit", CSettings::SSetting(MIN2S(60)));
	Settings.insert("Content/MagnetVolumeLimit", CSettings::SSetting(15));
	Settings.insert("Content/MagnetAllowDisplay", true);
#endif
	Settings.insert("Content/EndGameVolume", CSettings::SSetting(5,0,10));
	//Settings.insert("Content/AlwaysEndGame", CSettings::SSetting(true));
	Settings.insert("Content/FuseMount", CSettings::SSetting(""));
#ifndef WIN32
    Settings.insert("Content/FuseOptions", CSettings::SSetting("-o|allow_other"));
#endif
	Settings.insert("Content/IgnoreLastModified", CSettings::SSetting(0));
	Settings.insert("Content/ShareRatio", CSettings::SSetting(0)); // 0 means no ratio
	Settings.insert("Content/ShareTime", CSettings::SSetting(0)); // 0 means no limit
	Settings.insert("Content/AutoP2P", CSettings::SSetting(false));
	Settings.insert("Content/PreparePreview", CSettings::SSetting(false));
	Settings.insert("Content/Streamable", CSettings::SSetting("(mp4|m4v|divx|avi|mkv|mov|wmv|asf|mped|mpg|flv)"));
	Settings.insert("Content/FrontLoadSize", CSettings::SSetting(MB2B(8)));
	Settings.insert("Content/BackLoadSize", CSettings::SSetting(MB2B(2)));

	Settings.insert("Content/Preallocation", CSettings::SSetting(MB2B(100)));
	Settings.insert("Content/CalculateHashes", CSettings::SSetting(true));

	Settings.insert("Content/SaveSearch", CSettings::SSetting(true));

#ifdef _DEBUG
	Settings.insert("Other/PreventStandby", CSettings::SSetting(false));
#else
	Settings.insert("Other/PreventStandby", CSettings::SSetting(true));
#endif

	Settings.insert("Bandwidth/Upload", CSettings::SSetting(-1));
	Settings.insert("Bandwidth/Download", CSettings::SSetting(-1));
#ifdef _DEBUG
	Settings.insert("Bandwidth/UseUPnP",CSettings::SSetting(false));
#else
	Settings.insert("Bandwidth/UseUPnP",CSettings::SSetting(true));
#endif
	Settings.insert("Bandwidth/UseIPv6",CSettings::SSetting(false));
	Settings.insert("Bandwidth/DefaultNIC",CSettings::SSetting(""));
	Settings.insert("Bandwidth/WebProxy", CSettings::SSetting(""));
	Settings.insert("Bandwidth/MaxConnections", CSettings::SSetting(2500));
	Settings.insert("Bandwidth/MaxNewPer5Sec", CSettings::SSetting(250));
	Settings.insert("Bandwidth/TransportLimiting", CSettings::SSetting(true));
	Settings.insert("Bandwidth/FrameOverhead", CSettings::SSetting(18));

	Settings.insert("Upload/HistoryDepth", CSettings::SSetting(10,50,100));
	Settings.insert("Upload/UpWaitTime", CSettings::SSetting(HR2S(3),HR2S(1),HR2S(5)));
	Settings.insert("Upload/SlotSpeed", CSettings::SSetting(KB2B(8)));
	//Settings.insert("Upload/SlotFocus", CSettings::SSetting(false));
	Settings.insert("Upload/TrickleVolume", CSettings::SSetting(2));
	Settings.insert("Upload/TrickleSpeed", CSettings::SSetting(KB2B(1)));
	Settings.insert("Upload/DropBlocking", CSettings::SSetting(false));

	Settings.insert("Pinger/MinSpeed", CSettings::SSetting(KB2B(10))); // 10 kb/s
	Settings.insert("Pinger/MaxSpeed", CSettings::SSetting(-1));
	Settings.insert("Pinger/HostsToTrace", CSettings::SSetting(10));
	Settings.insert("Pinger/MaxFails", CSettings::SSetting(60));
	Settings.insert("Pinger/InitialPings", CSettings::SSetting(10));
	Settings.insert("Pinger/ToleranceVal", CSettings::SSetting(""));
	Settings.insert("Pinger/UpDivider", CSettings::SSetting(10));
	Settings.insert("Pinger/DownDivider", CSettings::SSetting(10));
	Settings.insert("Pinger/Average", CSettings::SSetting(3));
	Settings.insert("Pinger/Host", CSettings::SSetting(""));


	Settings.insert("PeerWatch/Enable",CSettings::SSetting(2));
	Settings.insert("PeerWatch/BlockTime", CSettings::SSetting(MIN2S(10),MIN2S(5),MIN2S(60)));
	Settings.insert("PeerWatch/BanTime", CSettings::SSetting(HR2S(2),HR2S(1),HR2S(6)));
	//Settings.insert("PeerWatch/IPFilter",CSettings::SSetting(""));

	Settings.insert("NeoShare/IdleTimeout",CSettings::SSetting(MIN2S(2),MIN2S(1),MIN2S(5)));
	Settings.insert("NeoShare/KeepAlive",CSettings::SSetting(60,30,MIN2S(3)));
	Settings.insert("NeoShare/ConnectTimeout",CSettings::SSetting(20,10,120));
	Settings.insert("NeoShare/RouteTimeout", CSettings::SSetting(MIN2S(5),MIN2S(1),MIN2S(15)));
	Settings.insert("NeoShare/MaxEntities", CSettings::SSetting(250));
	Settings.insert("NeoShare/RequestInterval",CSettings::SSetting(MIN2S(20),MIN2S(10),MIN2S(60)));
	Settings.insert("NeoShare/RequestLimit",CSettings::SSetting(100,10,250));
	Settings.insert("NeoShare/SaveInterval", CSettings::SSetting(MIN2S(5)));
	Settings.insert("NeoShare/SaveEntities",CSettings::SSetting(false));
	Settings.insert("NeoShare/KadPublishmentVolume",CSettings::SSetting(10,5,100));
	Settings.insert("NeoShare/KadLookupVolume",CSettings::SSetting(20,5,100));
	Settings.insert("NeoShare/KadLookupInterval",CSettings::SSetting(MIN2S(20),MIN2S(10),MIN2S(60)));
	Settings.insert("NeoShare/HubCheckoutInterval",CSettings::SSetting(MIN2S(40),MIN2S(20),MIN2S(120)));
	Settings.insert("NeoShare/LoadTreshold", CSettings::SSetting(75,50,100));
	Settings.insert("NeoShare/SaturationLimit", CSettings::SSetting(10,5,20));
	Settings.insert("NeoShare/Enable", CSettings::SSetting(true));
	Settings.insert("NeoShare/Anonymity", CSettings::SSetting(0, 0, 4));
	Settings.insert("NeoShare/Upload", CSettings::SSetting(-1));
	Settings.insert("NeoShare/Download", CSettings::SSetting(-1));
	Settings.insert("NeoShare/Priority", CSettings::SSetting(0));

	Settings.insert("NeoKad/Port", CSettings::SSetting(GetRandomInt(9000, 9999)));
	Settings.insert("NeoKad/TargetID", CSettings::SSetting(""));
	Settings.insert("NeoKad/LastDistance", CSettings::SSetting(0));
	Settings.insert("NeoKad/EntityKey", CSettings::SSetting(""));
	Settings.insert("NeoKad/StoreKey", CSettings::SSetting(""));

	Settings.insert("BitTorrent/Tracking", CSettings::SSetting("All-Tiers")); // "No-Trackers" "One-Tracker" "All-Trackers" "All-Tiers"
	Settings.insert("BitTorrent/MaxPeers", CSettings::SSetting(500, 10, 1000));
	Settings.insert("BitTorrent/ServerPort", CSettings::SSetting(GetRandomInt(6800, 6900)));
	Settings.insert("BitTorrent/AnnounceInterval",CSettings::SSetting(MIN2S(5),MIN2S(1),MIN2S(60)));
	Settings.insert("BitTorrent/AnnounceWanted",CSettings::SSetting(200,1,1000));
	Settings.insert("BitTorrent/ConnectTimeout",CSettings::SSetting(20,10,30));
	Settings.insert("BitTorrent/IdleTimeout",CSettings::SSetting(90,15,320));
	Settings.insert("BitTorrent/KeepAlive",CSettings::SSetting(30,15,60));
	Settings.insert("BitTorrent/RequestTimeout",CSettings::SSetting(60,15,120));
	Settings.insert("BitTorrent/RequestLimit",CSettings::SSetting(250,100,250));
	//Settings.insert("BitTorrent/RetryInterval",CSettings::SSetting(MIN2S(10),MIN2S(1),MIN2S(58)));
	Settings.insert("BitTorrent/Encryption", CSettings::SSetting("Support"));
	Settings.insert("BitTorrent/uTP", CSettings::SSetting(true));
	Settings.insert("BitTorrent/MaxRendezvous", CSettings::SSetting(5,3,10));
	Settings.insert("BitTorrent/CryptTCPPaddingLength",CSettings::SSetting(127,0,511));
	Settings.insert("BitTorrent/EnableTracker",CSettings::SSetting(1)); // 0 off, 1 on, 2 on but only for own torrents
	Settings.insert("BitTorrent/SavePeers",CSettings::SSetting(false));
	Settings.insert("BitTorrent/Enable", CSettings::SSetting(true));
	Settings.insert("BitTorrent/MaxTorrents", CSettings::SSetting(5));
	Settings.insert("BitTorrent/Upload", CSettings::SSetting(-1));
	Settings.insert("BitTorrent/Download", CSettings::SSetting(-1));
	Settings.insert("BitTorrent/Priority", CSettings::SSetting(0));

	Settings.insert("MainlineDHT/NodeID", CSettings::SSetting(""));
	Settings.insert("MainlineDHT/Address", CSettings::SSetting(""));
	Settings.insert("MainlineDHT/RouterNodes", CSettings::SSetting(QString("router.bittorrent.com:6881|router.utorrent.com:6881").split("|")));

	Settings.insert("Ed2kMule/UserHash", CSettings::SSetting(""));
	Settings.insert("Ed2kMule/NickName", CSettings::SSetting("http://neoloader.to"));
	Settings.insert("Ed2kMule/MaxSources", CSettings::SSetting(500, 50, 5000));
	Settings.insert("Ed2kMule/TCPPort", CSettings::SSetting(GetRandomInt(4600, 4700)));
	Settings.insert("Ed2kMule/UDPPort", CSettings::SSetting(GetRandomInt(4700, 4800)));
	Settings.insert("Ed2kMule/CheckFWInterval", CSettings::SSetting(MIN2S(15), MIN2S(10), MIN2S(60)));
	Settings.insert("Ed2kMule/ConnectTimeout",CSettings::SSetting(20,10,30));
	Settings.insert("Ed2kMule/IdleTimeout",CSettings::SSetting(60,15,320));
	Settings.insert("Ed2kMule/IncomingTimeOut",CSettings::SSetting(MIN2S(10),MIN2S(5),MIN2S(60)));
	Settings.insert("Ed2kMule/ReaskInterval",CSettings::SSetting(MIN2S(29),MIN2S(10),MIN2S(58)));
	Settings.insert("Ed2kMule/LanMode", CSettings::SSetting(false));
	Settings.insert("Ed2kMule/Obfuscation", CSettings::SSetting("Support"));
	Settings.insert("Ed2kMule/CryptTCPPaddingLength",CSettings::SSetting(127,0,254));
	Settings.insert("Ed2kMule/NatTraversal", CSettings::SSetting(true));
	Settings.insert("Ed2kMule/KadLookupInterval",CSettings::SSetting(MIN2S(29),MIN2S(10),MIN2S(58)));
	Settings.insert("Ed2kMule/KadMaxLookup",CSettings::SSetting(5,5,50));
	Settings.insert("Ed2kMule/SXInterval",CSettings::SSetting(MIN2S(10),MIN2S(5),MIN2S(40)));
	Settings.insert("Ed2kMule/SXVolume",CSettings::SSetting(20,5,50));
	Settings.insert("Ed2kMule/SaveSources",CSettings::SSetting(false));
	Settings.insert("Ed2kMule/Enable", CSettings::SSetting(true));
	Settings.insert("Ed2kMule/ShareDefault", CSettings::SSetting(false));
	Settings.insert("Ed2kMule/Upload", CSettings::SSetting(-1));
	Settings.insert("Ed2kMule/Download", CSettings::SSetting(-1));
	Settings.insert("Ed2kMule/Priority", CSettings::SSetting(0));
	Settings.insert("Ed2kMule/HashMode", CSettings::SSetting("Random-Secure")); // Random, Static, Random-Secure, Static-Secure
	Settings.insert("Ed2kMule/SUIKey", CSettings::SSetting(""));
	Settings.insert("Ed2kMule/HashAge", CSettings::SSetting(""));
	Settings.insert("Ed2kMule/UseServers", CSettings::SSetting("Static")); // None, Custom, Static, One, Booth
	Settings.insert("Ed2kMule/StaticServers", CSettings::SSetting(QString("ed2k://|server|91.200.42.46|1176|/\r\ned2k://|server|91.200.42.47|3883|/\r\ned2k://|server|91.200.42.119|9939|/\r\ned2k://|server|176.103.48.36|4184|/\r\ned2k://|server|77.120.115.66|5041|/\r\ned2k://|server|46.105.126.71|4661|/").split("\r\n")));
	Settings.insert("Ed2kMule/KeepServers",CSettings::SSetting(MIN2S(10),MIN2S(5),MIN2S(58)));
	Settings.insert("Ed2kMule/MinHordeSlots",CSettings::SSetting(1,0,10));

	Settings.insert("Hoster/MinWebTasks", CSettings::SSetting(10));
	Settings.insert("Hoster/MaxNewPer5Sec", CSettings::SSetting(25));
	Settings.insert("Hoster/MaxWebTasks", CSettings::SSetting(100));
	Settings.insert("Hoster/AccessFailedTimeOut", CSettings::SSetting(10));
	Settings.insert("Hoster/ServerBanTime", CSettings::SSetting(HR2S(2)));
	Settings.insert("Hoster/ServerBanTreshold", CSettings::SSetting(20));
	Settings.insert("Hoster/ServerDownTreshold", CSettings::SSetting(4));
	Settings.insert("Hoster/MaxUploads", CSettings::SSetting(1));
	Settings.insert("Hoster/MaxDownloads", CSettings::SSetting(5));
	Settings.insert("Hoster/MaxChecks", CSettings::SSetting(5));
	Settings.insert("Hoster/ReCheckInterval", CSettings::SSetting(HR2S(24),HR2S(1),HR2S(240)));
	Settings.insert("Hoster/AutoReUpload", CSettings::SSetting(true));
	Settings.insert("Hoster/MaxCheckouts", CSettings::SSetting(3));
	Settings.insert("Hoster/CheckoutInterval", CSettings::SSetting(MIN2S(60),MIN2S(30),HR2S(6)));
	Settings.insert("Hoster/MaxTransferRetry", CSettings::SSetting(3));
	Settings.insert("Hoster/ServerRetryDelay", CSettings::SSetting(MIN2S(2)));
	Settings.insert("Hoster/CaptchaRetryDelay", CSettings::SSetting(MIN2S(20)));
	Settings.insert("Hoster/MaxBadCaptcha", CSettings::SSetting(3));
	Settings.insert("Hoster/ManualCaptcha", CSettings::SSetting(1));
	Settings.insert("Hoster/AutoCleanUp", CSettings::SSetting(true));
	Settings.insert("Hoster/Encap", CSettings::SSetting("7z"));
	Settings.insert("Hoster/RarPath", CSettings::SSetting(""));
	Settings.insert("Hoster/RarComment", CSettings::SSetting(""));
#ifdef CRAWLER
	Settings.insert("Hoster/DecapCleanup", CSettings::SSetting("*.url;*.lnk;thumbs.db"));
	Settings.insert("Hoster/KeepSingleArchives", CSettings::SSetting(false));
#else
	Settings.insert("Hoster/KeepSingleArchives", CSettings::SSetting(true));
#endif
	Settings.insert("Hoster/EncryptUploads", CSettings::SSetting("RC4"));
	Settings.insert("Hoster/ProtectLinks", CSettings::SSetting(true));
	Settings.insert("Hoster/Enable", CSettings::SSetting(true));
	Settings.insert("Hoster/UseCaptcha", CSettings::SSetting(false));
	Settings.insert("Hoster/PartSize", CSettings::SSetting(0));
	Settings.insert("Hoster/MaxArcTasks", CSettings::SSetting(5));
	Settings.insert("Hoster/Upload", CSettings::SSetting(-1));
	Settings.insert("Hoster/Download", CSettings::SSetting(-1));
	Settings.insert("Hoster/Priority", CSettings::SSetting(0));

	Settings.insert("HosterCache/CacheMode", CSettings::SSetting("Off")); // Off, On, All
	Settings.insert("HosterCache/SelectionMode", CSettings::SSetting("Auto")); // All, Auto, Selected - hosters
	Settings.insert("HosterCache/PartSize", CSettings::SSetting(MB2B(100)));
	Settings.insert("HosterCache/MaxAvail", CSettings::SSetting(3));
	Settings.insert("HosterCache/MaxJoints", CSettings::SSetting(1));

	Settings.insert("HashInspector/Majority",CSettings::SSetting(90,80,100));
	Settings.insert("HashInspector/Quorum",CSettings::SSetting(10,5,20));

	Settings.insert("CorruptionLogger/MonitorTime", CSettings::SSetting(HR2S(4),HR2S(2),HR2S(16)));
	Settings.insert("CorruptionLogger/DropRatio", CSettings::SSetting(40, 10, 100));

	//Settings.insert("Script/AutoUpdate", CSettings::SSetting(true));
	//Settings.insert("Script/UserName", CSettings::SSetting(""));
	//Settings.insert("Script/Password", CSettings::SSetting(""));

	Settings.insert("Browser/MaxRetries", CSettings::SSetting(3));
	Settings.insert("Browser/TimeoutSecs", CSettings::SSetting(30));
	Settings.insert("Browser/ReplyBuffer", CSettings::SSetting(MB2B(4),KB2B(256),MB2B(64)));
	Settings.insert("Browser/TransferBuffer",CSettings::SSetting(KB2B(16),KB2B(4),KB2B(256)));

	return Settings;
}
