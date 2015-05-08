#pragma once

class CIncrementalPlot;

class CStatisticsWindow: public QWidget
{
	Q_OBJECT

public:
	CStatisticsWindow(QWidget *parent = 0);
	~CStatisticsWindow();

private slots:
	void				OnMenuRequested(const QPoint &point);

	void				OnCopy();
	void				OnCopyAll();
	void				OnReset();

protected:
	void				UpdateStats(const QVariantMap& Response);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	QVBoxLayout*		m_pMainLayout;
	QSplitter*			m_pSplitter;

	QWidget*			m_pStatisticsWidget;
	QVBoxLayout*		m_pStatisticsLayout;

	CIncrementalPlot*	m_DownRate;
	CIncrementalPlot*	m_UpRate;
	CIncrementalPlot*	m_Connections;

	QTreeWidget*		m_pStatisticsTree;

	QTreeWidgetItem*	m_pBandwidth;
	QTreeWidgetItem*		m_pUpRate;
	QTreeWidgetItem*		m_pDownRate;
	QTreeWidgetItem*		m_pUpVolume;
	QTreeWidgetItem*		m_pDownVolume;
	QTreeWidgetItem*		m_pCons;
	QTreeWidgetItem*		m_pPinger;
	QTreeWidgetItem*			m_pPingHost;
	QTreeWidgetItem*			m_pPingValue;
	QTreeWidgetItem*		m_pPeerWatch;
	QTreeWidgetItem*	m_pNetworks;
	QTreeWidgetItem*		m_pNeoShare;
	QTreeWidgetItem*			m_pNeoShareUpRate;
	QTreeWidgetItem*			m_pNeoShareDownRate;
	QTreeWidgetItem*			m_pNeoShareUpVolume;
	QTreeWidgetItem*			m_pNeoShareDownVolume;
	QTreeWidgetItem*			m_pNeoShareKadStatus;
	QTreeWidgetItem*			m_pNeoSharePort;
	QTreeWidgetItem*			m_pNeoShareIP;
	QTreeWidgetItem*			m_pNeoShareCons;
	QTreeWidgetItem*			m_pNeoShareFirewalled;
	QTreeWidgetItem*			m_pNeoShareRoutes;
	QTreeWidgetItem*		m_pBitTorrent;
	QTreeWidgetItem*			m_pTorrentUpRate;
	QTreeWidgetItem*			m_pTorrentDownRate;
	QTreeWidgetItem*			m_pTorrentUpVolume;
	QTreeWidgetItem*			m_pTorrentDownVolume;
	QTreeWidgetItem*			m_pTorrentPort;
	QTreeWidgetItem*			m_pTorrentIP;
	QTreeWidgetItem*			m_pTorrentCons;
	QTreeWidgetItem*			m_pTorrentFirewalled;
	QTreeWidgetItem*			m_pDHTStats;
	QTreeWidgetItem*				m_pDHTNodes;
	QTreeWidgetItem*				m_pDHTGlobalNodes;
	QTreeWidgetItem*				m_pDHTLookups;
	QTreeWidgetItem*		m_pEd2kMule;
	QTreeWidgetItem*			m_pMuleUpRate;
	QTreeWidgetItem*			m_pMuleDownRate;
	QTreeWidgetItem*			m_pMuleUpVolume;
	QTreeWidgetItem*			m_pMuleDownVolume;
	QTreeWidgetItem*			m_pMulePort;
	QTreeWidgetItem*			m_pMulePortUDP;
	QTreeWidgetItem*			m_pMuleIP;
	QTreeWidgetItem*			m_pMuleCons;
	QTreeWidgetItem*			m_pMuleFirewalled;
	QTreeWidgetItem*			m_pMuleKadStatus;
	QTreeWidgetItem*			m_pMuleKadPort;
	QTreeWidgetItem*			m_pMuleKadStats;
	QTreeWidgetItem*				m_pMuleKadTotalUsers;
	QTreeWidgetItem*				m_pMuleKadTotalFiles;
	QTreeWidgetItem*				m_pMuleKadIndexedSource;
	QTreeWidgetItem*				m_pMuleKadIndexedKeyword;
	QTreeWidgetItem*				m_pMuleKadIndexedNotes;
	QTreeWidgetItem*				m_pMuleKadIndexLoad;
	QTreeWidgetItem*	m_pHosters;
	QTreeWidgetItem*		m_pHostersUpRate;
	QTreeWidgetItem*		m_pHostersDownRate;
	QTreeWidgetItem*		m_pHostersUpVolume;
	QTreeWidgetItem*		m_pHostersDownVolume;
	QTreeWidgetItem*		m_pHostersUploads;
	QTreeWidgetItem*		m_pHostersDownloads;
	QTreeWidgetItem*	m_pIOStats;
	QTreeWidgetItem*		m_pPendingRead;
	QTreeWidgetItem*		m_pPendingWrite;
	QTreeWidgetItem*		m_pHashingCount;
	QTreeWidgetItem*		m_pAllocationCount;

	QMenu*				m_pMenu;

	QAction*			m_pCopy;
	QAction*			m_pCopyAll;
	QAction*			m_pReset;
};
