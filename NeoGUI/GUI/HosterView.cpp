#include "GlobalHeader.h"
#include "HosterView.h"
#include "../NeoGUI.h"
#include "AccountView.h"
#include "ServicesWidget.h"
#include "SettingsWidgets.h"

CHosterView::CHosterView(QWidget *parent)
:QWidget(parent)
{
	m_bLockDown = false;

	m_pMainLayout = new QVBoxLayout();

	m_pHosterWidget = new QGroupBox();
	m_pHosterLayout = new QFormLayout();

	m_pHostName = new QLineEdit();
	m_pHostName->setMaximumWidth(200);
	m_pHostName->setReadOnly(true);
	m_pHosterLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(tr("Hostname:")));
	m_pHosterLayout->setWidget(0, QFormLayout::FieldRole, m_pHostName);

	m_pUploadTo = new QCheckBox(tr("Upload without an Account"));
	connect(m_pUploadTo, SIGNAL(stateChanged(int)), this, SLOT(SaveHoster()));
	m_pHosterLayout->setWidget(1, QFormLayout::FieldRole, m_pUploadTo);

	m_pHosterLayout->setWidget(2, QFormLayout::LabelRole, new QLabel(tr("Custom Proxy:")));
	m_pProxy = new CProxyEdit();
	m_pProxy->setMaximumWidth(300);
	m_pHosterLayout->setWidget(2, QFormLayout::FieldRole, m_pProxy);
	connect(m_pProxy, SIGNAL(textChanged(const QString&)), this, SLOT(SaveHoster()));

	m_pHosterWidget->setLayout(m_pHosterLayout);

	m_pMainLayout->addWidget(m_pHosterWidget);

	setLayout(m_pMainLayout);
}

void CHosterView::SetMode(UINT Mode)
{
	m_pUploadTo->setVisible(Mode == CServicesWidget::eHosters);
}

class CHosterSyncJob: public CInterfaceJob
{
public:
	CHosterSyncJob(CHosterView* pView, const QString& HostName)
	{
		m_pView = pView;
		m_Request["HostName"] = HostName;
	}

	virtual QString			GetCommand()	{return "GetService";}
	virtual void			HandleResponse(const QVariantMap& Response) 
	{
		if(m_pView)
		{
			if(Response["AnonUpload"].toBool())
			{
				m_pView->m_bLockDown = true;
				m_pView->m_pUploadTo->setEnabled(true);
				m_pView->m_pUploadTo->setChecked(Response["Upload"].toBool());
				m_pView->m_pProxy->SetText(Response["Proxy"].toString());
				m_pView->m_bLockDown = false;
			}
			else
				m_pView->m_pUploadTo->setEnabled(false);
		}
	}
protected:
	QPointer<CHosterView>	m_pView; // Note: this can be deleted at any time
};

void CHosterView::ShowHoster(const QString& Hoster)
{
	m_Hoster = Hoster;

	m_pHosterWidget->setTitle(Hoster);
	m_pHostName->setText(Hoster);

	CHosterSyncJob* pHosterSyncJob = new CHosterSyncJob(this, m_Hoster);
	theGUI->ScheduleJob(pHosterSyncJob);
}

class CHosterApplyJob: public CInterfaceJob
{
public:
	CHosterApplyJob(CHosterView* pView, const QString& HostName)
	{
		m_Request["HostName"] = HostName;
		m_Request["Upload"] = pView->m_pUploadTo->isChecked();
		m_Request["Proxy"] = pView->m_pProxy->GetText();
	}

	virtual QString			GetCommand()	{return "SetService";}
	virtual void			HandleResponse(const QVariantMap& Response) {}	
};

void CHosterView::SaveHoster()
{
	if(m_bLockDown)
		return;

	CHosterApplyJob* pHosterApplyJob = new CHosterApplyJob(this, m_Hoster);
	theGUI->ScheduleJob(pHosterApplyJob);
}

