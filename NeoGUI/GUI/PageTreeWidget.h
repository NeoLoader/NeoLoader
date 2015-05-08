#pragma once

class CSearchesSyncJob;
class CGrabbingsSyncJob;
class CTreeLabelItem;

class CPageTreeWidget: public QWidget
{
	Q_OBJECT

public:
	CPageTreeWidget(bool bPlayer, QWidget *parent = 0);
	~CPageTreeWidget();

	enum EPages
	{
		eUndefine = 0,
		eSummary,
		eDownloads,
		eSearches,
		eGrabber,
		eShared,
		eOnLine, 
		eArchive,
		eTransfers,
		eClients,
		eWebTasks,
		ePlayer,
#ifdef CRAWLER
		eCrawler,
#endif
		eServices,
		eHosters,
		eServers,
		eSolvers,
		eFinders,

		eStatistics,

		eStarted,
		ePaused,
		eStopped,
		eCompleted,

		eUploads,
	};

	void				SetPage(int PageID, int PageSubID = 0);

	void				AddPlayer(uint64 ID, const QString& FileName);
	void				AddOnline(QWidget* pWnd, const QString& Expression);

#if QT_VERSION >= 0x050000
	uint64				GetPlayID()					{return m_PlayingNow;}
	QString				GetPlayName(uint64 TabID)	{return m_PlayingNames.value(TabID);}
#endif

signals:
	void				PageChanged(int PageID, uint64 TabID);
	void				PageClosed(int PageID, uint64 TabID);

public slots:
	//void				OnMenuRequested(const QPoint &point);
	void				OnItemClicked(QTreeWidgetItem* pItem, int Column);
	//void				OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column);
	void				OnSelectionChanged();
	void				OnCloseButton();

protected:
	friend class CSearchesSyncJob;
	friend class CGrabbingsSyncJob;
	CSearchesSyncJob*	m_pSearchesSyncJob;
	CGrabbingsSyncJob*	m_pGrabbingsSyncJob;
	void				SyncSearches(const QVariantMap& Response);
	void				SyncTasks(const QVariantMap& Response);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	struct SSearch
	{
		//QString				Expression;
		//QString				SearchNet;
		//QVariantMap			Criteria;
		QString				Status;
		CTreeLabelItem*		pLabel;
	};
	QMap<uint64, SSearch*>	m_Searches;

	struct STask
	{
		//QStringList			Urls;
		QString				Status;
		CTreeLabelItem*		pLabel;
	};
	QMap<uint64, STask*>	m_Tasks;

	QVBoxLayout*		m_pMainLayout;

	QTreeWidget*		m_pPageTree;

	QTreeWidgetItem*	m_pSummary;
	QTreeWidgetItem*	m_pDownloads;
	QTreeWidgetItem*	m_pSearches;
	QTreeWidgetItem*	m_pGrabber;
	QTreeWidgetItem*	m_pShared;
	QTreeWidgetItem*	m_pArchive;
	QTreeWidgetItem*	m_pTransfers;
	QTreeWidgetItem*	m_pClients;
	QTreeWidgetItem*	m_pWebTasks;
	QTreeWidgetItem*	m_pPlayer;
#ifdef CRAWLER
	QTreeWidgetItem*	m_pCrawler;
#endif

	QTreeWidgetItem*	m_pServices;
	QTreeWidgetItem*	m_pHosters;
	QTreeWidgetItem*	m_pServers;
	QTreeWidgetItem*	m_pSolvers;
	QTreeWidgetItem*	m_pFinders;

	QTreeWidgetItem*	m_pStatistics;

	uint64				m_PlayingNow;
#if QT_VERSION >= 0x050000
	QMap<uint64, QString> m_PlayingNames;
#endif
	QList<QWidget*>		m_OnLine;
};