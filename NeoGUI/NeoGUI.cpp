#include "GlobalHeader.h"
#include "NeoGUI.h"
#include "GUI/CaptchaDialog.h"
#include "GUI/LogView.h"
#include "GUI/SettingsWindow.h"
#include "ModernGUI.h"
#include "SetupWizard.h"
#include "GUI/GrabberWindow.h"
#include "GUI/StatisticsWindow.h"
#ifdef WIN32
#include "./Common/ShellSetup.h"
#endif
#include "../Framework/OtherFunctions.h"
#include <QMenuBar>
#include <QStatusBar>
#include "Connector.h"
#include "GUI/SettingsWidgets.h"
#include "../Framework/Archive/Archive.h"
#include "Common/Common.h"
#include "Common/Dialog.h"

CNeoGUI* theGUI = NULL;

CNeoGUI::CNeoGUI(CSettings* Settings, bool bLocal)
{
	ASSERT(theGUI == NULL);
	theGUI = this;

	m_bLocal = bLocal;

	m_PingTime = 0;

	m_pStatsUpdateJob = NULL;

	m_Settings = Settings;

	m_Manager = new CJobManager(this);
	connect(m_Manager, SIGNAL(SendRequest(QString, QVariantMap)), this, SIGNAL(SendRequest(QString, QVariantMap)));
	
	MakeGUI();

	if(Cfg()->GetInt("Gui/Setup") < 1) // increment if new pages added
	{
		static bool bWizardShown = false;
		if(!bWizardShown)
		{
			bWizardShown = true;
			QTimer::singleShot(100, this, SLOT(ShowWizard()));
		}
	}

	if(Cfg()->GetInt("Integration/SysTray") != 2)
		show();

	m_TimerId = startTimer(100);
}

CNeoGUI::~CNeoGUI()
{
	killTimer(m_TimerId);

	ClearGUI();

	theGUI = NULL;
}

void CNeoGUI::MakeGUI()
{
	LoadLanguage(m_Translation, m_Translator, "NeoGUI");

	// MENUS BEGIN ///////////////////////////////////////////////////////////////////////////////////////////////
	m_pNeoMenu = menuBar()->addMenu(tr("&Neo"));

		m_pConnect = MakeAction(m_pNeoMenu, tr("&Connection Manager"), ":/Icons/Plug");
		connect(m_pConnect, SIGNAL(triggered()), this, SLOT(OnConnector()));
	
		m_pOpenWebUI = MakeAction(m_pNeoMenu, tr("Open &WebUI in Browser"), ":/Icons/Globe");
		connect(m_pOpenWebUI, SIGNAL(triggered()), this, SLOT(OnWebUI()));

		m_pShowSettings = MakeAction(m_pNeoMenu, tr("&Settings"), ":/Icons/Settings");
		connect(m_pShowSettings, SIGNAL(triggered()), this, SLOT(OnSettings()));

		m_pNeoMenu->addSeparator();

		m_pUpLimit = new QSpinBoxEx(this, tr("Default"), " kb/s", true);
		CMenuAction* pUpLimit = new CMenuAction(m_pUpLimit, "Upload Limit:");
		connect(m_pUpLimit, SIGNAL(valueChanged(int)), this, SLOT(OnUpLimit(int)));
		m_pNeoMenu->addAction(pUpLimit);
		m_pDownLimit = new QSpinBoxEx(this, tr("Default"), " kb/s", true);
		CMenuAction* pDownLimit = new CMenuAction(m_pDownLimit, "Download Limit:");
		connect(m_pDownLimit, SIGNAL(valueChanged(int)), this, SLOT(OnDownLimit(int)));
		m_pNeoMenu->addAction(pDownLimit);

		m_pNeoMenu->addSeparator();

#ifndef NO_HOSTERS
		m_pDebugger = MakeAction(m_pNeoMenu, tr("&Debugger"), ":/Icons/Bug");
		connect(m_pDebugger, SIGNAL(triggered()), this, SIGNAL(OpenDebugger()));
#endif

		m_pNeoMenu->addSeparator();

		m_pExit = MakeAction(m_pNeoMenu, tr("E&xit"), ":/Icons/Exit");
		connect(m_pExit, SIGNAL(triggered()), this, SLOT(OnExit()));


	m_pHelpMenu = menuBar()->addMenu(tr("&Help"));

		m_pHelp = MakeAction(m_pHelpMenu, tr("&Help"), ":/Icons/Help");
		connect(m_pHelp, SIGNAL(triggered()), this, SLOT(OnHelp()));

		m_pNews = MakeAction(m_pHelpMenu, tr("&News"), ":/Icons/News");
		connect(m_pNews, SIGNAL(triggered()), this, SLOT(OnNews()));

		m_pWizard = MakeAction(m_pHelpMenu, tr("1st Start &Wizard"), ":/Icons/1stStart");
		connect(m_pWizard, SIGNAL(triggered()), this, SLOT(ShowWizard()));

		m_pUpdate = MakeAction(m_pHelpMenu, tr("Check for Updates"), ":/Icons/Update"); 
		connect(m_pUpdate, SIGNAL(triggered()), this, SIGNAL(CheckUpdates()));

		m_pAbout = MakeAction(m_pHelpMenu, tr("&About"), ":/Icons/About");
		connect(m_pAbout, SIGNAL(triggered()), this, SLOT(OnAbout()));
// MENUS END ///////////////////////////////////////////////////////////////////////////////////////////////

	// Tray BEGIN
	QIcon Icon;
	Icon.addFile(":/Home");
	m_pTrayIcon = new QSystemTrayIcon(Icon, this);
	m_pTrayIcon->setToolTip(tr("NeoLoader"));
	connect(m_pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSysTray(QSystemTrayIcon::ActivationReason)));
	// Tray END

	m_pMainWidget = new QWidget();

	m_pMainLayout = new QVBoxLayout(m_pMainWidget);
	m_pMainLayout->setMargin(0);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter);

	m_pGUI = new CModernGUI();
	m_pSplitter->addWidget(m_pGUI);
	m_pSplitter->setCollapsible(0, false);

	if(Cfg()->GetBool("Gui/ShowLog"))
	{
		m_pLogView = new CLogViewEx(m_pMainWidget);
		m_pLogView->Suspend(false);
		m_pSplitter->addWidget(m_pLogView);
		m_pSplitter->setCollapsible(1, false);
	}
	else
		m_pLogView = NULL;

	m_pTransfer = new QLabel();
	statusBar()->addPermanentWidget(m_pTransfer);

	m_pCaptchaDialog = new CCaptchaDialog();

	m_pSettingsWnd = new CSettingsWindow();

	m_pConnector = NULL;

	m_pConnection = new QLabel();
	statusBar()->addPermanentWidget(m_pConnection);

	m_pMainWidget->setLayout(m_pMainLayout);

	setCentralWidget(m_pMainWidget);

	m_pSplitter->restoreState(Cfg()->GetBlob("Gui/Widget_VSplitter"));
	restoreGeometry(Cfg()->GetBlob("Gui/Dialog_Window"));

	if(Cfg()->GetInt("Integration/SysTray"))
		m_pTrayIcon->show();

}

void CNeoGUI::ClearGUI()
{
	Cfg()->SetBlob("Gui/Widget_VSplitter",m_pSplitter->saveState());
	Cfg()->SetBlob("Gui/Dialog_Window",saveGeometry());

	menuBar()->clear();

	m_pTrayIcon->hide();

	delete m_pMainWidget;

	delete m_pCaptchaDialog;

	delete m_pSettingsWnd;

	if(m_pConnector)
	{
		m_pConnector->hide();
		delete m_pConnector;
	}

	delete m_pConnection;

	delete m_pTransfer;
}

void CNeoGUI::DoReset()
{
	ClearGUI();
	MakeGUI();
}

void CNeoGUI::OnSettings()
{
	m_pSettingsWnd->Open();
}

void CNeoGUI::OnConnector()
{
	if(!m_pConnector)	
		m_pConnector = new CConnector();
	m_pConnector->show();
}

void CNeoGUI::OnWebUI()
{
	if(m_bLocal)
		QDesktopServices::openUrl(QString("http://localhost:%1/WebUI/").arg(Cfg()->GetInt("HttpServer/Port")));
	else
		QDesktopServices::openUrl(QString("http://%1:%2/WebUI/").arg(Cfg()->GetString("Core/RemoteHost")).arg(Cfg()->GetInt("HttpServer/Port")));
}

void CNeoGUI::OnExit()
{
	PromptExit();
}

void CNeoGUI::OnUpLimit(int Limit)
{
	QVariantMap Request;
	Request["UpLimit"] = Limit * 1024;
	SendRequest("SetCore", Request);
}

void CNeoGUI::OnDownLimit(int Limit)
{
	QVariantMap Request;
	Request["DownLimit"] = Limit * 1024;
	SendRequest("SetCore", Request);
}

void CNeoGUI::OnHelp()
{
	QDesktopServices::openUrl(QUrl("http://neoloader.to/?dest=operationmanual"));
}

void CNeoGUI::OnNews()
{
	QDesktopServices::openUrl(QUrl("http://neoloader.to/?dest=projectnews"));
}

void CNeoGUI::OnAbout()
{
	QDesktopServices::openUrl(QUrl("http://neoloader.to/?dest=aboutpage"));
}

class CStatsUpdateJob: public CInterfaceJob
{
public:
	CStatsUpdateJob(CNeoGUI* pView)
	{
		m_pView = pView;
	}

	virtual QString			GetCommand()	{return "GetInfo";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if (m_pView)
		{
			m_pView->m_Data = Response;

			QVariantMap Bandwidth = Response["Bandwidth"].toMap();

			QString Info;
			Info += QString::fromWCharArray(L"↑ %1").arg(FormatSize(Bandwidth["UpLoad"].toInt()) + "/s");
			if (theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
				Info += QString(" (%1)").arg(FormatSize(Bandwidth["UpRate"].toInt()) + "/s");
			Info += " ";
			Info += QString::fromWCharArray(L"↓ %1").arg(FormatSize(Bandwidth["Download"].toInt()) + "/s");
			if (theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
				Info += QString(" (%1)").arg(FormatSize(Bandwidth["DownRate"].toInt()) + "/s");
			m_pView->m_pTransfer->setText(Info);
		}
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pStatsUpdateJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CNeoGUI>m_pView; // Note: this can be deleted at any time
};

void CNeoGUI::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	if(IsConnected())
	{
		if(m_PingTime == 0)
		{
			m_PingTime = GetCurTick();
			QVariantMap Request;
			Request["Ping"] = m_PingTime;
			Request["ID"] = -1;
			SendRequest("Console", Request);
		}
		else 
		{
			uint64 Freeze = GetCurTick() - m_PingTime;
			if(Freeze > SEC2MS(1))
				m_pConnection->setText(tr("Core is Busy: %1 ms\t").arg(Freeze));
		}
	}

	if(m_pStatsUpdateJob == NULL)
	{
		m_pStatsUpdateJob = new CStatsUpdateJob(this);
		ScheduleJob(m_pStatsUpdateJob);
	}
}

class CHosterIconJob: public CInterfaceJob
{
public:
	CHosterIconJob(CNeoGUI* pView, const QString& Name)
	{
		m_pView = pView;
		m_Name = Name;
		m_Request["Action"] = "LoadIcon";
		m_Request["HostName"] = Name;
	}

	virtual QString			GetCommand()	{return "ServiceAction";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
		{
			QByteArray IconData = Response["Icon"].toByteArray();

			QPixmap Pixmap;
			if(!IconData.isEmpty())
				Pixmap.loadFromData(IconData);
			else
				Pixmap.load(":/Icons/Unknown");

			QIcon Icon(Pixmap);
			m_pView->m_Icons.insert(m_Name, Icon);
		}
	}

protected:
	QString m_Name;
	QPointer<CNeoGUI>m_pView; // Note: this can be deleted at any time
};

QIcon CNeoGUI::GetHosterIcon(const QString& Hoster, bool bDefault)	
{
	if(!m_Icons.contains(Hoster))
	{
		CHosterIconJob* pHosterIconJob = new CHosterIconJob(this, Hoster);
		ScheduleJob(pHosterIconJob);

		if(bDefault)
			return QIcon(QPixmap(":/Icons/Unknown"));
		return QIcon();
	}
	return m_Icons[Hoster];
}

void CNeoGUI::UpdatesFound(int Code)
{
	if(Code == 0)
		LogLine(LOG_SUCCESS,tr("NeoLoader ist Up To Date"));
	else if(Code < 0)
		LogLine(LOG_ERROR,tr("Updater Failed!"));
	else if(Code == 2)
		LogLine(LOG_WARNING,tr("Found and Downlaoded NeoLoader Updates."));
	else
		LogLine(LOG_WARNING,tr("Found NeoLoader Updates."));
}

void CNeoGUI::ShowWizard()
{
	CSetupWizard::ShowWizard();
}

void CNeoGUI::Suspend(bool bSet)
{
	m_Manager->Suspend(bSet);
}

void CNeoGUI::ScheduleJob(CInterfaceJob* pJob)
{
	m_Manager->ScheduleJob(pJob);
}

void CNeoGUI::UnScheduleJob(CInterfaceJob* pJob)
{
	m_Manager->UnScheduleJob(pJob);
}

bool CNeoGUI::IsSchedulerBlocking()
{
	return m_Manager->IsBlocking();
}

bool CNeoGUI::IsConnected()
{
	return !m_Manager->IsSuspended();
}

void CNeoGUI::DispatchResponse(const QString& Command, const QVariantMap& vResponse)
{
	QVariantMap Response = vResponse;

	if(Command == "Console" && Response["ID"] == -1)
	{
		m_PingStat.append(GetCurTick() - m_PingTime);
		m_PingTime = 0;
		if(m_PingStat.count() > 10)
			m_PingStat.removeFirst();

		uint64 PingSumm = 0;
		foreach(uint64 PingTime, m_PingStat)
			PingSumm += PingTime;
		PingSumm /= m_PingStat.count();
		m_pConnection->setText(tr("Core Latency: %1 ms\t").arg(PingSumm));
		return;
	}

	QVariantList Log = Response.take("Log").toList();

	m_Manager->DispatchResponse(Command, Response);

	foreach(const QVariant& vLine, Log)
	{
		QVariantMap Line = vLine.toMap();
		if(Line["Flag"].toUInt() & LOG_DEBUG)
			continue;

		/*switch(Line["Flag"].toUInt() & LOG_MASK)
		{
		case LOG_ERROR:		QMessageBox::critical(this, tr("Error"), CLogMsg(Line["Line"]).Print()); break;
		case LOG_WARNING:	QMessageBox::warning(this, tr("Warning"), CLogMsg(Line["Line"]).Print()); break;
		case LOG_SUCCESS:	QMessageBox::question(this, tr("Success"), CLogMsg(Line["Line"]).Print()); break;
		case LOG_INFO:		QMessageBox::information(this, tr("Information"), CLogMsg(Line["Line"]).Print()); break;
		}*/

		static QDialog*	pDialog = NULL;
		static CLogView* pLogView = NULL;
		if(pDialog == NULL)
		{
			pDialog = new QDialog(this);
			QVBoxLayout* pLayout = new QVBoxLayout(pDialog);
			pLayout->setMargin(1);
			pLogView = new CLogView();
			pLayout->addWidget(pLogView);
			QDialogButtonBox* pButtonBox = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, this);
			QObject::connect(pButtonBox, SIGNAL(rejected()), pDialog, SLOT(reject()));
			pLayout->addWidget(pButtonBox);
		}
		if(!pDialog->isVisible())
		{
			pLogView->ClearLog();
			pDialog->show();
		}
		pLogView->AddLog(Line["Flag"].toUInt() & LOG_MASK, CLogMsg(Line["Line"]).Print());
	}
}

void CNeoGUI::LogLine(uint32 uFlag, const QString &sLine)
{
	if(m_pLogView)
		m_pLogView->AddLog(uFlag, QDateTime::fromTime_t(GetTime()).toLocalTime().time().toString() + ": " + sLine);
}

class CExitDialog : public QDialogEx
{
public:
	CExitDialog(bool bLocal, QWidget* parent = 0)
	 : QDialogEx(parent)
	{
		QGridLayout* m_pMainLayout = new QGridLayout(this);
 
		QLabel* pLabel = new QLabel(tr("Do you want to close NeoLoader Application?"));
		m_pMainLayout->addWidget(pLabel, 0, 0, 1, 1);

		if(!bLocal)
		{
			m_pCheck = new QCheckBox(tr("Shutdown the NeoLoader Core?"));
			m_pMainLayout->addWidget(m_pCheck, 1, 0, 1, 1);
		}
		else 
			m_pCheck = NULL;

		m_pButtonBox = new QDialogButtonBox();
		m_pButtonBox->setOrientation(Qt::Horizontal);
		m_pButtonBox->setStandardButtons(QDialogButtonBox::Yes|QDialogButtonBox::No);
		m_pMainLayout->addWidget(m_pButtonBox, 2, 0, 1, 1);
 
		connect(m_pButtonBox,SIGNAL(accepted()),this,SLOT(accept()));
		connect(m_pButtonBox,SIGNAL(rejected()),this,SLOT(reject()));

		m_TimerId = startTimer(1000);
		m_CountDown = 15;
	}
	~CExitDialog()
	{
		killTimer(m_TimerId);
	}

	bool Shutdown()		{return m_pCheck && m_pCheck->isChecked();}

protected:
	void timerEvent(QTimerEvent *e)
	{
		if (e->timerId() != m_TimerId) 
		{
			QDialog::timerEvent(e);
			return;
		}

		if(m_CountDown != 0)
		{
			m_CountDown--;
			m_pButtonBox->button(QDialogButtonBox::Yes)->setText(tr("Yes (%1)").arg(m_CountDown));
			if(m_CountDown == 0)
				accept();
		}
	}

	void reject()
	{
		hide();
	}

	void closeEvent(QCloseEvent *e)
	{
		hide();
		e->ignore();
	}

	int					m_TimerId;
	int					m_CountDown;

	QGridLayout*		m_pMainLayout;
	QCheckBox*			m_pCheck;
	QDialogButtonBox *	m_pButtonBox;
};

class CShutdownJob: public CInterfaceJob
{
public:
	CShutdownJob() {}

	virtual QString			GetCommand()	{return "Shutdown";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		QTimer::singleShot(0, QApplication::instance(), SLOT(quit()));
	}
};

void CNeoGUI::PromptExit()
{
	CExitDialog ExitDialog(m_bLocal);
	if(!ExitDialog.exec())
		return;

	if(ExitDialog.Shutdown())
	{
		ScheduleJob(new CShutdownJob());
		return;
	}

	QApplication::instance()->quit();
}

void CNeoGUI::closeEvent(QCloseEvent *e)
{
	if(m_pTrayIcon->isVisible() && Cfg()->GetInt("Integration/SysTrayTrigger") == 2)
	{
		hide();
		e->ignore();
	}
	else
	{
		PromptExit();
		e->ignore();
	}
}

void CNeoGUI::changeEvent(QEvent* e)
{
    switch (e->type())
    {
        case QEvent::WindowStateChange:
            if (windowState() & Qt::WindowMinimized)
            {
                if (m_pTrayIcon->isVisible() && Cfg()->GetInt("Integration/SysTrayTrigger") == 1)
                    hide();
            }
            break;
        default:
            break;
    }

    QMainWindow::changeEvent(e);
}

void CNeoGUI::OnSysTray(QSystemTrayIcon::ActivationReason Reason)
{
	/*if(Reason == QSystemTrayIcon::Context)
	{
		m_pUpLimit->setDefault(0);
		m_pDownLimit->setDefault(0);
	}*/

	switch(Reason)
	{
		case QSystemTrayIcon::Context:
			if (QDialogEx::GetOpenCount() == 0)
				m_pNeoMenu->popup(QCursor::pos());	
			break;

		case QSystemTrayIcon::DoubleClick:
			if(isHidden())
			{
				setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
				show();
			}
			else
				hide();
			
			break;
	}
}

void CNeoGUI::LoadLanguage(QByteArray& Translation, QTranslator& Translator, const QString& Prefix)
{
	qApp->removeTranslator(&Translator);
	Translation.clear();

	QString Lang = Cfg()->GetString("Gui/Language");
	if(Lang != "en_en")
	{
		QString LangAux = Lang; // Short version as fallback
		LangAux.truncate(LangAux.lastIndexOf('_'));

		QString LangPath = Cfg()->GetAppDir() + "/Languages";
		bool bAux = false;
#ifdef USE_7Z
		if(!QFile::exists(LangPath))
		{
			CArchive Langs(Cfg()->GetAppDir() + "/Language.7z");
			if(Langs.Open())
			{
				int ArcIndex = Langs.FindByPath(QString("/%1_%2.qm").arg(Prefix).arg(Lang));
				if(ArcIndex == -1)
					ArcIndex = Langs.FindByPath(QString("/%1_%2.qm").arg(Prefix).arg(LangAux));
				if(ArcIndex != -1)
				{
					QMap<int, QIODevice*> Files;
					Files.insert(ArcIndex, new QBuffer(&Translation));
					Langs.Extract(&Files);
				}
			}
		}
		else 
#endif
			if(QFile::exists(Lang) || (bAux = QFile::exists(LangAux)))
		{
			QFile File(LangPath + QString("/%1_%2.qm").arg(Prefix).arg(bAux ? LangAux : Lang));
			File.open(QFile::ReadOnly);
			Translation = File.readAll();
		}

		if(!Translation.isEmpty() && Translator.load((const uchar*)Translation.data(), Translation.size()))
			qApp->installTranslator(&Translator);
	}
}

void CNeoGUI::MakeFileIcon(const QString& Ext)
{
	::MakeFileIcon(Ext);
}

void CNeoGUI::DefaultSettings(QMap<QString, CSettings::SSetting>& Settings)
{
	// Gui configuration
	QString defaultLocale = QLocale::system().name().toLower();					// e.g. "de_DE"
	Settings.insert("Gui/Language", CSettings::SSetting(defaultLocale));

	Settings.insert("Gui/Dialog_Window", CSettings::SSetting(""));

	Settings.insert("Gui/Setup", CSettings::SSetting(0));
#ifdef _DEBUG
	Settings.insert("Gui/AdvancedControls", CSettings::SSetting(1));
	Settings.insert("Gui/ShowLog", CSettings::SSetting(true));
	//Settings.insert("Gui/VerboseGraphs", CSettings::SSetting(true));
#else
	Settings.insert("Gui/AdvancedControls", CSettings::SSetting(0));
	Settings.insert("Gui/ShowLog", CSettings::SSetting(false));
	//Settings.insert("Gui/VerboseGraphs", CSettings::SSetting(false));
#endif
	Settings.insert("Gui/AutoSort", CSettings::SSetting("FS"));
	Settings.insert("Gui/SimpleBars", CSettings::SSetting(true));
	Settings.insert("Gui/Alternate", CSettings::SSetting(false));
	Settings.insert("Gui/Widget_Page", CSettings::SSetting(0)); // M
	Settings.insert("Gui/Widget_ShowTabs", CSettings::SSetting(false));
	Settings.insert("Gui/Widget_SubPage", CSettings::SSetting(0)); // M
	Settings.insert("Gui/Widget_Collaps", CSettings::SSetting("")); // M
	Settings.insert("Gui/Widget_VSplitter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_HSplitter", CSettings::SSetting("")); // M
	Settings.insert("Gui/GoToNew", CSettings::SSetting(true));
	Settings.insert("Gui/UnifyGrabber", CSettings::SSetting(true));
	Settings.insert("Gui/SubControl", CSettings::SSetting(true));
	Settings.insert("Gui/OnDblClick", CSettings::SSetting("Open"));

	Settings.insert("Gui/LastFilePath", CSettings::SSetting(""));

	Settings.insert("Gui/DeleteWithSubFiles", CSettings::SSetting(false));
	Settings.insert("Gui/KeepFilesArchived", CSettings::SSetting(true));

	Settings.insert("Gui/InternPlayer", CSettings::SSetting(true));
	Settings.insert("Gui/ExternPlayer", CSettings::SSetting(""));

	Settings.insert("Gui/CaptchaTimeOut", CSettings::SSetting(30));

	Settings.insert("Gui/Widget_FileTab", CSettings::SSetting(0));

	Settings.insert("Gui/Widget_SearchMode", CSettings::SSetting("SmartAgent"));
	Settings.insert("Gui/Widget_SearchType", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_SearchSite", CSettings::SSetting(""));

	Settings.insert("Gui/Widget_Searches_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFCMDbBEAwlmRjQFKf-BAEg3QvnMIDEkU5DYzJNhbACcSQer"));
	Settings.insert("Gui/Widget_Grabber_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFCMDTDVAQkWRjQFKf-BAEg3QvksIDEkU3CwGYNgbAACxwfw"));

	Settings.insert("Gui/Widget_Stats_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_Settings_Page", CSettings::SSetting(-1));
	Settings.insert("Gui/Widget_Settings_Dialog", CSettings::SSetting(0));

	Settings.insert("Gui/Widget_Hosters_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_Hosters_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjAzoDARgqgMSzIyMqHIp_4EASDdC-cxAvAzJgBQEm7EExgYArWcH7g"));

	Settings.insert("Gui/Widget_Servers_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_Servers_Columns", CSettings::SSetting(""));

	Settings.insert("Gui/Widget_Share_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_Share_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFAMIVAB4wQgwcLIiCqX8h8IgHQjlM8CEkMygCAbAAnOCBQ"));

	Settings.insert("Gui/Widget_Properties_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFAMIVABUx2QYGZkRJVL-Q8EQLoRymcG4p8IA1AMa4WxAbDuB-8"));

	Settings.insert("Gui/Widget_FileList_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_FileList_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjAxIDF4kNhMQMwMxCxCzAjEbEHMAMTsQcwExJxBzAzEPRfoEoLpAvBSghhiQKCMjwlkgkPIfCIB0I5QPsu4Vkts9kNiOONgpSOx4JHYyEns9EnsuErsaB9sBOfxgbAAAUw0O"));
	Settings.insert("Gui/Widget_FileList_Detail", CSettings::SSetting(0));

	Settings.insert("Gui/Widget_Download_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_Download_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjAxIDF4kNhMQMwMxCxCzAjEbEHMAMTsQcwExJxBzAzEPRfoEoLpAvBSghhiQKCMjwlkgkPIfCIB0I5QPsu4Vkts9kNiOONgpSOx4JHYyEns9EnsuErsaB9sBOfxgbAAAUw0O"));
	Settings.insert("Gui/Widget_Download_Detail", CSettings::SSetting(0));

	Settings.insert("Gui/Widget_SharedFiles_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_SharedFiles_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjAxIDF4kNhMQMwMxCxCzAjEbEHMAMTsQcwExJxBzAzEPRfoEoLpAvBSghhiQKCMjwlkgkPIfCIB0I5QPsu4Vkts9kNiOONgpSOx4JHYyEns9EnsuErsaB9sBOfxgbAAAUw0O"));
	Settings.insert("Gui/Widget_SharedFiles_Detail", CSettings::SSetting(0));

	Settings.insert("Gui/Widget_FileArchive_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_FileArchive_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjAxIDF4kNhMQMwMxCxCzAjEbEHMAMTsQcwExJxBzAzEPRfoEoLpAvBSghhiQKCMjwlkgkPIfCIB0I5QPsu4Vkts9kNiOONgpSOx4JHYyEns9EnsuErsaB9sBOfxgbAAAUw0O"));
	Settings.insert("Gui/Widget_FileArchive_Detail", CSettings::SSetting(0));

	Settings.insert("Gui/Widget_FilesSearch_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_FilesSearch_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFDMCiJ4kQSZgJgZiFlAkkDMBsQcQMwOxFxAzAnE3EDMQ5E-AaguEC8FqCEGJMrICBaDg5T_QACkG6F8kHWvoBpBwAOJ7YiDnYLEjkdiJyOx1yOx5yKxq3GwHZDYMO8zAAAIDQ0U"));
	Settings.insert("Gui/Widget_FilesSearch_Detail", CSettings::SSetting(0));

	Settings.insert("Gui/Widget_FilesGrabber_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_FilesGrabber_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjAxIDF4kNhMQMwMxCxCzAjEbEHMAMTsQcwExJxBzAzEPRfoEoLpAvBSghhiQKCMjwlkgkPIfCIB0I5QPsu4Vkts9kNiOONgpSOx4JHYyEns9EnsuErsaB9sBOfxgbAAAUw0O"));
	Settings.insert("Gui/Widget_FilesGrabber_Detail", CSettings::SSetting(0));

#ifdef CRAWLER
	Settings.insert("Gui/Widget_Crawler_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_Crawler_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFAMIVAB0x4gwc7IiCqX8h8IgHQjlM8OEkMygCpsAF1QCXY"));
	Settings.insert("Gui/Widget_Crawler_Entries", CSettings::SSetting(""));
#endif

	Settings.insert("Gui/Widget_SubFiles_Spliter", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_SubFiles_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjAxIDF4kNhMQMwMxCxCzAjEbELMDMQcQcwExJxBzAzEPJfoeyCLJRkPZ0VBdKVATUqCqU6A6U6CmAGmWUJApjIwIb4BAyn8gANKNUD7QeUzySH71QGKnILEjkdh7kMOGRHYpKeoBkHsNaA"));
	Settings.insert("Gui/Widget_SubFiles_Detail", CSettings::SSetting(0));

	Settings.insert("Gui/Widget_Trackers_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFCMDTA1AgkWRjQFKf-BAEg3QvksQHkDJFNSkNid2MQB_mUH9g"));
	Settings.insert("Gui/Widget_Hosting_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFCMDTA_ABIsjGgKUv4DAZBuhPJZgPJLkUxpQbAZdZDEu2FsADE6CLY"));
	Settings.insert("Gui/Widget_Ratings_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFCaGcTgRhJkAgkCMQsQswIxOxCzATEnEHMAMRdZ6lmgqkG8FKDiUJAoIyPcGWCQ8h8IgHQjlA-yxgjJrSlI7Fwc4utxiFcjsWuQ2LVI7HQkNjyMAE-sC7o"));
	Settings.insert("Gui/Widget_WebTasks_Columns", CSettings::SSetting(""));

	Settings.insert("Gui/Widget_Transfers_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFCaGcTgRhJkAgkCMQsQswIxOxCzATEnEHMAMRdZ6lmgqkG8FKBiF5AoIyPcGWCQ8h8IgHQjlA-yxgjJrSlI7FwkdigSez0O9dVI7Bokdi0SOxWJDQ8jAD7FC5g"));
	Settings.insert("Gui/Widget_ActiveTransfers_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFCaGcTgRhJkAgkCMQsQswIxOxCzATEnEHMAMRdZ6lmgqkG8FKDiWJAoIyPcGWCQ8h8IgHQjlA-yxgjJrSlI7Fwc4utxiFcjsWuQ2LVI7HwkNjyMAFWkC8o"));
	Settings.insert("Gui/Widget_ActiveDownload_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFDMDiK4kQSZgJgZiFmAmBWkAIjZgJgTiDmAmIss9SxQ1SBeClBxLEiUkREsBgcp_4EASDdC-SBrjKAaGSAa4excHOLrcYhXI7FrkNi1SOx8JDbMewwAW00Lzw"));
	Settings.insert("Gui/Widget_ActiveUploads_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFDMBiK4kQSZgJgZiFmAmBWI2UGKgJgTiDmAmIss9SxQ1SBeClBxLEiUkREsBgcp_4EASDdC-SBrjKAaGSAa4excHOLrcYhXI7FrkNi1SOx8JDbMewwAWiwLzg"));
	Settings.insert("Gui/Widget_Clients_Columns", CSettings::SSetting(":PackedArray:eNpjYGD4zwABjFCaBcTgRhJkAmJmkAQQswIxOxCzATEnEHMAMRc56hXYoSqAokxhEBnGm1DZFKhOIM2aDFLNyAh3Hhik_AcCIN0I5QOtZzyH5IcnSOxcJPYkJPZSJDay_6txqGcgzGZ-AQB9mw6W"));


	Settings.insert("Gui/Widget_Captcha_Dialog", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_Upload_Dialog", CSettings::SSetting(""));
	Settings.insert("Gui/Widget_Upload_Hosters", CSettings::SSetting(""));
}