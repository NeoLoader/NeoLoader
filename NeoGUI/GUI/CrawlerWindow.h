#pragma once

class CCrawlerSyncJob;
class CCrawlerEntry;
class CCrawlerSite;

class CCrawlerWindow: public QWidget
{
	Q_OBJECT

#ifdef CRAWLER
public:
	CCrawlerWindow(QWidget *parent = 0);
	~CCrawlerWindow();

	void				Suspend(bool bSet);

private slots:
	void				OnMenuRequested(const QPoint &point);
	void				OnItemClicked(QTreeWidgetItem* pItem, int Column);
	void				OnSelectionChanged();

	void 				OnItemExpanded(QTreeWidgetItem * item);
	void				OnValueChanged(int i)				{OnFilterChanged();}
	void				OnCurrentIndexChanged(int index)	{OnFilterChanged();}	
	void				OnReturnPressed()					{OnFilterChanged();}
	void				OnFilterChanged();


	void				OnAddSite();
	void				OnIndexSite();
	void				OnCrawlSite()	{OnAction("CrawlSite");}
	void				OnStopTasks()	{OnAction("StopTasks");}
	void				OnBlastKad()	{OnAction("BlastKad");}
	void				OnAction(const QString& Action);
	void				OnRemoveSite();

protected:
	friend class CCrawlerSyncJob;
	void				SyncCrawler();
	void				SyncCrawler(const QVariantMap& Response);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;
	
	CCrawlerSyncJob*	m_pCrawlerSyncJob;

	enum EColumns
	{
		eSiteName = 0,
		eEntries,
		eSites,
		eActive,
		eLast,
		eKeywords,
		eFiles,
	};

	struct SEntry
	{
		QString FileName;
		QVariantMap Details;
		QStringList Links;
		QMap<QString, QString> Hashes;
		QTreeWidgetItem* pItem;
	};

	struct SSite
	{
		SSite(){
			Pages = 0;
			Page = 1;
			EntryCount = 0;
		}
		~SSite(){
			foreach(SEntry* pEntry, Entries)
				delete pEntry;
		}

		int Pages;
		int Page;
		int EntryCount;
		QTreeWidgetItem* pItem;

		QString SiteEntry;

		QMap<QString, SEntry*> Entries;
	};

	SSite*				GetCurSite();
	void				UpdatePage(SSite* pSite);

	void				SyncEntries(SSite* pSite, const QVariantList& Entries);
	QTreeWidgetItem*	GetDir(QTreeWidgetItem* pPrev, const QStringList& Path, int n);
	void				ClearDir(QTreeWidgetItem* pPrev);

	bool				m_UpdateFilter;

	QMap<QString, SSite*>	m_Sites;

	QVBoxLayout*		m_pMainLayout;
	QSplitter*			m_pSplitter;

	QToolBar*			m_pToolBar;
	QAction*			m_pAddSite;
	QSpinBox*			m_pLimit;
	QSpinBox*			m_pPage;
	QLabel*				m_pPages;
	QComboBox*			m_pSortBy;
	QLineEdit*			m_pFilter;

	QTreeWidget*		m_pCrawlerTree;

	QWidget*			m_SubWidget;
	QStackedLayout*		m_SubLayout;

	CCrawlerEntry*		m_pCrawlerEntry;
	CCrawlerSite*		m_pCrawlerSite;

	QMenu*				m_pMenu;
	QAction*			m_pIndexSite;
	QAction*			m_pCrawlSite;
	QAction*			m_pStopTasks;
	QAction*			m_pBlastKad;
	QAction*			m_pRemoveSite;
#endif
};