#pragma once

class CTransferSyncJob;
class CProgressBar;
class CTransfersModel;
class QTreeViewEx;

class CTransfersView: public QWidget
{
	Q_OBJECT

public:
	enum EMode
	{
		eTransfers = 0,
		eActive,
		eDownloads,
		eUploads,
		eClients
	};

	CTransfersView(UINT Mode = 0, QWidget *parent = 0);
	~CTransfersView();

	void				ShowTransfers(uint64 ID);

	void				ChangeMode(UINT Mode);

	static void			OnAction(const QString& Action, uint64 ID, uint64 SubID);

signals:
	void				OpenTransfer(uint64, uint64);

private slots:
	void				OnScroll();

	void				OnMenuRequested(const QPoint &point);

	void				OnDoubleClicked(const QModelIndex& Index);

	void				OnStartDownload()	{ OnAction("StartDownload"); }
	void				OnStartUpload()		{ OnAction("StartUpload"); }
	void				OnCancelTransfer()	{OnAction("CancelTransfer");}
	void				OnClearError()		{OnAction("ClearError");}
	void				OnRemoveTransfer()	{OnAction("RemoveTransfer");}

	void				OnCopyUrl();

protected:
	friend class CTransferSyncJob;
	void				SyncTransfers(const QVariantMap& Response, bool bFull);

	void				OnAction(const QString& Action);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;
	uint64				m_ID;
	UINT				m_Mode;
	QString				m_Ops;
	uint64				m_SyncToken;
	QSet<uint64>		m_VisibleTransfers;
	uint64				m_NextFullUpdate;

	CTransferSyncJob*	m_pTransferSyncJob;

	QVBoxLayout*		m_pMainLayout;

	QMenu*				m_pMenu;
	QAction*			m_StartDownload;
	QAction*			m_StartUpload;
	QAction*			m_CancelTransfer;
	QAction*			m_ClearError;
	QAction*			m_RemoveTransfer;
	QAction*			m_CopyUrls;

	QTreeViewEx*		m_pTransferTree;
	CTransfersModel*	m_pTransferModel;
	QSortFilterProxyModel* m_pSortProxy;
	QMap<uint64, QPair<QPointer<QWidget>, QPersistentModelIndex> > m_ProgressMap;
};
