#pragma once

class CServerSyncJob;
class CPropertiesView;

class CP2PServers: public QWidget
{
	Q_OBJECT

public:
	CP2PServers(QWidget *parent = 0);
	~CP2PServers();


public slots:
	void				OnCopyUrl();
	void				OnAddServer();
	void				OnRemoveServer()		{OnServer("RemoveServer");}
	void				OnConnectServer()		{OnServer("ConnectServer");}
	void				OnDisconnectServer()	{OnServer("DisconnectServer");}
	void				OnSetStaticServer();

	void				OnServer(const QString& Action);

private slots:
	void				OnMenuRequested(const QPoint &point);
	void				OnItemClicked(QTreeWidgetItem* pItem, int Column);
	void				OnSelectionChanged();

protected:
	friend class CServerSyncJob;
	void				SyncServers();
	void				SyncServers(const QVariantMap& Response);

	virtual void		timerEvent(QTimerEvent *e);

	int					m_TimerId;
	
	CServerSyncJob*		m_pServersSyncJob;

	struct SServer
	{
		QTreeWidgetItem* pItem;
	};

	QMap<QString, SServer*>	m_Servers;

	enum EColumns
	{
		eUrl = 0,
		eName,
		eVersion,
		eStatus,
		eUsers,
		eFiles,
		eDescription
	};

	QVBoxLayout*		m_pMainLayout;
	QSplitter*			m_pSplitter;

	QWidget*			m_pServerWidget;
	QVBoxLayout*		m_pServerLayout;

	QTreeWidget*		m_pServerTree;

	QTabWidget*			m_pServerTabs;

	QMenu*				m_pMenu;
	QAction*			m_pCopyUrl;
	QAction*			m_pAddServer;
	QAction*			m_pRemoveServer;
	QAction*			m_pSetStaticServer;
	QAction*			m_pConnectServer;
	QAction*			m_pDisconnectServer;
};
