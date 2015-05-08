#include "GlobalHeader.h"
#include "NeoGUI.h"
#include "ModernGUI.h"
#include "GUI/NeoSummary.h"
#include "GUI/SearchWindow.h"
#include "GUI/GrabberWindow.h"
#ifdef CRAWLER
#include "GUI/CrawlerWindow.h"
#endif
#include "GUI/StatisticsWindow.h"
#include "GUI/ServicesWidget.h"
#include "GUI/SettingsWindow.h"
#include "GUI/CaptchaDialog.h"
#include "GUI/LogView.h"
#include "GUI/FileListWidget.h"
#include "GUI/FileListView.h"
#include "GUI/TransfersView.h"
#include "GUI/PageTreeWidget.h"
#include "Common/MultiLineInput.h"
#include "GUI/OnlineSearch/OnlineCompleter.h"
#include "GUI/OnlineSearch/OnlineFileList.h"
#include "GUI/ServiceSummary.h"
#include "GUI/P2PServers.h"
#include "GUI/WebTaskView.h"
#include "Common/Common.h"

QString m_PlayerArgs = "TimeOut=10000";

CModernGUI::CModernGUI(QWidget* parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(0);

	if(theGUI->Cfg()->GetBool("Gui/InternPlayer"))
	{
#ifdef WIN32
        CreatePlayer = (CreatePlayerFkt)QLibrary::resolve("MediaPlayer","CreatePlayer");
#elif __APPLE__
		CreatePlayer = NULL; // https://bugreports.qt-project.org/browse/QTBUG-31843
		//CreatePlayer = (CreatePlayerFkt)QLibrary::resolve(QApplication::applicationDirPath() + "/libMediaPlayer.1.dylib","CreatePlayer");
#else
        CreatePlayer = (CreatePlayerFkt)QLibrary::resolve(QApplication::applicationDirPath() + "/libMediaPlayer.so.1","CreatePlayer");
#endif
    }
	else
		CreatePlayer = NULL;

	m_pToolBar = new QToolBar();

	m_pAddLinks = MakeAction(m_pToolBar, ":/Icons/Links", tr("Add Links"));
	connect(m_pAddLinks, SIGNAL(triggered()), this, SLOT(OnAddLinks()));

	m_pAddFile = MakeAction(m_pToolBar, ":/Icons/Files", tr("Add Torrent/Container"));
	connect(m_pAddFile, SIGNAL(triggered()), this, SLOT(OnAddFile()));

	m_pToolBar->addSeparator();

	m_pScanShare = MakeAction(m_pToolBar, ":/Icons/Rescan", tr("Scan Shared Files"));
	connect(m_pScanShare, SIGNAL(triggered()), this, SLOT(OnScanShare()));

	m_pClearCompleted = MakeAction(m_pToolBar, ":/Icons/Cleanup", tr("Clear"));
	connect(m_pClearCompleted, SIGNAL(triggered()), this, SLOT(OnClearCompleted()));

	m_pToolBar->addSeparator();

	m_pDetailTabs = MakeAction(m_pToolBar, ":/Icons/Tabs", tr("Details Tabs"));
	m_pDetailTabs->setCheckable(true);
	connect(m_pDetailTabs, SIGNAL(triggered()), this, SLOT(OnDetailTabs()));

	m_pToolBar->addSeparator();

	m_pSettings = MakeAction(m_pToolBar, ":/Icons/Config", tr("Settings Window"));
	connect(m_pSettings, SIGNAL(triggered()), theGUI, SLOT(OnSettings()));

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	m_pSearchExp = new QLineEdit();
	m_pSearchExp->setMinimumWidth(200);
	connect(m_pSearchExp, SIGNAL(returnPressed()), this, SLOT(OnSearch()));
	m_pToolBar->addWidget(m_pSearchExp);

//#ifndef _DEBUG
//	COnlineCompleter* pOnlineCompleter = new COnlineCompleter();
//	m_pSearchExp->setCompleter(pOnlineCompleter);
//	connect(m_pSearchExp, SIGNAL(textEdited(const QString&)), pOnlineCompleter, SLOT(OnUpdate(const QString&)));
//#endif

	m_pSearchBtn = new QToolButton(m_pToolBar);
	m_pSearchBtn->setIcon(QIcon(QPixmap::fromImage(QImage(":/Icons/Search"))));
	m_pSearchBtn->setText(tr("Start Search"));
	connect(m_pSearchBtn, SIGNAL(released()), this, SLOT(OnSearch()));

	m_pSearchMenu = new QMenu();

	m_pDownload = new QActionGroup(m_pSearchMenu);
	m_pAll = new QAction(tr("All Downloads"), m_pSearchMenu);
	m_pAll->setCheckable(true);
	m_pAll->setActionGroup(m_pDownload);
	m_pSearchMenu->addAction(m_pAll);
	m_pNeo = new QAction(tr("NeoLoader"), m_pSearchMenu);
	m_pNeo->setCheckable(true);
	m_pNeo->setActionGroup(m_pDownload);
	m_pSearchMenu->addAction(m_pNeo);
	m_pTorrent = new QAction(tr("BitTorrent"), m_pSearchMenu);
	m_pTorrent->setCheckable(true);
	m_pTorrent->setActionGroup(m_pDownload);
	m_pSearchMenu->addAction(m_pTorrent);
	m_pEd2k = new QAction(tr("eMule/Ed2k"), m_pSearchMenu);
	m_pEd2k->setCheckable(true);
	m_pEd2k->setActionGroup(m_pDownload);
	m_pSearchMenu->addAction(m_pEd2k);
	m_pLinks = new QAction(tr("Hoster Downloads"), m_pSearchMenu);
	m_pLinks->setCheckable(true);
	m_pLinks->setActionGroup(m_pDownload);
	m_pSearchMenu->addAction(m_pLinks);
	connect(m_pDownload, SIGNAL(triggered(QAction*)), this, SLOT(OnSearchNet(QAction*)));

	m_pSearchBtn->setPopupMode(QToolButton::MenuButtonPopup);
	m_pSearchBtn->setMenu(m_pSearchMenu);

	m_pToolBar->addWidget(m_pSearchBtn);

	m_pMainLayout->addWidget(m_pToolBar);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Horizontal);
	m_pMainLayout->addWidget(m_pSplitter);

	m_pPageTree = new CPageTreeWidget(CreatePlayer != NULL);
	connect(m_pPageTree, SIGNAL(PageChanged(int, uint64)), this, SLOT(OnPageChanged(int, uint64)));
	connect(m_pPageTree, SIGNAL(PageClosed(int, uint64)), this, SLOT(OnPageClosed(int, uint64)));
	m_pSplitter->addWidget(m_pPageTree);


	m_pSplitter->setStretchFactor(0, 0);
	m_pSplitter->setStretchFactor(1, 1);
	m_pSplitter->setCollapsible(0, false);
	m_pSplitter->setCollapsible(1, false);

	// view stack
	m_pStack = new QStackedWidget(this);

	m_pSummaryWnd = new CNeoSummary();
	m_pStack->addWidget(m_pSummaryWnd);

	m_pFileListWnd = new CFileListWidget();
	m_pFileListWnd->GetFileList()->AppendMenu(m_pDetailTabs);
	connect(m_pFileListWnd, SIGNAL(StreamFile(uint64, const QString&, bool)), this, SLOT(OnStreamFile(uint64, const QString&, bool)));
	connect(m_pFileListWnd, SIGNAL(TogleDetails()), this, SLOT(OnTogleDetails()));
	m_pStack->addWidget(m_pFileListWnd);

	m_pTransferListWnd = new CTransfersView(CTransfersView::eActive);
	m_pStack->addWidget(m_pTransferListWnd);

	m_pWebTaskViewWnd = new CWebTaskView();
	m_pStack->addWidget(m_pWebTaskViewWnd);

	m_pPlayerWnd = NULL;

	m_pSearchWnd = new CSearchWindow();
	connect(m_pSearchWnd, SIGNAL(OnLine(const QString&)), this, SLOT(OnOnLine(const QString&)));
	m_pStack->addWidget(m_pSearchWnd);

	if(theGUI->Cfg()->GetBool("Gui/UnifyGrabber"))
		m_pGrabberWnd = NULL;
	else
	{
		m_pGrabberWnd = new CGrabberWindow();
		m_pStack->addWidget(m_pGrabberWnd);
	}

#ifdef CRAWLER
	m_pCrawlerWnd = new CCrawlerWindow();
	m_pStack->addWidget(m_pCrawlerWnd);
#endif

	
	m_pServiceWnd = new CServiceSummary();
	m_pStack->addWidget(m_pServiceWnd);

	m_pServicesWnd = new CServicesWidget();
	m_pStack->addWidget(m_pServicesWnd);

	m_pServersWnd = new CP2PServers();
	m_pStack->addWidget(m_pServersWnd);

	m_pStatisticsWnd = new CStatisticsWindow();
	m_pStack->addWidget(m_pStatisticsWnd);

	//m_pStack->addWidget(m_pSettingsWnd);
	//

	m_pSplitter->addWidget(m_pStack);

	setLayout(m_pMainLayout);

	m_pPageTree->SetPage(theGUI->Cfg()->GetInt("Gui/Widget_Page"), theGUI->Cfg()->GetInt("Gui/Widget_SubPage"));

	m_pSplitter->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_HSplitter"));

	QString Type = theGUI->Cfg()->GetString("Gui/Widget_SearchType");


	if(Type == "neo")
        m_pNeo->setChecked(true);
	else if(Type == "btih")
        m_pTorrent->setChecked(true);
	else if(Type == "arch")
		m_pLinks->setChecked(true);
	else if(Type == "ed2k")
		m_pEd2k->setChecked(true);
	else
		m_pAll->setChecked(true);

	m_pDetailTabs->setChecked(theGUI->Cfg()->GetBool("Gui/Widget_ShowTabs"));
	OnDetailTabs();

	OnSearchNet(NULL);
}

CModernGUI::~CModernGUI()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_HSplitter",m_pSplitter->saveState());

	theGUI->Cfg()->setValue("Gui/Widget_ShowTabs", m_pDetailTabs->isChecked());

	delete m_pSummaryWnd;
	delete m_pFileListWnd;
	delete m_pPlayerWnd;
	delete m_pSearchWnd;
	delete m_pGrabberWnd;
#ifdef CRAWLER
	delete m_pCrawlerWnd;
#endif
	delete m_pServiceWnd;
	delete m_pServicesWnd;
	delete m_pServersWnd;
	delete m_pStatisticsWnd;
}

void CModernGUI::OnPageChanged(int PageID, uint64 TabID)
{
	theGUI->Cfg()->SetSetting("Gui/Widget_Page", PageID);

	switch(PageID)
	{
	case CPageTreeWidget::eSummary:			m_pStack->setCurrentWidget(m_pSummaryWnd);		break;
	case CPageTreeWidget::eDownloads:
	case CPageTreeWidget::eShared:
		{
											theGUI->Cfg()->SetSetting("Gui/Widget_SubPage", TabID);

											CFileListView::ESubMode Sub = CFileListView::eAll;
											switch(TabID)
											{
											case CPageTreeWidget::eStarted:		Sub = CFileListView::eStarted;		break;
											case CPageTreeWidget::ePaused:		Sub = CFileListView::ePaused;		break;
											case CPageTreeWidget::eStopped:		Sub = CFileListView::eStopped;		break;
											case CPageTreeWidget::eCompleted:	Sub = CFileListView::eCompleted;	break;
											}

											if(PageID == CPageTreeWidget::eDownloads)
												m_pFileListWnd->ChangeMode(CFileListView::eDownload, Sub);
											else
												m_pFileListWnd->ChangeMode(CFileListView::eSharedFiles, Sub);

											m_pStack->setCurrentWidget(m_pFileListWnd);	
		} break;
	case CPageTreeWidget::eSearches:		if(TabID)
											{
												m_pFileListWnd->ChangeMode(CFileListView::eFilesSearch);
												m_pStack->setCurrentWidget(m_pFileListWnd);
												m_pFileListWnd->SetID(TabID);
											}
											else
												m_pStack->setCurrentWidget(m_pSearchWnd);	break;
	case CPageTreeWidget::eOnLine:			m_pStack->addWidget((QWidget*)TabID);
											m_pStack->setCurrentWidget((QWidget*)TabID);	break;
	case CPageTreeWidget::eGrabber:	
											if(theGUI->Cfg()->GetBool("Gui/UnifyGrabber"))
											{
												m_pFileListWnd->ChangeMode(CFileListView::eFilesGrabber);
												m_pFileListWnd->SetID(0);
												m_pStack->setCurrentWidget(m_pFileListWnd);
											}
											else if(TabID)
											{
												m_pFileListWnd->ChangeMode(CFileListView::eFilesGrabber);
												m_pStack->setCurrentWidget(m_pFileListWnd);		
												m_pFileListWnd->SetID(TabID);
											}
											else
												m_pStack->setCurrentWidget(m_pGrabberWnd);	break;
	case CPageTreeWidget::eArchive:			m_pFileListWnd->ChangeMode(CFileListView::eFileArchive);
											m_pStack->setCurrentWidget(m_pFileListWnd);		break;

	case CPageTreeWidget::eTransfers:
		{
											theGUI->Cfg()->SetSetting("Gui/Widget_SubPage", TabID);

											CTransfersView::EMode Sub = CTransfersView::eActive;
											switch(TabID)
											{
											case CPageTreeWidget::eDownloads:	Sub = CTransfersView::eDownloads;	break;
											case CPageTreeWidget::eUploads:		Sub = CTransfersView::eUploads;		break;
											}

											m_pTransferListWnd->ChangeMode(Sub);
											m_pStack->setCurrentWidget(m_pTransferListWnd);
		} break;
	case CPageTreeWidget::eClients:			m_pTransferListWnd->ChangeMode(CTransfersView::eClients);
											m_pStack->setCurrentWidget(m_pTransferListWnd); break;
	case CPageTreeWidget::eWebTasks:		m_pStack->setCurrentWidget(m_pWebTaskViewWnd);	break;
	case CPageTreeWidget::ePlayer:			if(!CreatePlayer)								break;
											if(!m_pPlayerWnd) // setup the palyer only if its needed, it takes up 20 MB of RAM
                                            {
                                                m_pPlayerWnd = CreatePlayer();
                                                m_pStack->addWidget(m_pPlayerWnd);
											}
											m_pStack->setCurrentWidget(m_pPlayerWnd);
											if(TabID)
											{
												QString Args;
												if(theGUI->IsLocal())
													Args = QString("http://localhost:%1/Repository/Data/id:%2/?%3").arg(theGUI->Cfg()->GetString("HttpServer/Port")).arg(TabID).arg(m_PlayerArgs);
												else
													Args = QString("http://%1:%2/Repository/Data/id:%3/?%4").arg(theGUI->Cfg()->GetString("Core/RemoteHost")).arg(theGUI->Cfg()->GetString("HttpServer/Port")).arg(TabID).arg(m_PlayerArgs);

												QMetaObject::invokeMethod(m_pPlayerWnd, "openUrl", Qt::AutoConnection, Q_ARG(QString, m_pPageTree->GetPlayName(TabID)), Q_ARG(QString, Args));
											}												break;
#ifdef CRAWLER
	case CPageTreeWidget::eCrawler:			m_pStack->setCurrentWidget(m_pCrawlerWnd);		break;
#endif
	case CPageTreeWidget::eServices:		
	{
		switch(TabID)
		{
			case CPageTreeWidget::eHosters:	m_pServicesWnd->SetMode(CServicesWidget::eHosters); m_pStack->setCurrentWidget(m_pServicesWnd);	break;
			case CPageTreeWidget::eSolvers:	m_pServicesWnd->SetMode(CServicesWidget::eSolvers); m_pStack->setCurrentWidget(m_pServicesWnd);	break;
			case CPageTreeWidget::eFinders:	m_pServicesWnd->SetMode(CServicesWidget::eFinders); m_pStack->setCurrentWidget(m_pServicesWnd);	break;
			case CPageTreeWidget::eServers:	m_pStack->setCurrentWidget(m_pServersWnd);	break;
			default:						m_pStack->setCurrentWidget(m_pServiceWnd);	break;
		}
		break;
	}
	case CPageTreeWidget::eStatistics:		m_pStack->setCurrentWidget(m_pStatisticsWnd);	break;
	}

	//if(PageID > CPageTreeWidget::ePlayer)
	//	m_pToolBar->hide();
	//else if(m_pToolBar->isHidden())
	//	m_pToolBar->show();
}

void CModernGUI::OnPageClosed(int PageID, uint64 TabID)
{

}

void CModernGUI::OnAddLinks()
{
	QString Links = CMultiLineInput::GetInput(0, tr("Enter Links to add"));
	if(Links.isEmpty())
		return;
	CGrabberWindow::AddLinks(Links);
}

void CModernGUI::OnAddFile()
{
	QFileDialog dialog(this, tr("Add File"), theGUI->Cfg()->GetString("Gui/LastFilePath"), CGrabberWindow::GetOpenFilter(true));
	/*dialog.setOptions(QFileDialog::DontUseNativeDialog);
	QCheckBox* pUseGrabber = NULL;
	if(QGridLayout* pLayout = qobject_cast<QGridLayout*>(dialog.layout()))
	{
		pUseGrabber = new QCheckBox(tr("Use Grabber"));
		pLayout->addWidget(pUseGrabber, pLayout->rowCount(), pLayout->columnCount() -1);
	}*/
	if(dialog.exec() != QDialog::Accepted)
		return;
	QStringList Files = dialog.selectedFiles();
	if(!Files.isEmpty())
		theGUI->Cfg()->SetSetting("Gui/LastFilePath", QFileInfo(Files.first()).dir().absolutePath());
	foreach(const QString& FilePath, Files)
		CGrabberWindow::AddFile(FilePath);
	
	if(theGUI->Cfg()->GetBool("Gui/UnifyGrabber"))
		OnPageChanged(CPageTreeWidget::eGrabber, 0);
	// else we always auto jump to new grabber
}

class CCoreActionJob: public CInterfaceJob
{
public:
	CCoreActionJob(const QString& Action)
	{
		m_Request["Action"] = Action;
		//m_Request["Log"] = true;
	}

	virtual QString			GetCommand()	{return "CoreAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CModernGUI::CoreAction(const QString& Action)
{
	CCoreActionJob* pCoreActionJob = new CCoreActionJob(Action);
	theGUI->ScheduleJob(pCoreActionJob);
}

void CModernGUI::OnScanShare()
{
	CoreAction("ScanShare");
}

void CModernGUI::OnClearCompleted()
{
	if(m_pFileListWnd->GetMode() == CFileListView::eDownload)
		CoreAction("ClearCompleted");
	else if(m_pFileListWnd->GetMode() == CFileListView::eFileArchive)
	{
		if(QMessageBox(tr("File History"), tr("Clear file history?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default, QMessageBox::NoButton).exec() == QMessageBox::Yes)
			CoreAction("ClearHistory");
	}
	else if(m_pFileListWnd->GetMode() == CFileListView::eFilesGrabber)
		CGrabberWindow::GrabberAction(0, "Remove");
}

void CModernGUI::OnDetailTabs()
{
	m_pFileListWnd->ShowDetails(m_pDetailTabs->isChecked());
}

void CModernGUI::OnSearch()
{
	QString Expression = m_pSearchExp->text();
	if(Expression.isEmpty())
		return;
	m_pSearchExp->clear();

	QString Type = theGUI->Cfg()->GetString("Gui/Widget_SearchType");

	QVariantMap Criteria;
	Criteria["Type"] = Type;

	if(!Expression.isEmpty())
		CSearchWindow::StartSearch("SmartAgent", Expression, Criteria);
}

void CModernGUI::OnSearchNet(QAction* pAction)
{
	QString Type;
	if(m_pNeo->isChecked())
		Type = "neo";
	else if(m_pTorrent->isChecked())
		Type = "btih";
	else if(m_pLinks->isChecked())
		Type = "arch";
	else if(m_pEd2k->isChecked())
		Type = "ed2k";

	if(pAction)
		theGUI->Cfg()->SetSetting("Gui/Widget_SearchType", Type);
}

void CModernGUI::OnStreamFile(uint64 ID, const QString& FileName, bool bComplete)
{
	if(CreatePlayer)
	{
		m_pPageTree->SetPage(CPageTreeWidget::ePlayer);
		m_pPageTree->AddPlayer(ID, Split2(FileName,"/", true).second);
	}
	else
	{
		QString Player = theGUI->Cfg()->GetString("Gui/ExternPlayer");
		if(!Player.isEmpty())
		{
			QString Args;
			if(theGUI->IsLocal())
			{
				if(bComplete)
					Args = FileName;
				else
					Args = QString("http://localhost:%1/Repository/Data/id:%2/?%3").arg(theGUI->Cfg()->GetString("HttpServer/Port")).arg(ID).arg(m_PlayerArgs);
			}
			else
				Args = QString("http://%1:%2/Repository/Data/id:%3/?%4").arg(theGUI->Cfg()->GetString("Core/RemoteHost")).arg(theGUI->Cfg()->GetString("HttpServer/Port")).arg(ID).arg(m_PlayerArgs);

			if(Player.contains("%1"))
				QProcess::startDetached(Player.arg(Args));
			else
				QProcess::startDetached(Player, QStringList(Args));
		}
	}
}

void CModernGUI::OnTogleDetails()
{
	m_pDetailTabs->setChecked(!m_pDetailTabs->isChecked());
	OnDetailTabs();
}

void CModernGUI::OnOnLine(const QString& Expression)
{
	COnlineFileList* pOnlineFileList = new COnlineFileList();
	pOnlineFileList->SearchOnline(Expression);
	m_pOnlineWnds.append(pOnlineFileList);
	m_pPageTree->AddOnline(pOnlineFileList, Expression);
}
