#include "GlobalHeader.h"
#include "CrawlerWindow.h"
#include "../NeoGUI.h"
#include "CrawlerEntry.h"
#include "CrawlerSite.h"
#include "../Common/TreeWidgetEx.h"
#include "../Common/Common.h"
#include "../Common/Dialog.h"

#ifdef CRAWLER

CCrawlerWindow::CCrawlerWindow(QWidget *parent)
:QWidget(parent)
{
	m_pCrawlerSyncJob = NULL;
	m_UpdateFilter = true;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(1);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);

	m_pToolBar = new QToolBar();

	m_pAddSite = MakeAction(m_pToolBar, ":/Icons/Files", tr("Add Site"));
	connect(m_pAddSite, SIGNAL(triggered()), this, SLOT(OnAddSite()));

	m_pToolBar->addSeparator();

	m_pToolBar->addWidget(new QLabel(tr("Entries:")));
	m_pLimit = new QSpinBox();
	m_pLimit->setMaximum(10000);
	m_pLimit->setMinimum(10);
	m_pLimit->setValue(100);
	connect(m_pLimit, SIGNAL(valueChanged(int)), this, SLOT(OnValueChanged(int)));
	m_pToolBar->addWidget(m_pLimit);
	m_pToolBar->addWidget(new QLabel(tr(" @ ")));

	m_pPage = new QSpinBox();
	m_pPage->setMinimum(1);
	connect(m_pPage, SIGNAL(valueChanged(int)), this, SLOT(OnValueChanged(int)));
	m_pToolBar->addWidget(m_pPage);

	m_pPages = new QLabel(tr(" / 0"));
	m_pToolBar->addWidget(m_pPages);

	m_pToolBar->addSeparator();

	m_pToolBar->addWidget(new QLabel(tr("Sort:")));
	m_pSortBy = new QComboBox();
	m_pSortBy->addItem(tr("No"), "");
	m_pSortBy->addItem(tr("by Date Asc"), "Date+");
	m_pSortBy->addItem(tr("by Date Desc"), "Date-");
	m_pSortBy->addItem(tr("by Url Asc"), "Url+");
	m_pSortBy->addItem(tr("by Url Desc"), "Url-");
	connect(m_pSortBy, SIGNAL(currentIndexChanged(int)), this, SLOT(OnCurrentIndexChanged(int)));
	m_pToolBar->addWidget(m_pSortBy);

	m_pToolBar->addSeparator();

	m_pToolBar->addWidget(new QLabel(tr("Filter:")));
	m_pFilter = new QLineEdit();
	m_pFilter->setMaximumWidth(300);
	connect(m_pFilter, SIGNAL(returnPressed()), this, SLOT(OnReturnPressed()));
	m_pToolBar->addWidget(m_pFilter);

	m_pMainLayout->addWidget(m_pToolBar);

	m_pCrawlerTree = new QTreeWidgetEx();
	m_pCrawlerTree->setHeaderLabels(tr("Site Name|Entries|Sites|Tasks|Last Results|Keywords|Files").split("|"));
	m_pCrawlerTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pCrawlerTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	//connect(m_pCrawlerTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pCrawlerTree, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));
	connect(m_pCrawlerTree, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(OnItemExpanded(QTreeWidgetItem*)));
	m_pCrawlerTree->setSortingEnabled(true);
#ifdef WIN32
	m_pCrawlerTree->setStyle(QStyleFactory::create("windowsxp"));
#endif

	m_pSplitter->addWidget(m_pCrawlerTree);

	m_pMenu = new QMenu();

	m_pIndexSite = new QAction(tr("Index Site"), m_pMenu);
	connect(m_pIndexSite, SIGNAL(triggered()), this, SLOT(OnIndexSite()));
	m_pMenu->addAction(m_pIndexSite);

	m_pCrawlSite = new QAction(tr("Crawl Site"), m_pMenu);
	connect(m_pCrawlSite, SIGNAL(triggered()), this, SLOT(OnCrawlSite()));
	m_pMenu->addAction(m_pCrawlSite);

	m_pStopTasks = new QAction(tr("Stop Tasks"), m_pMenu);
	connect(m_pStopTasks, SIGNAL(triggered()), this, SLOT(OnStopTasks()));
	m_pMenu->addAction(m_pStopTasks);

	m_pMenu->addSeparator();

	m_pBlastKad = new QAction(tr("Blast Kad"), m_pMenu);
	connect(m_pBlastKad, SIGNAL(triggered()), this, SLOT(OnBlastKad()));
	m_pMenu->addAction(m_pBlastKad);

	m_pMenu->addSeparator();

	m_pRemoveSite = new QAction(tr("Remove Site"), m_pMenu);
	connect(m_pRemoveSite, SIGNAL(triggered()), this, SLOT(OnRemoveSite()));
	m_pMenu->addAction(m_pRemoveSite);

	m_SubWidget = new QWidget();
	m_SubLayout = new QStackedLayout(m_SubWidget);

	m_pCrawlerSite = new CCrawlerSite();
	m_SubLayout->addWidget(m_pCrawlerSite);

	m_pCrawlerEntry = new CCrawlerEntry();
	m_SubLayout->addWidget(m_pCrawlerEntry);

	m_SubWidget->setLayout(m_SubLayout);

	m_pSplitter->addWidget(m_SubWidget);

	m_pMainLayout->addWidget(m_pSplitter);

	setLayout(m_pMainLayout);

	m_pSplitter->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Crawler_Spliter"));
	m_pCrawlerTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Crawler_Columns"));

	m_TimerId = startTimer(500);
}

CCrawlerWindow::~CCrawlerWindow()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Crawler_Spliter",m_pSplitter->saveState());
	theGUI->Cfg()->SetBlob("Gui/Widget_Crawler_Columns",m_pCrawlerTree->header()->saveState());

	killTimer(m_TimerId);

	foreach(SSite* pSite, m_Sites)
		delete pSite;
}

class CCrawlerSyncJob: public CInterfaceJob
{
public:
	CCrawlerSyncJob(CCrawlerWindow* pView, const QVariantList& Browse)
	{
		m_pView = pView;
		m_Request["Browse"] = Browse;
	}

	virtual QString			GetCommand()	{return "CrawlerList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncCrawler(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pCrawlerSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CCrawlerWindow>m_pView; // Note: this can be deleted at any time
};

void CCrawlerWindow::SyncCrawler()
{
	if(m_pCrawlerSyncJob == NULL)
	{
		QVariantList Browse;

		QVariantMap Brows;
		Brows["Filter"] = m_pFilter->text();
		Brows["Sort"] = m_pSortBy->itemData(m_pSortBy->currentIndex());
		int Limit = m_pLimit->value();

		if(m_UpdateFilter)
		{
			m_UpdateFilter = false;
			for(QMap<QString, SSite*>::iterator I = m_Sites.begin(); I != m_Sites.end(); I++)
			{
				SSite* pSite = I.value();
				if(pSite->pItem->isExpanded())
				{
					Brows["SiteName"] = I.key();
					Brows["Offset"] = (pSite->Page - 1) * Limit;
					Brows["MaxCount"] = Limit;
					Browse.append(Brows);
				}
			}
		}

		m_pCrawlerSyncJob = new CCrawlerSyncJob(this, Browse);
		theGUI->ScheduleJob(m_pCrawlerSyncJob);
	}
}

CCrawlerWindow::SSite* CCrawlerWindow::GetCurSite()
{
	QTreeWidgetItem* pItem = m_pCrawlerTree->currentItem();
	if(!pItem)
		return NULL;
	while(pItem->parent())
		pItem = pItem->parent();

	return m_Sites.value(pItem->data(eSiteName, Qt::UserRole).toString());
}

void CCrawlerWindow::OnItemExpanded(QTreeWidgetItem * item)
{
	if(item->parent())
		return;

	m_pCrawlerTree->setCurrentItem(item);
	OnFilterChanged();
}

void CCrawlerWindow::OnFilterChanged()
{
	m_UpdateFilter = true;
	if(SSite* pSite = GetCurSite())
	{
		UpdatePage(pSite);
		pSite->Page = m_pPage->value();
	}
}

void CCrawlerWindow::UpdatePage(SSite* pSite)
{
	pSite->Pages = pSite->EntryCount / m_pLimit->value() + 1;

	m_pPage->setMaximum(Max(1,pSite->Pages));
	m_pPages->setText(tr(" / %1").arg(pSite->Pages));
}

void CCrawlerWindow::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	SyncCrawler();
}

void CCrawlerWindow::SyncCrawler(const QVariantMap& Response)
{
	QMap<QString, SSite*> OldSites = m_Sites;

	QList<QTreeWidgetItem*> NewItems;
	foreach (const QVariant vSite, Response["Sites"].toList())
	{
		QVariantMap Site = vSite.toMap();
		QString SiteName = Site["SiteName"].toString();

		SSite* pSite = OldSites.take(SiteName);
		if(!pSite)
		{
			pSite = new SSite();
			m_Sites.insert(SiteName, pSite);
			pSite->pItem = new QTreeWidgetItem();
			pSite->pItem->setData(eSiteName, Qt::UserRole, SiteName);
			NewItems.append(pSite->pItem);			
		}

		pSite->SiteEntry = Site["SiteEntry"].toString();
		pSite->pItem->setText(eSiteName, tr("%1 (%2)").arg(SiteName).arg(pSite->SiteEntry));

		pSite->pItem->setText(eEntries, tr("%1/%2 (%3)").arg(Site["CrawledEntries"].toInt()).arg(Site["TotalEntries"].toInt()).arg(Site["ProcessedEntries"].toInt()));
		pSite->pItem->setText(eSites, tr("%1/%2").arg(Site["CrawledSites"].toInt()).arg(Site["TotalSites"].toInt()));
		pSite->pItem->setText(eActive, tr("%1 (%2ms)").arg(Site["ActiveTasks"].toInt()).arg(Site["SecForTasks"].toInt()));

		time_t uDate = Site["LastIndexTime"].toULongLong();
		QString Date = uDate ? QDateTime::fromTime_t(uDate).toString() : tr("Never");
		pSite->pItem->setText(eLast, tr("%1 @ %2").arg(Site["LastCrawledCount"].toInt()).arg(Date));

		pSite->pItem->setText(eKeywords, tr("%1 - %2/%3 (%4)").arg(Site["IndexedKeywords"].toInt()).arg(Site["FinishedKeywords"].toInt()).arg(Site["WaitingKeywords"].toInt()).arg(Site["ActiveKeywords"].toInt()));
		pSite->pItem->setText(eFiles, tr("%1 - %2/%3 (%4) - %5/%6 (%7)").arg(Site["IndexedFiles"].toInt())
			.arg(Site["FinishedRatings"].toInt()).arg(Site["WaitingRatings"].toInt()).arg(Site["ActiveRatings"].toInt())
			.arg(Site["FinishedLinks"].toInt()).arg(Site["WaitingLinks"].toInt()).arg(Site["ActiveLinks"].toInt()));

		if(Site.contains("FilteredEntries"))
			pSite->EntryCount = Site["FilteredEntries"].toInt();
		UpdatePage(pSite);

		if(pSite->pItem->isExpanded())
		{
			if(Site.contains("Entries"))
				SyncEntries(pSite, Site["Entries"].toList());
		}
		else if(pSite->pItem->childCount() != 1)
		{
			for(int i=0; i < pSite->pItem->childCount(); i++)
				delete pSite->pItem->child(i);
			SEntry* pDummy = new SEntry();
			pDummy->pItem = new QTreeWidgetItem();
			pSite->pItem->addChild(pDummy->pItem);
			pSite->Entries.insert("", pDummy);
		}
	}
	m_pCrawlerTree->addTopLevelItems(NewItems);

	for(QMap<QString, SSite*>::iterator I = OldSites.begin(); I != OldSites.end();I++)
	{
		m_Sites.remove(I.key());
		SSite* pSite = I.value();
		delete pSite->pItem;
		delete pSite;
	}
}

QTreeWidgetItem* CCrawlerWindow::GetDir(QTreeWidgetItem* pPrev, const QStringList& Path, int n)
{
	if(Path.count() == n+1)
		return pPrev; // to be placed here

	QString Name = Path.at(n);
	for(int i=0; i < pPrev->childCount(); i++)
	{
		QTreeWidgetItem* pItem = pPrev->child(i);
		if(pItem->text(eSiteName) == Name)
			return GetDir(pItem, Path, ++n);
	}

	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setData(eSiteName, Qt::UserRole, "");
	pItem->setText(eSiteName, Name);
	pPrev->addChild(pItem);
	pPrev->setExpanded(true);
	return GetDir(pItem, Path, ++n);
}

void CCrawlerWindow::SyncEntries(SSite* pSite, const QVariantList& Entries)
{
	QMap<QString, SEntry*> OldEntries = pSite->Entries;

	QMultiMap<QTreeWidgetItem*, QTreeWidgetItem*> NewItems;
	foreach (const QVariant vEntries, Entries)
	{
		QVariantMap Entries = vEntries.toMap();
		QString Url = Entries["Url"].toString();
		//QString Date = QDateTime::fromTime_t(Entries["Date"].toULongLong()).toString();

		foreach (const QVariant vFile, Entries["Files"].toList())
		{
			QVariantMap File = vFile.toMap();
			
			QString FileName = File["FileName"].toString();
			QVariantMap Details = File["Details"].toMap();

			QString ID = FileName + "@" + Url;

			SEntry* pEntry = OldEntries.value(ID);
			if(!pEntry)
			{
				if(pSite->Entries.contains(ID))
					continue; // filename conflict, ignore this file

				QStringList Path = Details["Category"].toStringList();
				Path.append("");
				QTreeWidgetItem* pPrev = GetDir(pSite->pItem, Path, 0);

				pEntry = new SEntry();
				pEntry->pItem = new QTreeWidgetItem();
				pEntry->pItem->setData(eSiteName, Qt::UserRole, ID);
				NewItems.insert(pPrev, pEntry->pItem);

				pSite->Entries.insert(ID, pEntry);

				pEntry->FileName = FileName;
				pEntry->pItem->setText(eSiteName, tr("%1 (%2)").arg(FileName).arg(Url));
			}
			else
				OldEntries[ID] = NULL;

			pEntry->Details = Details;
			if(File.contains("Hashes"))
			{
				QVariantMap Hashes = File["Hashes"].toMap();
				for(QVariantMap::iterator J = Hashes.begin(); J != Hashes.end(); J++)
					pEntry->Hashes.insert(J.key(), J.value().toString());
			}
			pEntry->Links = File["Links"].toStringList();
		}
	}
	foreach(QTreeWidgetItem* pPrev, NewItems.uniqueKeys())
	{
		pPrev->addChildren(NewItems.values(pPrev));
		pPrev->setExpanded(true);
	}

	for(QMap<QString, SEntry*>::iterator I = OldEntries.begin(); I != OldEntries.end();I++)
	{
		SEntry* pEntry = I.value();
		if(pEntry == NULL)
			continue;

		QTreeWidgetItem* pPrev = pEntry->pItem->parent();
		pSite->Entries.remove(I.key());
		delete pEntry->pItem;
		delete pEntry;

		ClearDir(pPrev);
	}
}

void CCrawlerWindow::ClearDir(QTreeWidgetItem* pItem)
{
	if(!pItem || pItem->childCount() > 0 || !pItem->data(eSiteName,Qt::UserRole).toString().isEmpty())
		return;
	
	QTreeWidgetItem* pPrev = pItem->parent();
	delete pItem;

	ClearDir(pPrev);
}

void CCrawlerWindow::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	SSite* pSite = GetCurSite();
	if(!pSite)
		return;

	UpdatePage(pSite);
	m_pPage->setValue(pSite->Page);
	
	QString SiteName = pItem->data(eSiteName, Qt::UserRole).toString();
	if(SEntry* pEntry = pSite->Entries.value(SiteName))
	{
		m_SubLayout->setCurrentWidget(m_pCrawlerEntry);
		m_pCrawlerEntry->ShowEntry(pEntry->FileName, pEntry->Hashes, pEntry->Details["Description"].toString(), pEntry->Details["CoverUrl"].toString(), pEntry->Links, pEntry->Details);
	}
	else
	{
		m_SubLayout->setCurrentWidget(m_pCrawlerSite);
		m_pCrawlerSite->ShowSite(SiteName);
	}
}

void CCrawlerWindow::OnSelectionChanged()
{
	if(QTreeWidgetItem* pItem = m_pCrawlerTree->currentItem())
		OnItemClicked(pItem, 0);
}

void CCrawlerWindow::Suspend(bool bSet)
{
	if(bSet)
	{
		if(m_TimerId != 0)
		{
			killTimer(m_TimerId);
			m_TimerId = 0;
		}
	}
	else
	{
		if(m_TimerId == 0)
			m_TimerId = startTimer(500);
		SyncCrawler();
	}
}

void CCrawlerWindow::OnMenuRequested(const QPoint &point)
{
	QTreeWidgetItem* pItem = m_pCrawlerTree->currentItem();
	if(!pItem)
		return;

	bool bRoot = pItem->parent() == NULL;

	m_pMenu->popup(QCursor::pos());	
}

class CCrawlerActionJob: public CInterfaceJob
{
public:
	CCrawlerActionJob(const QString& SiteName, const QString& Action)
	{
		m_Request["Action"] = Action;
		m_Request["Log"] = true;
		m_Request["SiteName"] = SiteName;
	}

	void SetField(const QString& Key, const QVariant& Value) {m_Request[Key] = Value;}
	virtual QString			GetCommand()	{return "CrawlerAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CCrawlerWindow::OnAction(const QString& Action)
{
	QTreeWidgetItem* pItem = m_pCrawlerTree->currentItem();
	if(!pItem || pItem->parent())
		return;

	QString SiteName = pItem->data(eSiteName, Qt::UserRole).toString();

	CCrawlerActionJob* pCrawlerActionJob = new CCrawlerActionJob(SiteName, Action);
	theGUI->ScheduleJob(pCrawlerActionJob);
}

class CIndexSite: public QDialogEx
{
	//Q_OBJECT

public:
	CIndexSite(const QString& SiteName, QWidget *pMainWindow)
		: QDialogEx(pMainWindow)
	{
		setWindowTitle(CCrawlerWindow::tr("Index: %1").arg(SiteName));

		m_pSiteLayout = new QFormLayout(this);

		m_pSiteEntry = new QComboBox();
		m_pSiteEntry->setEditable(true);
		m_pSiteEntry->setMaximumWidth(500);
		m_pSiteLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(CCrawlerWindow::tr("Site Entry:")));
		m_pSiteLayout->setWidget(1, QFormLayout::FieldRole, m_pSiteEntry);

		m_pCrawlDepth = new QSpinBox();
		m_pCrawlDepth->setMaximumWidth(50);
		m_pCrawlDepth->setMinimum(0);
		m_pCrawlDepth->setMaximum(1000);
		m_pSiteLayout->setWidget(2, QFormLayout::LabelRole, new QLabel(tr("Crawling Depth:")));
		m_pSiteLayout->setWidget(2, QFormLayout::FieldRole, m_pCrawlDepth);

		m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
		QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
		QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
		m_pSiteLayout->setWidget(3, QFormLayout::FieldRole, m_pButtonBox);
	}

	static bool	IndexSite(const QString& SiteName, QString SiteEntry, QWidget *pMainWindow = NULL)
	{
		CIndexSite IndexSite(SiteName, pMainWindow);

		QStringList SiteEntrys = theGUI->Cfg()->GetStringList("Gui/Widget_Crawler_Entries");
		if(!SiteEntrys.contains(SiteEntry))
			SiteEntrys.append(SiteEntry);

		IndexSite.m_pSiteEntry->addItems(SiteEntrys);
		if(!IndexSite.exec())
			return false;

		SiteEntry = IndexSite.m_pSiteEntry->currentText();

		SiteEntrys.removeAll(SiteEntry);
		SiteEntrys.prepend(SiteEntry);
		while(SiteEntrys.size() > 10)
			SiteEntrys.removeLast();
		theGUI->Cfg()->SetSetting("Gui/Widget_Crawler_Entries", SiteEntrys);

		CCrawlerActionJob* pCrawlerActionJob = new CCrawlerActionJob(SiteName, "IndexSite");
		pCrawlerActionJob->SetField("SiteEntry", SiteEntry);
		pCrawlerActionJob->SetField("CrawlingDepth", IndexSite.m_pCrawlDepth->value());
		theGUI->ScheduleJob(pCrawlerActionJob);
		return true;
	}

protected:
	QComboBox*			m_pSiteEntry;
	QSpinBox*			m_pCrawlDepth;
	QDialogButtonBox*	m_pButtonBox;
	QFormLayout*		m_pSiteLayout;
};

void CCrawlerWindow::OnIndexSite()
{
	QTreeWidgetItem* pItem = m_pCrawlerTree->currentItem();
	if(!pItem || pItem->parent())
		return;

	QString SiteName = pItem->data(eSiteName, Qt::UserRole).toString();

	if(SSite* pSite = m_Sites.value(SiteName))
		CIndexSite::IndexSite(SiteName, pSite->SiteEntry, this);
}

class CNewSite: public QDialogEx
{
	//Q_OBJECT

public:
	CNewSite(QWidget *pMainWindow)
		: QDialogEx(pMainWindow)
	{
		setWindowTitle(CCrawlerWindow::tr("New Site"));

		m_pSiteLayout = new QFormLayout(this);

		m_pSiteName = new QLineEdit();
		m_pSiteName->setMaximumWidth(200);
		m_pSiteLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(CCrawlerWindow::tr("Site Name:")));
		m_pSiteLayout->setWidget(1, QFormLayout::FieldRole, m_pSiteName);

		m_pSiteEntry = new QLineEdit();
		m_pSiteEntry->setMaximumWidth(200);
		m_pSiteLayout->setWidget(2, QFormLayout::LabelRole, new QLabel(CCrawlerWindow::tr("Site Entry:")));
		m_pSiteLayout->setWidget(2, QFormLayout::FieldRole, m_pSiteEntry);

		m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
		QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
		QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
		m_pSiteLayout->setWidget(3, QFormLayout::FieldRole, m_pButtonBox);
	}

	static bool	AddSite(QWidget *pMainWindow = NULL)
	{
		CNewSite NewSite(pMainWindow);
		if(!NewSite.exec())
			return false;

		CCrawlerActionJob* pCrawlerActionJob = new CCrawlerActionJob(NewSite.m_pSiteName->text(), "AddSite");
		pCrawlerActionJob->SetField("SiteEntry", NewSite.m_pSiteEntry->text());
		theGUI->ScheduleJob(pCrawlerActionJob);
		return true;
	}

protected:
	QLineEdit*			m_pSiteName;
	QLineEdit*			m_pSiteEntry;
	QDialogButtonBox*	m_pButtonBox;
	QFormLayout*		m_pSiteLayout;
};

void CCrawlerWindow::OnAddSite()
{
	CNewSite::AddSite(this);
}

void CCrawlerWindow::OnRemoveSite()
{
	QTreeWidgetItem* pItem = m_pCrawlerTree->currentItem();
	if(!pItem || pItem->parent())
		return;

	QString SiteName = pItem->data(eSiteName, Qt::UserRole).toString();

	if(QMessageBox(tr("Remove Site"), tr("Remove Site %1").arg(SiteName)
	 , QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;
	
	CCrawlerActionJob* pCrawlerActionJob = new CCrawlerActionJob(SiteName, "RemoveSite");
	theGUI->ScheduleJob(pCrawlerActionJob);
}

#endif