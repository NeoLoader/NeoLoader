#include "GlobalHeader.h"
#include "P2PServers.h"
#include "../NeoGUI.h"

CP2PServers::CP2PServers(QWidget *parent)
:QWidget(parent)
{
	m_pServersSyncJob = NULL;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(1);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);

	m_pServerWidget = new QWidget();
	m_pServerLayout = new QVBoxLayout();
	m_pServerLayout->setMargin(0);

	m_pServerTree = new QTreeWidget();
	m_pServerTree->setHeaderLabels(tr("Url|Name|Version|Status|Users|Files|Description").split("|"));
	m_pServerTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pServerTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	//connect(m_pServerTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pServerTree, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));
	m_pServerTree->setSortingEnabled(true);

	m_pMenu = new QMenu();

	
	m_pCopyUrl = new QAction(tr("Copy Url"), m_pMenu);
	connect(m_pCopyUrl, SIGNAL(triggered()), this, SLOT(OnCopyUrl()));
	m_pMenu->addAction(m_pCopyUrl);

	m_pAddServer = new QAction(tr("Add Server"), m_pMenu);
	connect(m_pAddServer, SIGNAL(triggered()), this, SLOT(OnAddServer()));
	m_pMenu->addAction(m_pAddServer);

	m_pRemoveServer = new QAction(tr("Remove Server"), m_pMenu);
	connect(m_pRemoveServer, SIGNAL(triggered()), this, SLOT(OnRemoveServer()));
	m_pMenu->addAction(m_pRemoveServer);

	m_pMenu->addSeparator();

	m_pConnectServer = new QAction(tr("Connect Server"), m_pMenu);
	connect(m_pConnectServer, SIGNAL(triggered()), this, SLOT(OnConnectServer()));
	m_pMenu->addAction(m_pConnectServer);

	m_pDisconnectServer = new QAction(tr("Disconnect Server"), m_pMenu);
	connect(m_pDisconnectServer, SIGNAL(triggered()), this, SLOT(OnDisconnectServer()));
	m_pMenu->addAction(m_pDisconnectServer);

	m_pMenu->addSeparator();

	m_pSetStaticServer = new QAction(tr("Static Server"), m_pMenu);
	m_pSetStaticServer->setCheckable(true);
	connect(m_pSetStaticServer, SIGNAL(triggered()), this, SLOT(OnSetStaticServer()));
	m_pMenu->addAction(m_pSetStaticServer);

	m_pServerLayout->addWidget(m_pServerTree);

	m_pServerWidget->setLayout(m_pServerLayout);
	m_pSplitter->addWidget(m_pServerWidget);

	//m_pServerTabs = new QTabWidget();
	//m_pSplitter->addWidget(m_pServerTabs);

	m_pMainLayout->addWidget(m_pSplitter);

	setLayout(m_pMainLayout);

	m_pSplitter->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Servers_Spliter"));
	m_pServerTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Servers_Columns"));

	m_TimerId = startTimer(500);
}

CP2PServers::~CP2PServers()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Servers_Spliter",m_pSplitter->saveState());
	theGUI->Cfg()->SetBlob("Gui/Widget_Servers_Columns",m_pServerTree->header()->saveState());

	killTimer(m_TimerId);

	foreach(SServer* pServer, m_Servers)
		delete pServer;
}

class CServerSyncJob: public CInterfaceJob
{
public:
	CServerSyncJob(CP2PServers* pView)
	{
		m_pView = pView;
	}

	virtual QString			GetCommand()	{return "ServerList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncServers(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pServersSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CP2PServers>m_pView; // Note: this can be deleted at any time
};

void CP2PServers::SyncServers()
{
	if(m_pServersSyncJob == NULL)
	{
		m_pServersSyncJob = new CServerSyncJob(this);
		theGUI->ScheduleJob(m_pServersSyncJob);
	}
}

void CP2PServers::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	SyncServers();
}

void CP2PServers::SyncServers(const QVariantMap& Response)
{
	QMap<QString, SServer*> OldServers = m_Servers;

	foreach (const QVariant& vServer,Response["Servers"].toList())
	{
		QVariantMap Server = vServer.toMap();

		QString Url = Server["Url"].toString();

		SServer* pServer = OldServers.take(Url);
		if(!pServer)
		{
			pServer = new SServer();
			pServer->pItem = new QTreeWidgetItem();
			m_pServerTree->addTopLevelItem(pServer->pItem);

			pServer->pItem->setText(eUrl, Url);

			m_Servers.insert(Url, pServer);
		}

		QFont Font = pServer->pItem->font(eUrl);
		if(Font.bold() != Server["IsStatic"].toBool())
		{
			Font.setBold(Server["IsStatic"].toBool());
			pServer->pItem->setFont(eUrl, Font);
		}

		pServer->pItem->setText(eName, Server["Name"].toString());
		pServer->pItem->setData(eName, Qt::UserRole, Server["IsStatic"]);
		pServer->pItem->setText(eVersion, Server["Version"].toString());
		pServer->pItem->setText(eStatus, Server["Status"].toString());
		pServer->pItem->setData(eStatus, Qt::UserRole, Server["Status"]);
		pServer->pItem->setText(eUsers, Server["UserCount"].toString() + "(" + Server["LowIDCount"].toString() + ")/" + Server["UserLimit"].toString());
		pServer->pItem->setText(eFiles, Server["FileCount"].toString() + "|" + Server["HardLimit"].toString() + "(" + Server["SoftLimit"].toString() + ")");
		pServer->pItem->setText(eDescription, Server["Description"].toString());
	}

	foreach(SServer* pServer, OldServers)
	{
		m_Servers.remove(OldServers.key(pServer));
		delete pServer->pItem;
		delete pServer;
	}
}

void CP2PServers::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{

}

void CP2PServers::OnSelectionChanged()
{
	if(QTreeWidgetItem* pItem = m_pServerTree->currentItem())
		OnItemClicked(pItem, 0);
}

void CP2PServers::OnMenuRequested(const QPoint &point)
{
	QTreeWidgetItem* pItem = m_pServerTree->currentItem();

	m_pRemoveServer->setEnabled(pItem);
	m_pSetStaticServer->setEnabled(pItem);
	m_pSetStaticServer->setChecked(pItem ? pItem->data(eName, Qt::UserRole).toBool() : false);
	m_pConnectServer->setEnabled(pItem ? pItem->data(eStatus, Qt::UserRole).toString() == "Disconnected" : false);
	m_pDisconnectServer->setEnabled(pItem ? pItem->data(eStatus, Qt::UserRole).toString() == "Connected" : false);

	m_pMenu->popup(QCursor::pos());	
}

class CServerActionJob: public CInterfaceJob
{
public:
	CServerActionJob(const QString& Url, const QString& Action)
	{
		m_Request["Action"] = Action;
		m_Request["Log"] = true;
		m_Request["Url"] = Url;
	}

	void SetField(const QString& Key, const QVariant& Value) {m_Request[Key] = Value;}
	virtual QString			GetCommand()	{return "ServerAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CP2PServers::OnCopyUrl()
{
	QApplication::clipboard()->setText(m_pServerTree->currentItem()->text(eUrl));
}

void CP2PServers::OnAddServer()
{
	QString Url = QInputDialog::getText(NULL, tr("Enter Server Url"), tr("Enter the URL of a new server to be added"), QLineEdit::Normal);
	if(Url.isEmpty())
		return;

	CServerActionJob* pServerActionJob = new CServerActionJob(Url, "AddServer");
	theGUI->ScheduleJob(pServerActionJob);
}

void CP2PServers::OnServer(const QString& Action)
{
	CServerActionJob* pServerActionJob = new CServerActionJob(m_pServerTree->currentItem()->text(eUrl), Action);
	theGUI->ScheduleJob(pServerActionJob);
}

void CP2PServers::OnSetStaticServer()	
{
	CServerActionJob* pServerActionJob = new CServerActionJob(m_pServerTree->currentItem()->text(eUrl), "SetStaticServer");
	pServerActionJob->SetField("Static", m_pSetStaticServer->isChecked());
	theGUI->ScheduleJob(pServerActionJob);
}