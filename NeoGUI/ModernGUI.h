#pragma once

class CNeoSummary;
class CFileListWidget;
class CSearchWindow;
class CGrabberWindow;
class CStatisticsWindow;
class CServicesWidget;
class CTransfersView;
#ifdef CRAWLER
class CCrawlerWindow;
#endif
class CPageTreeWidget;
class COnlineFileList;
class CServiceSummary;
class CP2PServers;
class CWebTaskView;

class CModernGUI: public QWidget
{
	Q_OBJECT
public:
	CModernGUI(QWidget* parent = 0);
	~CModernGUI();
	
	static void			CoreAction(const QString& Action);

private slots:
	void				OnPageChanged(int PageID, uint64 TabID);
	void				OnPageClosed(int PageID, uint64 TabID);

	void 				OnAddLinks();
	void 				OnAddFile();
	void				OnScanShare();
	void 				OnClearCompleted();
	void 				OnDetailTabs();
	void 				OnSearch();

	void				OnSearchNet(QAction* pAction);

	void				OnStreamFile(uint64 ID, const QString& FileName, bool bComplete);
	void				OnTogleDetails();

	void				OnOnLine(const QString& Expression);

protected:
	QVBoxLayout*		m_pMainLayout;

	QSplitter*			m_pSplitter;

	CPageTreeWidget*	m_pPageTree;

	QToolBar*			m_pToolBar;

	QAction*			m_pAddLinks;
	QAction*			m_pAddFile;

	QAction*			m_pScanShare;
	QAction*			m_pClearCompleted;

	QAction*			m_pDetailTabs;

	QAction*			m_pSettings;

	QLineEdit*			m_pSearchExp;
	QToolButton*		m_pSearchBtn;

	QMenu*				m_pSearchMenu;

	QActionGroup*		m_pDownload;
	QAction*			m_pAll;
	QAction*			m_pNeo;
	QAction*			m_pTorrent;
	QAction*			m_pLinks;
	QAction*			m_pEd2k;

	QStackedWidget*		m_pStack;

	CNeoSummary*		m_pSummaryWnd;
	CFileListWidget*	m_pFileListWnd;
	CSearchWindow*		m_pSearchWnd;
	QList<COnlineFileList*> m_pOnlineWnds;
	CGrabberWindow*		m_pGrabberWnd;
	CTransfersView*		m_pTransferListWnd;
	CWebTaskView*		m_pWebTaskViewWnd;
	QWidget*			m_pPlayerWnd;
#ifdef CRAWLER
	CCrawlerWindow*		m_pCrawlerWnd;
#endif
	CServiceSummary*	m_pServiceWnd;
	CServicesWidget*	m_pServicesWnd;
	CP2PServers*		m_pServersWnd;
	CStatisticsWindow*	m_pStatisticsWnd;

	typedef QWidget* (*CreatePlayerFkt)();
	CreatePlayerFkt		CreatePlayer;
};
