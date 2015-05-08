#include "GlobalHeader.h"
#include "ServicesWidget.h"
#include "HosterView.h"
#include "AccountView.h"
#include "PropertiesView.h"
#include "../NeoGUI.h"

CServicesWidget* g_Services = NULL;

CServicesWidget::CServicesWidget(QWidget *parent)
:QWidget(parent)
{
	g_Services = this;

	m_Mode = eUnknown;
	m_pHostersSyncJob = NULL;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(1);

	m_pFilterWidget = new QWidget();
	m_pFilterLayout = new QGridLayout();
	m_pFilterLayout->setMargin(3);
	m_pFilterLayout->setAlignment(Qt::AlignLeft);

	m_pHostFilter = new QLineEdit();
	m_pHostFilter->setMaximumWidth(200);
	m_pFilterLayout->addWidget(m_pHostFilter, 0, 0);

	m_pAccountsOnly = new QCheckBox(tr("Only with Accounts"));
	m_pFilterLayout->addWidget(m_pAccountsOnly, 0, 1);

	m_pFilterWidget->setLayout(m_pFilterLayout);
	m_pFilterWidget->setMaximumWidth(300);

	m_pMainLayout->addWidget(m_pFilterWidget);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);

	m_pHostsWidget = new QWidget();
	m_pHostsLayout = new QVBoxLayout();
	m_pHostsLayout->setMargin(0);

	m_pHosterTree = new QTreeWidget();
	m_pHosterTree->setHeaderLabels(tr("HostName|Status|APIs").split("|"));
	m_pHosterTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pHosterTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));
	//connect(m_pHosterTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pHosterTree, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));
	m_pHosterTree->setSortingEnabled(true);

	m_pMenu = new QMenu();

	m_pAddAccount = new QAction(tr("Add Account"), m_pMenu);
	connect(m_pAddAccount, SIGNAL(triggered()), this, SLOT(OnAddAccount()));
	m_pMenu->addAction(m_pAddAccount);

	m_pRemoveAccount = new QAction(tr("Remove Account"), m_pMenu);
	connect(m_pRemoveAccount, SIGNAL(triggered()), this, SLOT(OnRemoveAccount()));
	m_pMenu->addAction(m_pRemoveAccount);

	m_pCheckAccount = new QAction(tr("Check Account"), m_pMenu);
	connect(m_pCheckAccount, SIGNAL(triggered()), this, SLOT(OnCheckAccount()));
	m_pMenu->addAction(m_pCheckAccount);


	m_pHostsLayout->addWidget(m_pHosterTree);

	m_pHostsWidget->setLayout(m_pHostsLayout);
	m_pSplitter->addWidget(m_pHostsWidget);

	m_pHosterTabs = new QTabWidget();

	m_pHostWidget = new QWidget();
	m_pHostLayout = new QStackedLayout(m_pHostWidget);
	m_pHostLayout->addWidget(new QWidget()); // blank on top

	m_HosterView = new CHosterView();
	m_pHostLayout->addWidget(m_HosterView);

	m_AccountView = new CAccountView();
	m_pHostLayout->addWidget(m_AccountView);

	m_pHostWidget->setLayout(m_pHostLayout);
	m_pHosterTabs->addTab(m_pHostWidget, tr("Details"));

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
	{
		m_pProperties = new CPropertiesView(true);
		m_pHosterTabs->addTab(m_pProperties, tr("Properties"));
	}
	else
		m_pProperties = NULL;

	m_pSubWidget = new QWidget();
	m_pSubLayout = new QVBoxLayout();
	m_pSubLayout->setMargin(0);

	m_pSubLayout->addWidget(m_pHosterTabs);

	m_pSubWidget->setLayout(m_pSubLayout);

	m_pSplitter->addWidget(m_pSubWidget);

	m_pMainLayout->addWidget(m_pSplitter);

	setLayout(m_pMainLayout);

	m_pSplitter->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Hosters_Spliter"));
	m_pHosterTree->header()->restoreState(theGUI->Cfg()->GetBlob("Gui/Widget_Hosters_Columns"));

	m_TimerId = startTimer(500);
}

CServicesWidget::~CServicesWidget()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Hosters_Spliter",m_pSplitter->saveState());
	theGUI->Cfg()->SetBlob("Gui/Widget_Hosters_Columns",m_pHosterTree->header()->saveState());

	killTimer(m_TimerId);

	foreach(SHoster* pHoster, m_Hosters)
		delete pHoster;
}

void CServicesWidget::SetMode(EMode Mode)
{
	if(m_Mode == Mode)
		return;

	m_Mode = Mode;
	m_HosterView->SetMode(Mode);
	m_AccountView->SetMode(Mode);

	m_pAccountsOnly->setChecked(false);
	m_pHostFilter->clear();

	UpdateTree();
}

class CHostersSyncJob: public CInterfaceJob
{
public:
	CHostersSyncJob(CServicesWidget* pView)
	{
		m_pView = pView;
	}

	virtual QString			GetCommand()	{return "ServiceList";}
	virtual void			HandleResponse(const QVariantMap& Response)
	{
		if(m_pView)
			m_pView->SyncHosters(Response);
	}
	virtual void			Finish(bool bOK)
	{
		if(m_pView)
			m_pView->m_pHostersSyncJob = NULL;
		CInterfaceJob::Finish(bOK);
	}

protected:
	QPointer<CServicesWidget>m_pView; // Note: this can be deleted at any time
};

void CServicesWidget::SyncHosters()
{
	if(m_pHostersSyncJob == NULL)
	{
		m_pHostersSyncJob = new CHostersSyncJob(this);
		theGUI->ScheduleJob(m_pHostersSyncJob);
	}
}

void CServicesWidget::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_TimerId) 
	{
        QObject::timerEvent(e);
		return;
    }

	SyncHosters();
}

void CServicesWidget::SyncHosters(const QVariantMap& Response)
{
	m_Services.clear();

	foreach (const QVariant vHoster, Response["Services"].toList())
	{
		QVariantMap Hoster = vHoster.toMap();
		QStringList APIs = Hoster["APIs"].toStringList();
		
		if(APIs.contains("Upload") || APIs.contains("Download") || APIs.contains("Check")) // Hoster APIs
			m_Services.insert(eHosters, Hoster);
		else if(APIs.contains("Solve")) // Hoster Captcha Solver APIs
			m_Services.insert(eSolvers, Hoster);	
		else if(APIs.contains("Search")) // Link/Search Site APIs
			m_Services.insert(eFinders, Hoster);
		else if(APIs.contains("Decrypt")) // Cryptosite APIs also encountered with Multi Hosters
			m_Services.insert(eHosters, Hoster);
	}

	UpdateTree();
}

void CServicesWidget::UpdateTree()
{
	bool bAccOnly = m_pAccountsOnly->isChecked();
	QString Filter = m_pHostFilter->text();

	QMap<QString, SHoster*> OldHosters = m_Hosters;

	foreach (const QVariantMap& Hoster, m_Services.values(m_Mode))
	{
		QString Name = Hoster["HostName"].toString();

		QVariantList Logins = Hoster["Logins"].toList();
		if(bAccOnly && Logins.isEmpty())
			continue;
		if(!Filter.isEmpty() && !Name.contains(Filter))
			continue;

		SHoster* pHoster = OldHosters.take(Name);
		if(!pHoster)
		{
			pHoster = new SHoster();
			pHoster->pItem = new QTreeWidgetItem();
			m_pHosterTree->addTopLevelItem(pHoster->pItem);

			pHoster->pItem->setText(eName, Name);

			m_Hosters.insert(Name, pHoster);
		}
		if(pHoster->pItem->icon(eName).isNull())
			pHoster->pItem->setIcon(eName, theGUI->GetHosterIcon(Name, false));

		QFont Font = pHoster->pItem->font(eName);
		if(Font.bold() != !Logins.isEmpty())
		{
			Font.setBold(!Logins.isEmpty());
			pHoster->pItem->setFont(eName, Font);
		}


		pHoster->pItem->setText(eStatus, Hoster["Status"].toString());
		pHoster->pItem->setText(eAPIs, Hoster["APIs"].toStringList().join(", "));

		// logins:

		QMap<QString, QTreeWidgetItem*> OldAccounts;
		for(int i = 0; i < pHoster->pItem->childCount(); ++i) 
		{
			QTreeWidgetItem* pItem = pHoster->pItem->child(i);
			QString Login = pItem->text(eName);
			Q_ASSERT(!OldAccounts.contains(Login));
			OldAccounts.insert(Login,pItem);
		}

		foreach (const QVariant vLogin, Logins)
		{
			QVariantMap Login = vLogin.toMap();
			QString Account = Login["UserName"].toString();

			QTreeWidgetItem* pItem = OldAccounts.take(Account);
			if(!pItem)
			{
				pItem = new QTreeWidgetItem();
				pHoster->pItem->addChild(pItem);
				pHoster->pItem->setExpanded(true);
				
				pItem->setText(eName, Account);
			}

			pItem->setText(eStatus, Login["Status"].toString() + (Login["Free"].toBool() ? tr(" (Free)") : ""));
		}

		foreach(QTreeWidgetItem* pItem, OldAccounts)
			delete pItem;
	}

	foreach(SHoster* pHoster, OldHosters)
	{
		m_Hosters.remove(OldHosters.key(pHoster));
		delete pHoster->pItem;
		delete pHoster;
	}
}

void CServicesWidget::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	if(QTreeWidgetItem* pParent = pItem->parent())
	{
		QString Hoster = pParent->text(eName);
		QString Account = pItem->text(eName);
		m_pHostLayout->setCurrentWidget(m_AccountView);
		m_AccountView->ShowAccount(Hoster, Account);

		if(m_pProperties)
			m_pProperties->ShowHoster(Hoster, Account);
	}
	else
	{
		QString Hoster = pItem->text(eName);
		m_pHostLayout->setCurrentWidget(m_HosterView);
		m_HosterView->ShowHoster(Hoster);
		
		if(m_pProperties)
			m_pProperties->ShowHoster(Hoster);
	}
}

void CServicesWidget::OnSelectionChanged()
{
	if(QTreeWidgetItem* pItem = m_pHosterTree->currentItem())
		OnItemClicked(pItem, 0);
}

void CServicesWidget::OnMenuRequested(const QPoint &point)
{
	QTreeWidgetItem* pItem = m_pHosterTree->currentItem();
	if(!pItem)
		return;
	bool bRoot = pItem->parent() == NULL;

	m_pAddAccount->setEnabled(bRoot);
	m_pRemoveAccount->setEnabled(!bRoot);
	m_pCheckAccount->setEnabled(!bRoot);

	m_pMenu->popup(QCursor::pos());	
}

class CAccountActionJob: public CInterfaceJob
{
public:
	CAccountActionJob(const QString& HostName, const QString& UserName, const QString& Action)
	{
		m_Request["Action"] = Action;
		m_Request["Log"] = true;
		m_Request["HostName"] = HostName;
		m_Request["UserName"] = UserName;
	}

	void SetField(const QString& Key, const QVariant& Value) {m_Request[Key] = Value;}
	virtual QString			GetCommand()	{return "ServiceAction";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CServicesWidget::OnRemoveAccount()
{
	QTreeWidgetItem* pItem = m_pHosterTree->currentItem();
	if(!pItem)
		return;
	QTreeWidgetItem* pParent = pItem->parent();
	if(!pParent)
		return;
	
	QString Hoster = pParent->text(eName);
	QString Account = pItem->text(eName);

	if(QMessageBox(tr("Remove Account"), tr("Remove Account %1 for %2").arg(Account).arg(Hoster)
	 , QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;
	
	CAccountActionJob* pAccountActionJob = new CAccountActionJob(Hoster, Account, "ClearAccount");
	theGUI->ScheduleJob(pAccountActionJob);
}

void CServicesWidget::OnCheckAccount()
{
	QTreeWidgetItem* pItem = m_pHosterTree->currentItem();
	if(!pItem)
		return;
	QTreeWidgetItem* pParent = pItem->parent();
	if(!pParent)
		return;
	
	QString Hoster = pParent->text(eName);
	QString Account = pItem->text(eName);

	CAccountActionJob* pAccountActionJob = new CAccountActionJob(Hoster, Account, "CheckAccount");
	theGUI->ScheduleJob(pAccountActionJob);
}

void CServicesWidget::OnAddAccount()
{
	QTreeWidgetItem* pItem = m_pHosterTree->currentItem();
	if(!pItem)
		return;
	QString Hoster = pItem->text(eName);

	CNewAccount::AddAccount(Hoster, this);
}

void CServicesWidget::AddAccount(const QString& ServiceName, const QString& UserName, const QString& Password)
{
	CAccountActionJob* pAccountActionJob = new CAccountActionJob(ServiceName, UserName, "SetAccount");
	pAccountActionJob->SetField("Password", Password);
	theGUI->ScheduleJob(pAccountActionJob);
}

QStringList CServicesWidget::GetBestHosters()
{
	return QString("uploaded.to depositfiles.com share-online.biz mediafire.com").split(" ");
}

QStringList CServicesWidget::GetKnownHosters()
{
	QStringList Hosters;
	foreach (const QVariantMap& Hoster, m_Services.values(eHosters))
	{
		QString Name = Hoster["HostName"].toString();

		if(GetBestHosters().contains(Name))
			Hosters.prepend(Name);
		else
			Hosters.append(Name);
	}
	return Hosters;
}

QStringList CServicesWidget::GetFinders()
{
	QStringList Finders;
	foreach (const QVariantMap& Hoster, m_Services.values(eFinders))
	{
		QString Name = Hoster["HostName"].toString();
		Finders.append(Name);
	}
	return Finders;
}

QStringList CServicesWidget::GetHosters(bool bAll)
{
	QStringList Hosters;
	foreach (const QVariantMap& Hoster, m_Services.values(eHosters))
	{
		QString Name = Hoster["HostName"].toString();

		if(bAll)
			Hosters.append(Name);
		else
		{
			foreach (const QVariant vLogin, Hoster["Logins"].toList())
			{
				QVariantMap Login = vLogin.toMap();
				if(!Login["Free"].toBool())
				{
					Hosters.append(Name);
					break;
				}
			}
		}
	}
	return Hosters;
}

QMap<QString, QStringList> CServicesWidget::GetAccounts()
{
	QMap<QString, QStringList> Accounts;
	foreach (const QVariantMap& Hoster, m_Services.values(eHosters))
	{
		QString Name = Hoster["HostName"].toString();

		if(Hoster["AnonUpload"].toBool())
			Accounts[Name].append("");

		foreach (const QVariant vLogin, Hoster["Logins"].toList())
		{
			QVariantMap Login = vLogin.toMap();
			Accounts[Name].append(Login["UserName"].toString());
		}
	}
	return Accounts;
}

void CServicesWidget::BuyAccount(const QString& ServiceName)
{
	QDesktopServices::openUrl(QString("http://neoloader.to/affiliate.php?domain=" + ServiceName));
}

CNewAccount::CNewAccount(const QString& Hoster, QWidget *pMainWindow)
	: QDialogEx(pMainWindow)
{
	m_pAccountLayout = new QFormLayout(this);

	if(Hoster.isEmpty())
	{
		setWindowTitle(tr("New Account"));

		m_pHosters = new QComboBox();
		const QStringList& AllHosters = g_Services->GetKnownHosters();
		foreach(const QString& Hoster, AllHosters)
			m_pHosters->addItem(theGUI->GetHosterIcon(Hoster), Hoster);
		m_pAccountLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(tr("Hoster:")));
		m_pAccountLayout->setWidget(0, QFormLayout::FieldRole, m_pHosters);
	}
	else
	{
		m_Hoster = Hoster;
		setWindowTitle(tr("New: %1").arg(m_Hoster));

		m_pHosters = NULL;
	}

	m_pBuyAccount = new QPushButton(tr("Buy Account"));
	connect(m_pBuyAccount, SIGNAL(pressed()), this, SLOT(OnBuyAccount()));
	m_pAccountLayout->setWidget(1, QFormLayout::SpanningRole, m_pBuyAccount);

	m_pUserName = new QLineEdit();
	m_pUserName->setMaximumWidth(200);
	m_pAccountLayout->setWidget(2, QFormLayout::LabelRole, new QLabel(tr("Username (Optional):")));
	m_pAccountLayout->setWidget(2, QFormLayout::FieldRole, m_pUserName);

	m_pPassword = new QLineEdit();
	m_pPassword->setMaximumWidth(200);
	m_pAccountLayout->setWidget(3, QFormLayout::LabelRole, new QLabel(tr("Password / API Key:")));
	m_pAccountLayout->setWidget(3, QFormLayout::FieldRole, m_pPassword);

	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
	QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
	m_pAccountLayout->setWidget(4, QFormLayout::FieldRole, m_pButtonBox);
}