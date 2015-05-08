#include "GlobalHeader.h"
#include "Connector.h"
#include "NeoGUI.h"
//#include "../../qtservice/src/qtservice.h"

CConnector::CConnector(QWidget *pMainWindow)
	: QDialogEx(pMainWindow)
{
	m_pMainLayout = new QFormLayout();
	m_pMainLayout->setMargin(5);

	m_Mode = -1;

	int Counter = 0;
	m_pMainLayout->setWidget(Counter, QFormLayout::SpanningRole, new QLabel(tr(	"Setup NeoLoader Mode of operation\r\n"
																				"\r\n"
																				"Unified - Single Process\r\n"
																				"Separate - GUI in a separate process\r\n"
																				"Local - Core in a separate console process\r\n"
																				"Remote - No Core, only remote GUI \r\n"
																				"\r\n"
																				)));

	Counter++;
	m_pLabel = new QLabel(tr("<b>Restart NeoLoader to apply changes.</b><br />"));
	m_pLabel->setVisible(false);
	m_pMainLayout->setWidget(Counter, QFormLayout::SpanningRole, m_pLabel);

	Counter++;
	m_pMainLayout->setWidget(Counter, QFormLayout::LabelRole, new QLabel(tr("Operation Mode")));
	m_pMode = new QComboBox();
	m_pMode->addItem(tr("Single Process"), "Unified");
	m_pMode->addItem(tr("Separated Process"), "Separate");
	m_pMode->addItem(tr("Local Core"), "Local");
	m_pMode->addItem(tr("Remote Core"), "Remote");
	connect(m_pMode, SIGNAL(currentIndexChanged(int)), this, SLOT(OnModeChanged(int)));
	m_pMainLayout->setWidget(Counter, QFormLayout::FieldRole, m_pMode);

	Counter++;
	m_pMainLayout->setWidget(Counter, QFormLayout::LabelRole, new QLabel(tr("Password")));
	m_pPassword = new QLineEdit();
	m_pMainLayout->setWidget(Counter, QFormLayout::FieldRole, m_pPassword);

	Counter++;
	m_pMainLayout->setWidget(Counter, QFormLayout::LabelRole, new QLabel(tr("Host Port")));
	m_pHostPort = new QLineEdit();
	m_pMainLayout->setWidget(Counter, QFormLayout::FieldRole, m_pHostPort);

	Counter++;
	m_pMainLayout->setWidget(Counter, QFormLayout::LabelRole, new QLabel(tr("Host Name")));
	m_pHostName = new QLineEdit();
	m_pMainLayout->setWidget(Counter, QFormLayout::FieldRole, m_pHostName);

	Counter++;
	m_pMainLayout->setWidget(Counter, QFormLayout::LabelRole, new QLabel(tr("Pipe Name")));
	m_pPipeName = new QLineEdit();
	m_pMainLayout->setWidget(Counter, QFormLayout::FieldRole, m_pPipeName);

	Counter++;
	m_pMainLayout->setWidget(Counter, QFormLayout::LabelRole, new QLabel(tr("Auto Connect")));
	m_pAutoConnect = new QComboBox();
	m_pAutoConnect->addItem(tr("No"));
	m_pAutoConnect->addItem(tr("Yes"));
	m_pAutoConnect->addItem(tr("Yes &  Start"));
	m_pMainLayout->setWidget(Counter, QFormLayout::FieldRole, m_pAutoConnect);

	/*QtServiceController svc(theLoader->Cfg()->GetString("Core/ServiceName"));

	Counter++;
	m_pServiceBtn = new QPushButton(svc.isInstalled() ? tr("Remove Service") : tr("Install Service"));
	connect(m_pServiceBtn, SIGNAL(pressed()), this, SLOT(OnService()));
	m_pMainLayout->setWidget(Counter, QFormLayout::LabelRole, m_pServiceBtn);
	m_pServiceName = new QLineEdit();
	m_pMainLayout->setWidget(Counter, QFormLayout::FieldRole, m_pServiceName);*/


	Counter++;
	//m_pStartBtn = new QPushButton(tr("Start"));
	//connect(m_pStartBtn, SIGNAL(pressed()), this, SLOT(OnStart()));
	//m_pMainLayout->setWidget(Counter, QFormLayout::LabelRole, m_pStartBtn);
	m_pConnectBtn = new QPushButton(tr("Connect"));
	connect(m_pConnectBtn, SIGNAL(pressed()), this, SLOT(OnConnect()));
	m_pMainLayout->setWidget(Counter, QFormLayout::FieldRole, m_pConnectBtn);

	Counter++;
	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(OnClicked(QAbstractButton*)));
	m_pMainLayout->setWidget(Counter, QFormLayout::SpanningRole, m_pButtonBox);

	setLayout(m_pMainLayout);

	Load();

	m_uTimerID = startTimer(100);
}

CConnector::~CConnector()
{
	 killTimer(m_uTimerID);
}

void CConnector::OnClicked(QAbstractButton* pButton)
{
	switch(m_pButtonBox->buttonRole(pButton))
	{
		case QDialogButtonBox::ApplyRole: 
			Apply();	
			break;
		case QDialogButtonBox::RejectRole: 
			Load();	
			hide();
			break;
	}
}

void CConnector::timerEvent(QTimerEvent* pEvent)
{
	if(pEvent->timerId() != m_uTimerID)
		return;

	//m_pStartBtn->setText(theLoader->NeoCoreRunning() || theLoader->Itf()->IsConnected() ? tr("Stop") : tr("Start"));
	m_pConnectBtn->setText(theGUI->IsConnected() ? tr("Disconnect") : tr("Connect"));
}

void CConnector::Load()
{
	int Index = m_pMode->findData(theGUI->Cfg()->GetString("Core/Mode"));
	if(Index != -1)
	{
		m_pMode->setCurrentIndex(Index);
		OnModeChanged(Index);
	}

	m_pPassword->setText(theGUI->Cfg()->GetString("Core/Password"));

	m_pPipeName->setText(theGUI->Cfg()->GetString("Core/LocalName"));

	m_pHostPort->setText(theGUI->Cfg()->GetString("Core/RemotePort"));
	m_pHostName->setText(theGUI->Cfg()->GetString("Core/RemoteHost"));

	m_pAutoConnect->setCurrentIndex(theGUI->Cfg()->GetInt("Core/AutoConnect"));

	if(m_Mode == -1)
	{
		m_Mode = Index;

		switch(Index)
		{
		case 2:
			//m_pStartBtn->setEnabled(true);
			m_pConnectBtn->setEnabled(true);
			break;
		case 3:
			//m_pStartBtn->setEnabled(false);
			m_pConnectBtn->setEnabled(true);
			break;
		default:
			//m_pStartBtn->setEnabled(false);
			m_pConnectBtn->setEnabled(false);
		}
	}
}

void CConnector::Apply()
{
	m_pLabel->setVisible(m_Mode != m_pMode->currentIndex());

	theGUI->Cfg()->SetSetting("Core/Mode", m_pMode->itemData(m_pMode->currentIndex()));

	theGUI->Cfg()->SetSetting("Core/Password", m_pPassword->text());

	theGUI->Cfg()->SetSetting("Core/LocalName", m_pPipeName->text());

	theGUI->Cfg()->SetSetting("Core/RemotePort", m_pHostPort->text());
	theGUI->Cfg()->SetSetting("Core/RemoteHost", m_pHostName->text());

	theGUI->Cfg()->SetSetting("Core/AutoConnect", m_pAutoConnect->currentIndex());
}

void CConnector::OnModeChanged(int Index)
{
	switch(Index)
	{
	case 0: // Single Process
		m_pHostName->setEnabled(false);
		m_pPipeName->setEnabled(false);
		/*m_pServiceBtn->setEnabled(false);
		m_pServiceName->setEnabled(false);*/
		m_pAutoConnect->setEnabled(false);
		break;

	case 1: // Core with Tray
		m_pHostName->setEnabled(false);
		m_pPipeName->setEnabled(true);
		/*m_pServiceBtn->setEnabled(false);
		m_pServiceName->setEnabled(false);*/
		m_pAutoConnect->setEnabled(false);
		break;

	case 2: // Local Core
		m_pHostName->setEnabled(false);
		m_pPipeName->setEnabled(true);
		/*m_pServiceBtn->setEnabled(true);
		m_pServiceName->setEnabled(true);*/
		m_pAutoConnect->setEnabled(true);
		break;

	case 3: // Remote Core
		m_pHostName->setEnabled(true);
		m_pPipeName->setEnabled(false);
		/*m_pServiceBtn->setEnabled(false);
		m_pServiceName->setEnabled(false);*/
		m_pAutoConnect->setEnabled(true);
		break;
	}

	QModelIndex ModelIndex = m_pAutoConnect->model()->index(2, 0); 
	m_pAutoConnect->model()->setData(ModelIndex, QVariant(Index == 3 ? 0 : 1 | 32), Qt::UserRole - 1);
}

void CConnector::OnConnect()
{
	if(!theGUI->IsConnected())
		emit theGUI->Connect();
	else
		emit theGUI->Disconnect();
}

void CConnector::closeEvent(QCloseEvent *e)
{
	hide();
	e->ignore();
}