#include "GlobalHeader.h"
#include "PageTreeWidget.h"
#include "../NeoGUI.h"
#include "GUI/SearchWindow.h"
#include "GUI/GrabberWindow.h"

void AddSeparator(QTreeWidget* pPageTree)
{
	QTreeWidgetItem* pSeparator = new QTreeWidgetItem;
	pPageTree->addTopLevelItem(pSeparator);
    QFrame* pLine = new QFrame();
    pLine->setFrameShape(QFrame::HLine);
    pLine->setFrameShadow(QFrame::Sunken);
	pPageTree->setItemWidget(pSeparator, 0, pLine);
	pSeparator->setDisabled(true);
}

CPageTreeWidget::CPageTreeWidget(bool bPlayer, QWidget *parent)
:QWidget(parent)
{
	m_pSearchesSyncJob = NULL;
	m_pGrabbingsSyncJob = NULL;

	m_PlayingNow = 0;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(1);

	m_pPageTree = new QTreeWidget();
	m_pPageTree->header()->hide();
	//m_pPageTree->setHeaderLabels(tr("").split("|"));
	//m_pPageTree->setContextMenuPolicy(Qt::CustomContextMenu);
	//connect(m_pPageTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	//connect(m_pPageTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pPageTree, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));
	//connect(m_pPageTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));
	m_pMainLayout->addWidget(m_pPageTree);
	
	QString Collaps = theGUI->Cfg()->GetString("Gui/Widget_Collaps");

	m_pSummary = new QTreeWidgetItem(QStringList(tr("Summary")));
	m_pSummary->setData(0, Qt::UserRole, eSummary);
	m_pSummary->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Info"))));
	m_pPageTree->addTopLevelItem(m_pSummary);

	m_pDownloads = new QTreeWidgetItem(QStringList(tr("Downloads")));
	m_pDownloads->setData(0, Qt::UserRole, eDownloads);
	m_pDownloads->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Downloads"))));
	m_pPageTree->addTopLevelItem(m_pDownloads);

	QTreeWidgetItem* m_pDlStarted = new QTreeWidgetItem(QStringList(tr("Started")));
	m_pDlStarted->setData(0, Qt::UserRole, eStarted);
	m_pDlStarted->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Start"))));
	m_pDownloads->addChild(m_pDlStarted);

	QTreeWidgetItem* m_pDlPaused = new QTreeWidgetItem(QStringList(tr("Paused")));
	m_pDlPaused->setData(0, Qt::UserRole, ePaused);
	m_pDlPaused->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Pause"))));
	m_pDownloads->addChild(m_pDlPaused);

	QTreeWidgetItem* m_pDlStopped = new QTreeWidgetItem(QStringList(tr("Stopped")));
	m_pDlStopped->setData(0, Qt::UserRole, eStopped);
	m_pDlStopped->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Stop"))));
	m_pDownloads->addChild(m_pDlStopped);

	QTreeWidgetItem* m_pDlCompleted = new QTreeWidgetItem(QStringList(tr("Completed")));
	m_pDlCompleted->setData(0, Qt::UserRole, eCompleted);
	m_pDlCompleted->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Check"))));
	m_pDownloads->addChild(m_pDlCompleted);

	if(!Collaps.contains("d"))
		m_pDownloads->setExpanded(true);

	m_pSearches = new QTreeWidgetItem(QStringList(tr("Searches")));
	m_pSearches->setData(0, Qt::UserRole, eSearches);
	m_pSearches->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Search"))));
	m_pPageTree->addTopLevelItem(m_pSearches);

	m_pGrabber = new QTreeWidgetItem(QStringList(tr("Grabber")));
	m_pGrabber->setData(0, Qt::UserRole, eGrabber);
	m_pGrabber->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Grabber"))));
	m_pPageTree->addTopLevelItem(m_pGrabber);

	m_pShared = new QTreeWidgetItem(QStringList(tr("Shared")));
	m_pShared->setData(0, Qt::UserRole, eShared);
	m_pShared->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Share"))));
	m_pPageTree->addTopLevelItem(m_pShared);

	QTreeWidgetItem* m_pShStarted = new QTreeWidgetItem(QStringList(tr("Started")));
	m_pShStarted->setData(0, Qt::UserRole, eStarted);
	m_pShStarted->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Start"))));
	m_pShared->addChild(m_pShStarted);

	QTreeWidgetItem* m_pShPaused = new QTreeWidgetItem(QStringList(tr("Paused")));
	m_pShPaused->setData(0, Qt::UserRole, ePaused);
	m_pShPaused->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Pause"))));
	m_pShared->addChild(m_pShPaused);

	QTreeWidgetItem* m_pShStopped = new QTreeWidgetItem(QStringList(tr("Stopped")));
	m_pShStopped->setData(0, Qt::UserRole, eStopped);
	m_pShStopped->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Stop"))));
	m_pShared->addChild(m_pShStopped);

	if(!Collaps.contains("s"))
		m_pShared->setExpanded(true);

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
	{
		m_pArchive = new QTreeWidgetItem(QStringList(tr("Archive")));
		m_pArchive->setData(0, Qt::UserRole, eArchive);
		m_pArchive->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Archive"))));
		m_pPageTree->addTopLevelItem(m_pArchive);
	}
	else
		m_pArchive = NULL;

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls"))
	{
		AddSeparator(m_pPageTree);

		m_pTransfers = new QTreeWidgetItem(QStringList(tr("Transfers")));
		m_pTransfers->setData(0, Qt::UserRole, eTransfers);
		m_pTransfers->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Transfers"))));
		m_pPageTree->addTopLevelItem(m_pTransfers);

		QTreeWidgetItem* pDownloads = new QTreeWidgetItem(QStringList(tr("Downloads")));
		pDownloads->setData(0, Qt::UserRole, eDownloads);
		pDownloads->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Download"))));
		m_pTransfers->addChild(pDownloads);

		QTreeWidgetItem* pUploads = new QTreeWidgetItem(QStringList(tr("Uploads")));
		pUploads->setData(0, Qt::UserRole, eUploads);
		pUploads->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Upload"))));
		m_pTransfers->addChild(pUploads);

		if(!Collaps.contains("t"))
			m_pTransfers->setExpanded(true);

		m_pClients = new QTreeWidgetItem(QStringList(tr("P2P Clients")));
		m_pClients->setData(0, Qt::UserRole, eClients);
		m_pClients->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/P2PClients"))));
		m_pPageTree->addTopLevelItem(m_pClients);

		m_pWebTasks = new QTreeWidgetItem(QStringList(tr("Web Tasks")));
		m_pWebTasks->setData(0, Qt::UserRole, eWebTasks);
		m_pWebTasks->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/WebTasks"))));
		m_pPageTree->addTopLevelItem(m_pWebTasks);
	}
	else
	{
		m_pTransfers = NULL;

		m_pClients = NULL;

		m_pWebTasks = NULL;
	}

	AddSeparator(m_pPageTree);

	if(bPlayer)
	{
		m_pPlayer = new QTreeWidgetItem(QStringList(tr("Player")));
		m_pPlayer->setData(0, Qt::UserRole, ePlayer);
		m_pPlayer->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Playback"))));
		m_pPageTree->addTopLevelItem(m_pPlayer);

		AddSeparator(m_pPageTree);
	}
	else
		m_pPlayer = NULL;

#ifdef CRAWLER
	m_pCrawler = new QTreeWidgetItem(QStringList(tr("Crawler")));
	m_pCrawler->setData(0, Qt::UserRole, eCrawler);
	m_pCrawler->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Links"))));
	m_pPageTree->addTopLevelItem(m_pCrawler);
#endif

	m_pServices = new QTreeWidgetItem(QStringList(tr("Services")));
	m_pServices->setData(0, Qt::UserRole, eServices);
	m_pServices->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Services"))));
	m_pPageTree->addTopLevelItem(m_pServices);

	m_pHosters = new QTreeWidgetItem(QStringList(tr("File Hosters")));
	m_pHosters->setData(0, Qt::UserRole, eHosters);
	m_pHosters->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Hosters"))));
	m_pServices->addChild(m_pHosters);

	m_pSolvers = new QTreeWidgetItem(QStringList(tr("Captcha Solvers")));
	m_pSolvers->setData(0, Qt::UserRole, eSolvers);
	m_pSolvers->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Captcha"))));
	m_pServices->addChild(m_pSolvers);

	m_pFinders = new QTreeWidgetItem(QStringList(tr("Search Engines")));
	m_pFinders->setData(0, Qt::UserRole, eFinders);
	m_pFinders->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Searching"))));
	m_pServices->addChild(m_pFinders);

	m_pServers = new QTreeWidgetItem(QStringList(tr("P2P Servers")));
	m_pServers->setData(0, Qt::UserRole, eServers);
	m_pServers->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Servers"))));
	m_pServices->addChild(m_pServers);

	if(!Collaps.contains("x"))
		m_pServices->setExpanded(true);

	m_pStatistics = new QTreeWidgetItem(QStringList(tr("Statistics")));
	m_pStatistics->setData(0, Qt::UserRole, eStatistics);
	m_pStatistics->setIcon(0, QIcon(QPixmap::fromImage(QImage(":/Icons/Statistics"))));
	m_pPageTree->addTopLevelItem(m_pStatistics);

	setLayout(m_pMainLayout);

	//m_pPageTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Page_Columns"));

	m_TimerId = startTimer(100);
}

CPageTreeWidget::~CPageTreeWidget()
{
	QString Collaps;
	if(!m_pDownloads->isExpanded())
		Collaps.append("d");
	if(!m_pShared->isExpanded())
		Collaps.append("s");
	if(m_pTransfers && !m_pTransfers->isExpanded())
		Collaps.append("t");
	if(!m_pServices->isExpanded())
		Collaps.append("x");
	theGUI->Cfg()->SetSetting("Gui/Widget_Collaps", Collaps);
	
	//theGUI->Cfg()->SetBlob("Gui/Widget_Page_Columns",m_pPageTree->header()->saveState());

	killTimer(m_TimerId);
}

class CSearchesSyncJob: public CInterfaceJob
{
public:
	CSearchesSyncJob(CPageTreeWidget* pView)
	{
		m_pView = pView;
	}

	virtual QString			GetCommand()	{return "SearchList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncSearches(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pSearchesSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CPageTreeWidget>	m_pView; // Note: this can be deleted at any time
};

class CGrabbingsSyncJob: public CInterfaceJob
{
public:
	CGrabbingsSyncJob(CPageTreeWidget* pView)
	{
		m_pView = pView;
	}

	virtual QString			GetCommand()	{return "GrabberList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncTasks(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pGrabbingsSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CPageTreeWidget>	m_pView; // Note: this can be deleted at any time
};

void CPageTreeWidget::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	if(m_pSearchesSyncJob == NULL)
	{
		m_pSearchesSyncJob = new CSearchesSyncJob(this);
		theGUI->ScheduleJob(m_pSearchesSyncJob);
	}

	if(!theGUI->Cfg()->GetBool("Gui/UnifyGrabber"))
	{
		if(m_pGrabbingsSyncJob == NULL)
		{
			m_pGrabbingsSyncJob = new CGrabbingsSyncJob(this);
			theGUI->ScheduleJob(m_pGrabbingsSyncJob);
		}
	}
}

class CTreeLabelItem: public QWidget
{
	// Q_OBJECT
public:
	CTreeLabelItem(QTreeWidgetItem* pItem, QWidget* parent)
	{
		m_pItem = pItem;

		m_pButton = new QPushButton();
		m_pButton->setFlat(true);
		m_pButton->setMaximumHeight(16);
		m_pButton->setMaximumWidth(16);
		connect(m_pButton, SIGNAL(released()), parent, SLOT(OnCloseButton()));

		m_pLayout = new QHBoxLayout(this);
		m_pLayout->setMargin(0);
		m_pLayout->setAlignment(Qt::AlignLeft);

		m_pLayout->addSpacing(5);

		m_pLabel = new QLabel();
		m_pLayout->addWidget(m_pLabel);

		m_pLayout->addWidget(m_pButton);
	}

	void	SetText(const QString& Text)	{m_pLabel->setText(Text);}
	void	SetIcon(const QIcon& Icon)		{m_pButton->setIcon(Icon);}

	QTreeWidgetItem* GetItem()				{return m_pItem;}

protected:
	QTreeWidgetItem* m_pItem;

	QHBoxLayout*	m_pLayout;
	QPushButton*	m_pButton;
	QLabel*			m_pLabel;
};

void CPageTreeWidget::SyncSearches(const QVariantMap& Response)
{
	QMap<uint64, SSearch*> OldSearches = m_Searches;

	foreach (const QVariant vSearch, Response["Searches"].toList())
	{
		QVariantMap Search = vSearch.toMap();
		uint64 SearchID = Search["ID"].toULongLong();
		
		QString Expression = Search["Expression"].toString();
#ifdef _DEBUG
		if(Expression.isEmpty())
			Expression = "...";
#endif
		if(Expression.isEmpty())
			continue;

		SSearch* pSearch = OldSearches.take(SearchID);
		if(!pSearch)
		{
			pSearch = new SSearch();
			//pSearch->Expression = Expression;
			//pSearch->SearchNet = Search["SearchNet"].toString();
			//pSearch->Criteria = Search["Criteria"].toMap();
			//pSearch->pItem = new QTreeWidgetItem(QStringList(Expression));
			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, SearchID);

			m_pSearches->addChild(pItem);
			m_pSearches->setExpanded(true);
			pSearch->pLabel = new CTreeLabelItem(pItem, this);
			m_pPageTree->setItemWidget(pItem, 0, pSearch->pLabel);

			m_Searches.insert(SearchID, pSearch);

			if(QTreeWidgetItem* pCurItem = m_pPageTree->currentItem())
			{
				if(pCurItem->parent() || pCurItem->data(0, Qt::UserRole).toInt() != eSearches || theGUI->Cfg()->GetBool("Gui/GoToNew"))
					m_pPageTree->setCurrentItem(pItem);
			}
		}

		pSearch->pLabel->SetText(tr("%1 (%2)").arg(Expression).arg(Search["Count"].toInt()));

		if(pSearch->Status != Search["Status"])
		{
			pSearch->Status = Search["Status"].toString();
			if(pSearch->Status == "Running")
				pSearch->pLabel->SetIcon(QIcon(QPixmap::fromImage(QImage(":/Icons/CloseSearchRun"))));
			else if(pSearch->Status == "Finished")
				pSearch->pLabel->SetIcon(QIcon(QPixmap::fromImage(QImage(":/Icons/CloseSearch"))));
			else
				pSearch->pLabel->SetIcon(QIcon(QPixmap::fromImage(QImage(":/Icons/CloseSearchErr"))));
		}
	}

	foreach(SSearch* pSearch, OldSearches)
	{
		m_Searches.remove(OldSearches.key(pSearch));
		delete pSearch->pLabel->GetItem();
		delete pSearch;
	}
}

void CPageTreeWidget::SyncTasks(const QVariantMap& Response)
{
	QMap<uint64, STask*> OldTasks = m_Tasks;

	foreach (const QVariant vTask, Response["Tasks"].toList())
	{
		QVariantMap Task = vTask.toMap();
		uint64 GrabberID = Task["ID"].toULongLong();

		STask* pTask = OldTasks.take(GrabberID);
		if(!pTask)
		{
			pTask = new STask();
			//pTask->Urls = Task["Uris"].toStringList();
			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, GrabberID);

			m_pGrabber->addChild(pItem);
			m_pGrabber->setExpanded(true);
			pTask->pLabel = new CTreeLabelItem(pItem, this);
			m_pPageTree->setItemWidget(pItem, 0, pTask->pLabel);

			m_Tasks.insert(GrabberID, pTask);

			if(QTreeWidgetItem* pCurItem = m_pPageTree->currentItem())
			{
				if(pCurItem->parent() || pCurItem->data(0, Qt::UserRole).toInt() != eGrabber) // || theGUI->Cfg()->GetBool("Gui/GoToNew"))
					m_pPageTree->setCurrentItem(pItem);
			}
		}

		pTask->pLabel->SetText(tr("%1 (%2)").arg(CGrabberWindow::GetDisplayName(Task["Uris"].toStringList())).arg(Task["FileCount"].toInt()));

		QString Status;
		if(Task["TasksPending"].toInt() > Task["TasksFailed"].toInt())
			Status = "Running";
		else if(Task["TasksFailed"].toInt() == 0)
			Status = "Finished";
		else
			Status = "Errors";

		if(pTask->Status != Status)
		{
			pTask->Status = Status;
			if(pTask->Status == "Running")
				pTask->pLabel->SetIcon(QIcon(QPixmap::fromImage(QImage(":/Icons/CloseSearchRun"))));
			else if(pTask->Status == "Finished")
				pTask->pLabel->SetIcon(QIcon(QPixmap::fromImage(QImage(":/Icons/CloseSearch"))));
			else
				pTask->pLabel->SetIcon(QIcon(QPixmap::fromImage(QImage(":/Icons/CloseSearchErr"))));
		}
	}

	foreach(STask* pTask, OldTasks)
	{
		if(m_OnLine.contains((QWidget*)OldTasks.key(pTask)))
			continue;
		m_Tasks.remove(OldTasks.key(pTask));
		delete pTask->pLabel->GetItem();
		delete pTask;
	}
}

void CPageTreeWidget::SetPage(int PageID, int PageSubID)
{
	switch(PageID)
	{
		case eSummary:
		case eDownloads:
		case eShared:
		case eTransfers:
		{
			if(!m_pTransfers)
				break;

			QTreeWidgetItem* pItem = NULL;
			if(PageID == eSummary)
				pItem = m_pSummary;
			else if(PageID == eDownloads)
				pItem = m_pDownloads;
			else if(PageID == eShared)
				pItem = m_pShared;
			else
				pItem = m_pTransfers;

			if(PageSubID != 0)
			{
				for(int i=0; i < pItem->childCount(); i++)
				{
					if(pItem->child(i)->data(0, Qt::UserRole) == PageSubID)
					{
						pItem = pItem->child(i);
						break;
					}
				}
			}

			m_pPageTree->setCurrentItem(pItem);
			break;
		}
		case eSearches:			m_pPageTree->setCurrentItem(m_pSearches);	break;
		case eGrabber:			m_pPageTree->setCurrentItem(m_pGrabber);	break;
		case eClients:			if(m_pClients) m_pPageTree->setCurrentItem(m_pClients); 	break;
		case eWebTasks:			if(m_pWebTasks) m_pPageTree->setCurrentItem(m_pWebTasks);	break;
		case eArchive:			if(m_pArchive) m_pPageTree->setCurrentItem(m_pArchive);		break;
		case ePlayer:			if(m_pPlayer) m_pPageTree->setCurrentItem(m_pPlayer);		break;
#ifdef CRAWLER
		case eCrawler:			m_pPageTree->setCurrentItem(m_pCrawler);	break;
#endif
		case eServices:		
		{
			switch(PageSubID)
			{
				case eHosters:	m_pPageTree->setCurrentItem(m_pHosters);	break;
				case eSolvers:	m_pPageTree->setCurrentItem(m_pSolvers);	break;
				case eFinders:	m_pPageTree->setCurrentItem(m_pFinders);	break;
				case eServers:	m_pPageTree->setCurrentItem(m_pServers);	break;
				default:		m_pPageTree->setCurrentItem(m_pServices);	break;
			}
			break;
		}
		case eStatistics:		m_pPageTree->setCurrentItem(m_pStatistics);	break;
	}
}

void CPageTreeWidget::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	QTreeWidgetItem* pParent = pItem;
	while(QTreeWidgetItem* pTemp = pParent->parent())
		pParent = pTemp;
	
	int PageID = pParent->data(0, Qt::UserRole).toInt();
	uint64 TabID = pParent != pItem ? pItem->data(0, Qt::UserRole).toULongLong() : 0;

	if(PageID == ePlayer)
	{
		if(m_PlayingNow == TabID)
			TabID = 0;
		else if(TabID)
			m_PlayingNow = TabID;
	}

	if(m_OnLine.contains((QWidget*)TabID))
		PageID = eOnLine;
	emit PageChanged(PageID, TabID);
}

void CPageTreeWidget::OnSelectionChanged()
{
	if(QTreeWidgetItem* pItem = m_pPageTree->currentItem())
		OnItemClicked(pItem, 0);
}

//void CPageTreeWidget::OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
//{
//
//}

void CPageTreeWidget::OnCloseButton()
{
	CTreeLabelItem* pLabel = (CTreeLabelItem*)((QPushButton*)sender())->parentWidget();
	QTreeWidgetItem* pItem = pLabel->GetItem();
	QTreeWidgetItem* pParent = pItem;
	while(QTreeWidgetItem* pTemp = pParent->parent())
		pParent = pTemp;
	
	int PageID = pParent->data(0, Qt::UserRole).toInt();
	uint64 TabID = pParent != pItem ? pItem->data(0, Qt::UserRole).toULongLong() : 0;

	if(PageID == eSearches)
		CSearchWindow::StopSearch(TabID);
	else if(PageID == eGrabber)
		CGrabberWindow::GrabberAction(TabID, "Remove");
	else if(PageID == ePlayer)
		delete pItem;

	if(m_OnLine.removeOne((QWidget*)TabID))
	{
		delete pItem;
		PageID = eOnLine;
		delete (QWidget*)TabID;
	}
	emit PageClosed(PageID, TabID);
}

//void CPageTreeWidget::OnMenuRequested(const QPoint &point)
//{
//	m_pMenu->popup(QCursor::pos());	
//}

void CPageTreeWidget::AddPlayer(uint64 ID, const QString& FileName)
{
	if(!m_pPlayer)
		return;

	for(int i=0; i < m_pPlayer->childCount(); i++)
	{
		QTreeWidgetItem* pPlayer = m_pPlayer->child(i);
		if(pPlayer->data(0, Qt::UserRole).toULongLong() == ID)
		{
			m_pPageTree->setCurrentItem(pPlayer);
			return;
		}
	}

	QTreeWidgetItem* pPlayer = new QTreeWidgetItem();
	pPlayer->setData(0, Qt::UserRole, ID);
	m_pPlayer->addChild(pPlayer);
	m_pPlayer->setExpanded(true);

	CTreeLabelItem* pLabel = new CTreeLabelItem(pPlayer, this);
	m_pPageTree->setItemWidget(pPlayer, 0, pLabel);

	pLabel->SetText(FileName);
	pLabel->SetIcon(QIcon(QPixmap::fromImage(QImage(":/Icons/CloseSearch"))));

#if QT_VERSION >= 0x050000
	m_PlayingNames.insert(ID, FileName);
#endif

	m_pPageTree->setCurrentItem(pPlayer);
}

void CPageTreeWidget::AddOnline(QWidget* pWnd, const QString& Expression)
{
	m_OnLine.append(pWnd);

	QTreeWidgetItem* pPlayer = new QTreeWidgetItem();
	pPlayer->setData(0, Qt::UserRole, (uint64)pWnd);
	m_pSearches->addChild(pPlayer);
	m_pSearches->setExpanded(true);

	CTreeLabelItem* pLabel = new CTreeLabelItem(pPlayer, this);
	m_pPageTree->setItemWidget(pPlayer, 0, pLabel);

	pLabel->SetText(Expression);
	pLabel->SetIcon(QIcon(QPixmap::fromImage(QImage(":/Icons/CloseSearch"))));

	m_pPageTree->setCurrentItem(pPlayer);
}