#pragma once

class CHostingSyncJob;
class CProgressBar;

class CHostingView: public QWidget
{
	Q_OBJECT

public:
	CHostingView(QWidget *parent = 0);
	~CHostingView();

	void				ShowHosting(uint64 ID);
	void				ChangeMode(UINT Mode)		{m_Mode = Mode;}

private slots:
	void				OnUpload();
	void				OnCheck();
	void				OnReUpload();
	void				OnAddArch();
	void				OnDelArch();
	void				OnAddLink();
	void				OnDelLink();
	void				OnCopyUrl();

public slots:
	void				OnMenuRequested(const QPoint &point);


protected:
	friend class CHostingSyncJob;

	void				SyncHosting();
	void				SyncHosting(const QVariantMap& Response);
	void				SyncHosters(const QVariantList& Hosters, QTreeWidgetItem* pRoot);
	void				SyncUsers(const QVariantList& Users, QTreeWidgetItem* pItem);
	void				SyncLinks(const QVariantList& Links, QTreeWidgetItem* pItem);
	void				DeleteRecursivly(QTreeWidgetItem* pCurItem);

	QSet<QTreeWidgetItem*> GetLinks();
	QSet<QTreeWidgetItem*> GetLinks(QTreeWidgetItem* pItem);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;

	uint64				m_ID;
	UINT				m_Mode;

	CHostingSyncJob*	m_pHostingSyncJob;

	enum EColumns
	{
		eName,
		eStatus,
		eProgress,
		eSharing
	};

	enum EType
	{
		eGroupe,
		eHoster,
		eUser,
		eLink
	};

	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pHostingWidget;
	QFormLayout*		m_pHostingLayout;

	QTreeWidget*		m_pHosting;

	QMenu*				m_pMenu;
	QAction*			m_pUpload;
	QAction*			m_pCheck;
	QAction*			m_pReUpload;
	QAction*			m_pAddArch;
	QAction*			m_pDelArch;
	QAction*			m_pAddLink;
	QAction*			m_pDelLink;
	QAction*			m_pCopyUrl;

	QMap<QTreeWidgetItem*, CProgressBar*> m_ProgressMap;
};