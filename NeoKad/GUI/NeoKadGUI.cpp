#include "GlobalHeader.h"
#include "NeoKadGUI.h"
#include "../NeoKad.h"
#include "../../Framework/Settings.h"
#include "../Kad/KadHeader.h"
#include "../Kad/Kademlia.h"
#include "../Kad/KadHandler.h"
#include "../Kad/RoutingRoot.h"
#include "../Kad/KadNode.h"
#include "../Kad/FirewallHandler.h"
#include "../Kad/KadEngine/KadEngine.h"
#include "LookupWidget.h"
#include "IndexWidget.h"
#include "RoutingWidget.h"
#include "SearchList.h"
#include "SearchView.h"
#include "./ScriptDebugger/KadScriptDebuggerBackend.h"

#include "../Common/v8Engine/JSEngine.h"

CKadScriptDebuggerThread* g_pDebugger = NULL;

#if QT_VERSION < 0x050000
CNeoKadGUI::CNeoKadGUI(QWidget *parent, Qt::WFlags flags)
#else
CNeoKadGUI::CNeoKadGUI(QWidget *parent, Qt::WindowFlags flags)
#endif
 : QMainWindow(parent, flags)
{
	connect(theKad, SIGNAL(ShowGUI()), this, SLOT(OnShowGUI()));

	QString Title = "NeoKad v" + QString::number(KAD_VERSION_MJR) + "." + QString::number(KAD_VERSION_MIN).rightJustified(2, '0');
#if KAD_VERSION_UPD > 0
	Title.append('a' + KAD_VERSION_UPD - 1);
#endif
#ifdef _DEBUG
	Title += " - DEBUG";
	//Title += " (" __DATE__ ")";
#endif
	setWindowTitle(Title);

	m_uTimerCounter = 0;
	m_uTimerID = startTimer(10);

	m_uLastLog = 0;

	m_pMainWidget = new QWidget(this);
	setCentralWidget(m_pMainWidget);

	m_pKadMenu = menuBar()->addMenu(tr("&Kad"));

		m_pConnect = new QAction(tr("&Connect"), this);
		connect(m_pConnect, SIGNAL(triggered()), this, SLOT(OnConnect()));
		m_pKadMenu->addAction(m_pConnect);

		m_pDisconnect = new QAction(tr("&Disconnect"), this);
		connect(m_pDisconnect, SIGNAL(triggered()), this, SLOT(OnDisconnect()));
		m_pKadMenu->addAction(m_pDisconnect);

		m_pKadMenu->addSeparator();

		m_pBootstrap = new QAction(tr("&Bootstrap"), this);
		connect(m_pBootstrap, SIGNAL(triggered()), this, SLOT(OnBootstrap()));
		m_pKadMenu->addAction(m_pBootstrap);

	/*m_pViewMenu = menuBar()->addMenu(tr("&View"));

		m_pShowAll = new QAction(tr("&Show All Nodes"), this);
		m_pShowAll->setCheckable(true);
		m_pViewMenu->addAction(m_pShowAll);*/

	m_pDevMenu = menuBar()->addMenu(tr("&Dev"));

		m_pConsolidate = new QAction(tr("&Consolidate Routing Table"), this);
		connect(m_pConsolidate, SIGNAL(triggered()), this, SLOT(OnConsolidate()));
		m_pDevMenu->addAction(m_pConsolidate);

		/*m_pAddNode = new QAction(tr("Add &Node"), this);
		connect(m_pAddNode, SIGNAL(triggered()), this, SLOT(OnAddNode()));
		m_pDevMenu->addAction(m_pAddNode);*/

		m_pDevMenu->addSeparator();

		m_pTerminate = new QAction(tr("&Reset JS Engine"), this);
		connect(m_pTerminate, SIGNAL(triggered()), this, SLOT(OnTerminate()));
		m_pDevMenu->addAction(m_pTerminate);

		m_pDebugger = new QAction(tr("Script &Debugger"), this);
		connect(m_pDebugger, SIGNAL(triggered()), this, SLOT(OnDebugger()));
		m_pDevMenu->addAction(m_pDebugger);


		m_pAuthenticate = new QAction(tr("&Authenticate Scripts"), this);
		connect(m_pAuthenticate, SIGNAL(triggered()), this, SLOT(OnAuthenticate()));
		m_pDevMenu->addAction(m_pAuthenticate);

	m_pHelpMenu = menuBar()->addMenu(tr("&Help"));

		m_pAbout = new QAction(tr("&About"), this);
		connect(m_pAbout, SIGNAL(triggered()), this, SLOT(OnAbout()));
		m_pHelpMenu->addAction(m_pAbout);

	// log Begin
		m_pLogTab = new QTabWidget(NULL);

		QIcon logIcon;
		logIcon.addFile(":/Log");
		m_pUserLog = new QTextEdit(m_pLogTab);
		//m_pUserLog->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		m_pUserLog->setUndoRedoEnabled(false);
		m_pLogTab->addTab(m_pUserLog, logIcon, tr("Log"));

		QIcon debugLogIcon;
		debugLogIcon.addFile(":/DebugLog");
		m_pDebugLog = new QTextEdit(m_pLogTab);
		//m_pDebugLog->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		m_pDebugLog->setUndoRedoEnabled(false);
		m_pLogTab->addTab(m_pDebugLog, debugLogIcon, tr("Debug"));

#ifdef _DEBUG
		m_pLogTab->setCurrentWidget(m_pDebugLog);
#endif
	// logEnd

		m_pRoutingWidget = new CRoutingWidget();
		m_pSearchView = new CSearchView();
		m_pSearchList = new CSearchList();
		m_pLookupWidget = new CLookupWidget();
		m_pIndexWidget = new CIndexWidget();
		connect(m_pSearchList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), m_pLookupWidget, SLOT(OnLoadLookup(QTreeWidgetItem*, int)));

		m_pKadTab = new QTabWidget(NULL);
		connect(m_pKadTab, SIGNAL(currentChanged(int)), this, SLOT(OnTab(int)));
		m_pKadTab->addTab(m_pRoutingWidget, tr("Routign Tree"));
		m_pKadTab->addTab(m_pSearchView, tr("Routign Graph"));
		m_pKadTab->addTab(m_pLookupWidget, tr("Manual Lookup"));
		m_pKadTab->addTab(m_pIndexWidget, tr("Local Index"));
		

	m_pSplitter = new QSplitter(m_pMainWidget);
	m_pSplitter->setOrientation(Qt::Vertical);

	m_pSplitter->addWidget(m_pKadTab);
	m_pSplitter->addWidget(m_pSearchList);
	m_pSplitter->addWidget(m_pLogTab);

	m_pMainLayout = new QHBoxLayout(m_pMainWidget);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	m_pMainLayout->addWidget(m_pSplitter);
	m_pMainWidget->setLayout(m_pMainLayout);

	QStatusBar* pStatusBar = statusBar();
	m_pNode = new QLabel(this);
	pStatusBar->addPermanentWidget(m_pNode, 200);
	m_pAddress = new QLabel(this);
	pStatusBar->addPermanentWidget(m_pAddress, 200);
	m_pNetwork = new QLabel(this);
	pStatusBar->addPermanentWidget(m_pNetwork, 200);

	restoreGeometry(theKad->Cfg()->GetBlob("Gui/Window"));
	m_pSplitter->restoreState(theKad->Cfg()->GetBlob("Gui/Main"));

#ifdef _DEBUG
	g_pDebugger = new CKadScriptDebuggerThread("KadDebug", this);
#endif
}

CNeoKadGUI::~CNeoKadGUI()
{
	killTimer(m_uTimerID);

	theKad->Cfg()->SetBlob("Gui/Window",saveGeometry());
	theKad->Cfg()->SetBlob("Gui/Main",m_pSplitter->saveState());
}

void CNeoKadGUI::Process()
{
	m_uTimerCounter++;

	if(!isVisible())
		return;

	if(m_uTimerCounter % 50 == 0) // 2 times a second
	{
		GetLogLines();

		QVariant ID = m_pSearchList->GetCurID();
		if(ID.isValid())
		{
			m_pLookupWidget->SetCurID(ID);
			m_pSearchView->DumpSearch(ID, m_pLookupWidget->GetLookups());
		}
		m_pSearchList->DumpSearchList(m_pLookupWidget->GetLookups());

		CUInt128 tID = theKad->Kad()->GetID();
		
		m_pNode->setText("KadID: " + QString::fromStdWString(tID.ToHex()));

		QString Addresses;
		if(CFirewallHandler* pFirewallHandler = theKad->Kad()->GetChild<CFirewallHandler>())
			Addresses = ListAddr(pFirewallHandler->AddrPool());
		m_pAddress->setText(Addresses);

		QString Network;
		if(CRoutingRoot* pRoot = theKad->Kad()->GetChild<CRoutingRoot>())
		{
			Network.append(tr("NodeCount: %1\t").arg(pRoot->GetNodeCount()));
			CUInt128 tDist = pRoot->GetMaxDistance();
			m_pNode->setToolTip("Max Distance:" + QString::fromStdWString(tDist.ToBin()));
		}
		if(CKadEngine* pEngine = theKad->Kad()->GetChild<CKadEngine>())
		{
			QString Size;
			if(pEngine->GetTotalMemory() > (1024*1024))
				Size = QString::number(double(pEngine->GetTotalMemory())/(1024*1024), 'f', 2) + " Mb";
			else if(pEngine->GetTotalMemory() > (1024))
				Size = QString::number(double(pEngine->GetTotalMemory())/(1024), 'f', 2) + " Kb";
			else
				Size = QString::number(pEngine->GetTotalMemory()) + " b";
			Network.append(tr("EngineMemory: %1\t").arg(Size));
		}
		m_pNetwork->setText(Network);

		if(m_uTimerCounter % 200 == 0) // every 2 seconds
		{
			if(m_pKadTab->currentWidget() == m_pRoutingWidget)
				m_pRoutingWidget->DumpRoutignTable();

			m_uTimerCounter = 0;
		}
	}
}

void CNeoKadGUI::closeEvent(QCloseEvent *e)
{
	if(theKad->IsEmbedded())
	{
		e->ignore();
		hide();
	}
}

void CNeoKadGUI::OnShowGUI()
{
	show();
}

void CNeoKadGUI::OnTab(int Index)
{
	if(Index == m_pKadTab->indexOf(m_pIndexWidget))
		m_pIndexWidget->DumpLocalIndex();
}

void CNeoKadGUI::OnConnect()
{
	theKad->Connect();
}

void CNeoKadGUI::OnDisconnect()
{
	theKad->Disconnect();

	m_pRoutingWidget->ClearRoutignTable();
	m_pSearchList->ClearSearchList();
}

void CNeoKadGUI::OnBootstrap()
{
	if(CRoutingRoot* pRoot = theKad->Kad()->GetChild<CRoutingRoot>())
	{
		QString Bootstrap = QInputDialog::getText(this, tr("bootstrap kad"),tr("KadID@IP:Port") + QString(100,' '));
		QString sID;
		QString Addr;
		if(Bootstrap.contains("://"))
		{
			QUrl BootUrl(Bootstrap);
			sID = BootUrl.userName();
			Addr = QString("%1://%2:%3").arg(BootUrl.scheme()).arg(BootUrl.host()).arg(BootUrl.port());
		}
		else
		{
			StrPair IDAddr = Split2(Bootstrap, "@");
			sID = IDAddr.first;
			Addr = "utp://" + IDAddr.second;
		}

		CUInt128 ID;
		if(ID.FromHex(sID.toStdWString()))
		{
			CSafeAddress Address(Addr.toStdWString());
			if(Address.IsValid())
			{
				CPointer<CKadNode> pNode = new CKadNode();
				memcpy(pNode->GetID().GetData(),ID.GetData(),ID.GetSize());
				pNode->UpdateAddress(Address);
				pRoot->AddNode(pNode);
				CKadHandler* pHandler = theKad->Kad()->GetChild<CKadHandler>();
				CComChannel* pChannel = pHandler->PrepareChannel(pNode);
				if(pChannel)
					pHandler->SendNodeReq(pNode, pChannel, CUInt128());
			}
		}
	}
}

void CNeoKadGUI::OnConsolidate()
{
	CRoutingRoot* pRoot = theKad->Kad()->GetChild<CRoutingRoot>();
	if(!pRoot)
		return;
	for(;;)
	{
		size_t NodeCount = pRoot->GetNodeCount();
		size_t NewCount = pRoot->Consolidate(true);
		if(NewCount < NodeCount)
			continue;
		break;
	}
}

/*void CNeoKadGUI::OnAddNode()
{
}*/

void CNeoKadGUI::OnTerminate()
{
	theKad->Kad()->GetChild<CKadEngine>()->HardReset();
}

void CNeoKadGUI::OnDebugger()
{
	if(!g_pDebugger)
		//g_pDebugger = new CKadScriptDebuggerThread(":9222", this);
		g_pDebugger = new CKadScriptDebuggerThread("KadDebug_" + GetRand64Str(), this);

	QStringList Args("-debug");
	Args.append(g_pDebugger->Pipe());

#ifndef WIN32
	QProcess::startDetached(theKad->Cfg()->GetAppDir() + "/NeoKad", Args);
#else
	QProcess::startDetached(theKad->Cfg()->GetAppDir() + "/NeoKad.exe", Args);
#endif
}

void CNeoKadGUI::OnAuthenticate()
{
	QString ScriptPath = QFileDialog::getExistingDirectory(this, tr("Select Script Directory"));
	if(ScriptPath.isEmpty())
		return;

	theKad->Authenticate(ScriptPath);
}

void CNeoKadGUI::OnAbout()
{
	//QMessageBox::about(this, tr("Neo Kad"), tr("http://neoloader.com"));

#ifdef Q_WS_MAC
    static QPointer<QMessageBox> oldMsgBox;

    if (oldMsgBox) {
        oldMsgBox->show();
        oldMsgBox->raise();
        oldMsgBox->activateWindow();
        return;
    }
#endif

	QString Version = QString::number(KAD_VERSION_MJR) + "." + QString::number(KAD_VERSION_MIN).rightJustified(2, '0');
#if KAD_VERSION_UPD > 0
	Version.append('a' + KAD_VERSION_UPD - 1);
#endif

    QString AboutCaption = tr(
        "<h3>About NeoKad</h3>"
        "<p>Version %1</p>"
        ).arg(Version);
    QString AboutText = tr(
        "<p>NeoKad is a generic Kademlia platform application development.</p>"
        "<p>See <a href=\"http://neoloader.com\">neoloader.com</a> for more information.</p>"
        );
    QMessageBox *msgBox = new QMessageBox(this);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowTitle(tr("About NeoKad"));
    msgBox->setText(AboutCaption);
    msgBox->setInformativeText(AboutText);

	QIcon ico(QLatin1String(":/Globe"));
	msgBox->setIconPixmap(ico.pixmap(128,128));
#if defined(Q_WS_WINCE)
    msgBox->setDefaultButton(msgBox->addButton(QMessageBox::Ok));
#endif

    // should perhaps be a style hint
#ifdef Q_WS_MAC
    oldMsgBox = msgBox;
    msgBox->show();
#else
    msgBox->exec();
#endif
}

void AddLog(QTextEdit *pLog, uint32 uFlag, const QString &Pack, int Limit)
{
	QColor Color;
	switch(uFlag & LOG_MASK)
	{
		case LOG_ERROR:		Color = Qt::red;		break;
		case LOG_WARNING:	Color = Qt::darkYellow;	break;
		case LOG_SUCCESS:	Color = Qt::darkGreen;	break;
		case LOG_INFO:		Color = Qt::blue;		break;
		default:			Color = Qt::black;		break;
	}

	QTextDocument* Document = pLog->document();
	QTextCursor Cursor = pLog->textCursor();
	Cursor.movePosition(QTextCursor::Start);
	for(int i=Document->lineCount();i > Limit;i--)
		Cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
	Cursor.removeSelectedText();
	Cursor.movePosition(QTextCursor::End);
	pLog->setTextCursor(Cursor);

	pLog->setTextColor(Color);
	pLog->append(Pack);
}

void CNeoKadGUI::AddLog(uint32 uFlag, const QString &Pack)
{
	int Limit = theKad->Cfg()->GetInt("Gui/LogLength");
	::AddLog(m_pDebugLog, uFlag, Pack, Limit);
	if ((uFlag & LOG_DEBUG) == 0)  
		::AddLog(m_pUserLog, uFlag, Pack, Limit);
}

void CNeoKadGUI::GetLogLines()
{
	QList<CLog::SLine> Log = theKad->GetLog();

	int Index = 0;
	if(uint64 uLast = m_uLastLog)
	{
		for(int i=0;i < Log.count(); i++)
		{
			if(uLast == Log.at(i).uID)
			{
				Index = i+1;
				break;
			}
		}
	}

	if(Index >= Log.count())
		return;

	int Flags = Log.at(Index).uFlag;
	QString Pack = QDateTime::fromTime_t((time_t)Log.at(Index).uStamp).toLocalTime().time().toString() + ": " + Log.at(Index).Line.Print();
	for(Index++; Index < Log.count(); Index++)
	{
		if(Flags != Log.at(Index).uFlag)
		{
			AddLog(Flags, Pack);
			Pack.clear();
			Flags = Log.at(Index).uFlag;
		}
		if(!Pack.isEmpty())
			Pack += "\n";
		Pack += QDateTime::fromTime_t((time_t)Log.at(Index).uStamp).toLocalTime().time().toString() + ": " + Log.at(Index).Line.Print();
	}
	AddLog(Flags, Pack);
	m_uLastLog = Log.at(Index-1).uID;
}
