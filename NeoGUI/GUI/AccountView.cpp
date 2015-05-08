#include "GlobalHeader.h"
#include "AccountView.h"
#include "../NeoGUI.h"
#include "ServicesWidget.h"

CAccountView::CAccountView(QWidget *parent)
:QWidget(parent)
{
	m_bLockDown = false;

	m_pMainLayout = new QVBoxLayout();

	m_pAccountWidget = new QGroupBox();
	m_pAccountLayout = new QFormLayout();

	m_pUserName = new QLineEdit();
	m_pUserName->setMaximumWidth(200);
	m_pUserName->setReadOnly(true);
	m_pAccountLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(tr("Username (Optional):")));
	m_pAccountLayout->setWidget(0, QFormLayout::FieldRole, m_pUserName);

	m_pPassword = new QLineEdit();
	m_pPassword->setMaximumWidth(200);
	connect(m_pPassword, SIGNAL(editingFinished()), this, SLOT(SaveAccount()));
	m_pAccountLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(tr("Password / API Key:")));
	m_pAccountLayout->setWidget(1, QFormLayout::FieldRole, m_pPassword);

	m_pEnabled = new QCheckBox(tr("Account Enabled"));
	connect(m_pEnabled, SIGNAL(stateChanged(int)), this, SLOT(SaveAccount()));
	m_pAccountLayout->setWidget(2, QFormLayout::FieldRole, m_pEnabled);

	m_pFree = new QCheckBox(tr("Free Account"));
	connect(m_pFree, SIGNAL(stateChanged(int)), this, SLOT(SaveAccount()));
	m_pAccountLayout->setWidget(3, QFormLayout::FieldRole, m_pFree);

	m_pUploadTo = new QCheckBox(tr("Upload using this Account"));
	connect(m_pUploadTo, SIGNAL(stateChanged(int)), this, SLOT(SaveAccount()));
	m_pAccountLayout->setWidget(4, QFormLayout::FieldRole, m_pUploadTo);

	m_pAccountWidget->setLayout(m_pAccountLayout);

	m_pMainLayout->addWidget(m_pAccountWidget);

	setLayout(m_pMainLayout);
}

void CAccountView::SetMode(UINT Mode)
{
	m_pFree->setVisible(Mode == CServicesWidget::eHosters);
	m_pUploadTo->setVisible(Mode == CServicesWidget::eHosters);
}

class CAccountSyncJob: public CInterfaceJob
{
public:
	CAccountSyncJob(CAccountView* pView, const QString& HostName, const QString& UserName)
	{
		m_pView = pView;
		m_Request["HostName"] = HostName;
		m_Request["UserName"] = UserName;
	}

	virtual QString			GetCommand()	{return "GetService";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			m_pView->m_bLockDown = true;
			m_pView->m_pPassword->setText(Response["Password"].toString());
			m_pView->m_pEnabled->setChecked(Response["Enabled"].toBool());
			m_pView->m_pFree->setChecked(Response["Free"].toBool());
			m_pView->m_pUploadTo->setChecked(Response["Upload"].toBool());
			m_pView->m_bLockDown = false;
		}
	}
protected:
	QPointer<CAccountView>	m_pView; // Note: this can be deleted at any time
};


void CAccountView::ShowAccount(const QString& Hoster, const QString& Account)
{
	m_Hoster = Hoster;
	m_Account = Account;

	m_pAccountWidget->setTitle(Account + "@" + Hoster);
	m_pUserName->setText(Account);

	CAccountSyncJob* pAccountSyncJob = new CAccountSyncJob(this, m_Hoster, m_Account);
	theGUI->ScheduleJob(pAccountSyncJob);
}

class CAccountSaveJob: public CInterfaceJob
{
public:
	CAccountSaveJob(const QString& HostName, const QString& UserName, CAccountView* pView)
	{
		m_Request["HostName"] = HostName;
		m_Request["UserName"] = UserName;
		m_Request["Password"] = pView->m_pPassword->text();
		m_Request["Enabled"] = pView->m_pEnabled->isChecked();
		m_Request["Free"] = pView->m_pFree->isChecked();
		m_Request["Upload"] = pView->m_pUploadTo->isChecked();
	}

	virtual QString			GetCommand()	{return "SetService";}
	virtual void			HandleResponse(const QVariantMap& Response) {}
};

void CAccountView::SaveAccount()
{
	if(m_bLockDown)
		return;

	CAccountSaveJob* pAccountSaveJob = new CAccountSaveJob(m_Hoster, m_Account, this);
	theGUI->ScheduleJob(pAccountSaveJob);
}

