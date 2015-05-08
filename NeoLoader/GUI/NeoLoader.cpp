#include "GlobalHeader.h"
#include <QStatusBar>
#include <QMenuBar>
#include <QMessageBox>
#include <QLabel>
#include <QApplication>
#include <QtGui/QClipboard>
#include <QtGui/QCloseEvent>
#include <QDesktopServices>
#include "../../Framework/MT/ThreadEx.h"
#include "../NeoCore.h"
#include "../NeoVersion.h"
#include "../Interface/CoreClient.h"
#include "../Interface/CoreServer.h"
#include "NeoLoader.h"
//#include "../../qtservice/src/qtservice.h"
#include "../../Framework/Xml.h"
#include "../../NeoGUI/NeoGUI.h"
#ifndef NO_HOSTERS
#include "./ScriptDebugger/NeoScriptDebugger.h"
#endif
#include <QDialogButtonBox>
#include <QWidgetAction>
#include <QSpinBox>

CNeoLoader* theLoader = NULL;

CNeoLoader::CNeoLoader(CSettings* Settings, int eMode)
{
	theLoader = this;

	if(!CLogger::Instance())
		new CLogger("NeoGUI");

	m_NeoMode = (eMode & eMask);
	bool bMinimized = (eMode & eMinimized);
	m_Settings = Settings;

	m_LastCheck = 0;
	m_Updater = NULL;

#ifndef _DEBUG
	if(Cfg()->GetString("Core/Password") == "*")
	{
		Cfg()->SetSetting("Core/Password", GetRand64Str(true));
		Cfg()->sync();
	}
#endif

	m_pSplash = NULL;
	if(Cfg()->GetBool("Sartup/Splash") && GetMode() != CNeoLoader::eGUIProcess && !bMinimized)
	{
		m_pSplash = new QSplashScreen;
		m_pSplash->setPixmap(QPixmap(":/Splash"));
		m_pSplash->show();

		m_pSplash->showMessage(tr("Starting NeoLoader..."), Qt::AlignRight | Qt::AlignTop, Qt::white);
		if(theCore)
			connect(theCore, SIGNAL(SplashMessage(QString)), this, SLOT(OnSplashMessage(QString)));
	}

	m_Client = new CCoreClient(this);

	m_ConnectTimer = 0;
	m_IsConnected = m_Client->HasCore();

	m_CoreProcess = NULL;

	m_GUIProcess = NULL;

	if(Itf()->HasCore())
	{
		//if(m_pSplash) m_pSplash->showMessage(tr("Waiting for NeoLoader Core..."), Qt::AlignRight | Qt::AlignTop, Qt::white);
		connect(theLoader->Itf(), SIGNAL(Request(QString, QVariant)), theCore, SLOT(OnRequest(QString, QVariant)));
		connect(theCore, SIGNAL(Response(QString, QVariant)), theLoader->Itf(), SLOT(OnResponse(QString, QVariant)));
		while(!theLoader->Itf()->IsConnected())
			QThreadEx::msleep(100);
	}

	if(m_pSplash)
		m_pSplash->close();

	m_Title = GetNeoVersion(true);
#ifdef CRAWLER
	m_Title += " - CRAWLER";
#endif
#ifdef _DEBUG
	m_Title += " - DEBUG";
#endif

#ifndef WIN32
	QIcon AppIcon;
	AppIcon.addFile(":/Home");
    QApplication::setWindowIcon(AppIcon);
#endif

	m_pTrayIcon = NULL;
	m_pTrayMenu = NULL;

	m_pWnd = NULL;
	if(GetMode() != CNeoLoader::eCoreProcess)
	{
		m_pWnd = new CNeoGUI(Cfg(), Itf()->HasCore());

		m_pWnd->setWindowTitle(m_Title);

		connect(m_pWnd, SIGNAL(SendRequest(QString, QVariantMap)), theLoader, SLOT(SendRequest(QString, QVariantMap)));
		connect(theLoader, SIGNAL(DispatchResponse(QString, QVariantMap)), m_pWnd, SLOT(DispatchResponse(QString, QVariantMap)));

		connect(m_pWnd, SIGNAL(Connect()), this, SLOT(ConnectCore()));
		connect(m_pWnd, SIGNAL(Disconnect()), this, SLOT(DisconnectCore()));

#ifndef NO_HOSTERS
		connect(m_pWnd, SIGNAL(OpenDebugger()), this, SLOT(OnDebugger()));
#endif

		connect(m_pWnd, SIGNAL(CheckUpdates()), this, SLOT(CheckUpdates()));
		connect(this, SIGNAL(UpdatesFound(int)), m_pWnd, SLOT(UpdatesFound(int)));

		if(Itf()->HasCore())
			m_pWnd->Suspend(false);
	}
	else
	{
		// Tray BEGIN
		QIcon Icon;
		Icon.addFile(":/Home");
		m_pTrayIcon = new QSystemTrayIcon(Icon, this);
		m_pTrayIcon->setToolTip(m_Title);
		connect(m_pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSysTray(QSystemTrayIcon::ActivationReason)));
	
		//m_pTrayIcon->setContextMenu(m_pNeoMenu);

		m_pTrayMenu = new QMenu();
	
		QAction* pAbout = new QAction(tr("&About"), this);
		connect(pAbout, SIGNAL(triggered()), this, SLOT(OnAbout()));
		m_pTrayMenu->addAction(pAbout);

		m_pTrayMenu->addSeparator();

		QAction* pExit = new QAction(tr("E&xit"), m_pTrayMenu);
		connect(pExit, SIGNAL(triggered()), this, SLOT(OnExit()));
		m_pTrayMenu->addAction(pExit);
		// Tray END

		m_pTrayIcon->show();
	}

	if(!bMinimized &&  !m_pWnd)
		theLoader->StartNeoGUI();

	m_TimerId = startTimer(100);
}

CNeoLoader::~CNeoLoader()
{
	killTimer(m_TimerId);

#ifndef NO_HOSTERS
	CNeoScriptDebugging::CloseAll();
#endif

	if(m_pTrayIcon)
	{
		m_pTrayIcon->hide();
		delete m_pTrayMenu;
	}

	delete m_Client;
	
	//m_Settings->sync();
	//delete m_Settings;

	delete m_pWnd;

	delete m_pSplash;

	delete m_Updater;

	theLoader = NULL;
}

void CNeoLoader::OnSplashMessage(QString Message)
{
	if(m_pSplash) m_pSplash->showMessage(Message, Qt::AlignRight | Qt::AlignTop, Qt::white);
}

void CNeoLoader::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId)
	{
		QObject::timerEvent(e);
		return;
	}

	if(!Itf()->HasCore())
	{
		if(!Itf()->IsConnected())
		{
			if(m_IsConnected) // did we got disconnected
			{
				if(m_pWnd)
				{
					m_pWnd->Suspend(true);
					if(m_NeoMode == eLocalGUI || m_NeoMode == eRemoteGUI)
						m_pWnd->setWindowTitle(m_Title + tr(" - ..."));
				}

				m_ConnectTimer = 0;

				m_IsConnected = false;
			}

			if(Cfg()->GetInt("Core/AutoConnect") || m_NeoMode == eGUIProcess)
			{
				if(m_ConnectTimer-- == 0 && Itf()->IsDisconnected())
					ConnectCore();
				else if(m_ConnectTimer == -(Cfg()->GetInt("Core/TimeoutSecs")))
				{
					m_ConnectTimer = Cfg()->GetInt("Core/TimeoutSecs");

					DisconnectCore();

					if(m_NeoMode == eLocalGUI && Cfg()->GetInt("Core/AutoConnect") == 2)
						StartNeoCore();
				}
			}
		}
		else if(!m_IsConnected) // did we connect
		{
			if(m_pWnd)
			{
				m_pWnd->Suspend(false);
				if(m_NeoMode == eLocalGUI)
					m_pWnd->setWindowTitle(m_Title + tr(" - %1").arg(Cfg()->GetString("Core/LocalName")));
				else if(m_NeoMode == eRemoteGUI)
					m_pWnd->setWindowTitle(m_Title + tr(" - %1:%2").arg(Cfg()->GetUInt("Core/RemoteHost")).arg(Cfg()->GetString("Core/RemotePort")));
			}

			m_IsConnected = true;
		}
	}

	int UpdateInterval = theLoader->Cfg()->GetInt("Updater/UpdateInterval");
	if(m_Updater == NULL && (UpdateInterval ? (m_LastCheck + UpdateInterval < GetTime()) : (m_LastCheck == 0)) 
	 && theLoader->Cfg()->GetInt("Updater/AutoUpdate") && theLoader->Itf()->HasCore())
		CheckUpdates();
}

void CNeoLoader::CheckUpdates()
{
	m_LastCheck = GetTime();
	
	m_Updater = new QProcess();
	connect(m_Updater, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(OnUpdate(int, QProcess::ExitStatus)));
	m_Updater->setWorkingDirectory(theLoader->Cfg()->GetAppDir());
#ifndef WIN32
	m_Updater->start(theLoader->Cfg()->GetAppDir() + "/NeoSetup", QString("-embedded|-update|-download").split("|"));
#else
	m_Updater->start(theLoader->Cfg()->GetAppDir() + "/NeoSetup.exe", QString("-embedded|-update|-download").split("|"));
#endif
}

int g_iUpdateReady = 0;

void CNeoLoader::OnUpdate(int exitCode, QProcess::ExitStatus exitStatus)
{
	emit UpdatesFound(exitCode);

	if(exitCode > 0)
		g_iUpdateReady = exitCode;
	
	m_Updater->deleteLater();
	m_Updater = NULL;
}

void CNeoLoader::ConnectCore()
{
	if(Itf()->HasCore())
		return;

	if(m_NeoMode == eLocalGUI || m_NeoMode == eGUIProcess)
	{
		QString Name = Cfg()->GetString("Core/LocalName");
		Itf()->ConnectLocal(Name);
	}
	else if(m_NeoMode == eRemoteGUI)
	{
		quint16 Port = Cfg()->GetUInt("Core/RemotePort");
		QString Host = Cfg()->GetString("Core/RemoteHost");
		if(Port && !Host.isEmpty())
			//Itf()->ConnectRemote(Host, Port, Cfg()->GetString("Core/UserName"), Cfg()->GetString("Core/Password"));
			Itf()->ConnectRemote(Host, Port, "", Cfg()->GetString("Core/Password"));
	}
}

void CNeoLoader::DisconnectCore()
{
	if(Itf()->HasCore())
		return;

	Itf()->Disconnect();
}

void CNeoLoader::StartNeoCore()
{
	if(NeoCoreRunning())
		StopNeoCore();

	/*QtServiceController svc(theLoader->Cfg()->GetString("Core/ServiceName"));
	if(svc.isInstalled())
		svc.start();
	else*/
	{
		if(m_CoreProcess)
			delete m_CoreProcess;
		m_CoreProcess = new QProcess();
		QStringList Arguments;
		Arguments.append("-core");
		m_CoreProcess->start(QApplication::applicationFilePath(),Arguments);
	}
}

void CNeoLoader::StopNeoCore()
{
	QStringList Arguments;
	Arguments.append("-core");
	Arguments.append("neo://quit");
	QProcess::startDetached(QApplication::applicationFilePath(), Arguments);

	/*QtServiceController svc(theLoader->Cfg()->GetString("Core/ServiceName"));
	if(svc.isInstalled())
		svc.stop();
	else */if(m_CoreProcess && m_CoreProcess->state() != QProcess::NotRunning)
	{
		if(!m_CoreProcess->waitForFinished(20))
			m_CoreProcess->terminate();
		delete m_CoreProcess;
		m_CoreProcess = NULL;
	}
}

bool CNeoLoader::NeoCoreRunning()
{
	/*QtServiceController svc(theLoader->Cfg()->GetString("Core/ServiceName"));
	if(svc.isInstalled())
		return svc.isRunning();
	else*/
		return m_CoreProcess && m_CoreProcess->state() == QProcess::Running;
}

void CNeoLoader::StartNeoGUI()
{
	if(m_GUIProcess)
	{
		if(m_GUIProcess->state() == QProcess::Running)
			return;
		delete m_GUIProcess;
	}

	QStringList Arguments;
	Arguments.append("-gui");
	Arguments.append("-tray");

	m_GUIProcess = new QProcess(this);
	m_GUIProcess->start(QApplication::applicationFilePath(), Arguments);
	m_pTrayIcon->hide();

	connect(m_GUIProcess, SIGNAL(finished(int)), m_pTrayIcon, SLOT(show()));
}

void CNeoLoader::Dispatch(const QString& Command, const QVariantMap& Response)
{
#ifndef NO_HOSTERS
	if(Command == "DbgAddTask")
	{
		uint64 ID = Response["ID"].toULongLong();
		if(ID)
			new CNeoScriptDebugging(ID);
		else
			QMessageBox::critical(NULL, tr("NeoLoader"), tr("failed to add new debuger task"));
	}
	else if(Command == "DbgRemoveTask")
	{

	}
	else if(Command == "DbgCommand")
	{
		CNeoScriptDebugging::Dispatch(Response);
	}
	else
#endif
		emit DispatchResponse(Command, Response);
}

void CNeoLoader::SendRequest(const QString& Command, const QVariantMap& Request)
{
	m_Client->SendRequest(Command, Request);
}

void CNeoLoader::OnMessage(const QString& Message)
{
	if(Message == "neo://quit")
		QApplication::instance()->quit();
	else
	{
		bool bUseGrabber = Cfg()->GetBool("Integration/SkipGrabber") == false;

		if(QRegExp("(magnet|ed2k|jd|jdlist|dlc|http|https|ftp)\\:.+").exactMatch(Message.trimmed()))
		{
			QVariantMap Request;
			Request["Links"] = Message;
			if(!bUseGrabber)
				Request["Direct"] = true;
			theLoader->Itf()->SendRequest("GrabLinks", Request);
		}
		else
		{
			QFile File(Message);
			File.open(QFile::ReadOnly);

			if(!(Message.right(7).compare("torrent", Qt::CaseInsensitive) == 0 || Message.right(14).compare("mulecollection", Qt::CaseInsensitive) == 0))
				bUseGrabber = true; // if this is not a torrent or a collection it must go through the grabber

			QVariantMap Request;
			Request["FileName"] = Message;
			Request["FileData"] = File.readAll();
			theLoader->Itf()->SendRequest(bUseGrabber ? "GrabLinks" : "AddFile", Request);
		}
	}
}

void CNeoLoader::OnSysTray(QSystemTrayIcon::ActivationReason Reason)
{
	switch(Reason)
	{
		case QSystemTrayIcon::Context:
			m_pTrayMenu->popup(QCursor::pos());	
			break;
		case QSystemTrayIcon::DoubleClick:
			theLoader->StartNeoGUI();
			break;
	}
}

void CNeoLoader::OnDebugger()
{
#ifndef NO_HOSTERS
	CNeoScriptDebugging::OpenNew();
#endif
}

void CNeoLoader::OnExit()
{
	if(QMessageBox("NeoLoader", tr("Do you want to close NeoLoader?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;
	QApplication::instance()->quit();
}

void CNeoLoader::OnAbout()
{
	QDesktopServices::openUrl(QUrl("http://neoloader.to/?dest=aboutpage"));
}

void CNeoLoader::CreateFileIcon(QString Ext)
{
	CNeoGUI::MakeFileIcon(Ext);
}

QMap<QString, CSettings::SSetting> CNeoLoader::GetDefaultSettings()
{
	QMap<QString, CSettings::SSetting> Settings;

	Settings.insert("Sartup/Splash", CSettings::SSetting(true));

	Settings.insert("Updater/AutoUpdate", CSettings::SSetting(2));
	Settings.insert("Updater/UpdateInterval", CSettings::SSetting(DAY2S(1)));

	Settings.insert("Integration/SysTray", CSettings::SSetting(false));
	Settings.insert("Integration/SysTrayTrigger", CSettings::SSetting(1));
	//Settings.insert("Integration/Separate", CSettings::SSetting(false));
#ifndef WIN32
    Settings.insert("Integration/NoHUP", CSettings::SSetting(false));
#endif
	Settings.insert("Integration/SkipGrabber", CSettings::SSetting(true));

	Settings.insert("Core/Mode", CSettings::SSetting("Unified")); // Unified Separate Local Remote

	/*Settings.insert("Core/BusName", CSettings::SSetting("NeoBus"));
	Settings.insert("Core/BusPort", CSettings::SSetting('NB'));*/

	//Settings.insert("Core/UserName", CSettings::SSetting(""));
	Settings.insert("Core/Password", CSettings::SSetting("*"));
	Settings.insert("Core/TockenTimeOut", CSettings::SSetting(DAY2S(30)));
	Settings.insert("Core/ValidTockens", CSettings::SSetting(""));
	Settings.insert("Core/AutoConnect", CSettings::SSetting(0));

	Settings.insert("Core/LocalName", CSettings::SSetting("NeoCore"));
	Settings.insert("Core/RemotePort", CSettings::SSetting('NC'));
	Settings.insert("Core/RemoteHost", CSettings::SSetting("localhost"));

	Settings.insert("Core/ServiceName", CSettings::SSetting("NeoLoader"));

	Settings.insert("Core/TimeoutSecs", CSettings::SSetting(30));

	Settings.insert("HttpServer/Port", CSettings::SSetting(1600));
	Settings.insert("HttpServer/KeepAlive", CSettings::SSetting(115));
	Settings.insert("HttpServer/TransferBuffer",CSettings::SSetting(KB2B(16),KB2B(4),KB2B(256)));
	Settings.insert("HttpServer/WebUIPath", CSettings::SSetting(""));
	Settings.insert("HttpServer/WebAPIPath", CSettings::SSetting(""));
	Settings.insert("HttpServer/EnableFTP", CSettings::SSetting(false));
	Settings.insert("HttpServer/Whitelist", CSettings::SSetting("https?://link\\.neoloader\\.to/.*"));

#ifndef NO_HOSTERS
	Settings.insert("Debugger/Wnd_Geometry", CSettings::SSetting(""));
	Settings.insert("Debugger/Wnd_State", CSettings::SSetting(""));
	Settings.insert("Debugger/Last_Urls", CSettings::SSetting(""));
	Settings.insert("Debugger/Last_Files", CSettings::SSetting(""));
	Settings.insert("Debugger/Last_Entries", CSettings::SSetting(""));
#endif

	CNeoGUI::DefaultSettings(Settings);

	return Settings;
}
