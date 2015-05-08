#include "GlobalHeader.h"
#include "SetupWizard.h"
#include "GUI/ServicesWidget.h"
#include "GUI/SettingsView.h"
#include "GUI/SettingsWindow.h"
#include "NeoGUI.h"

CSetupWizard::CSetupWizard(QWidget *parent)
 : QWizard(parent)
{
    setPage(Page_Intro, new CIntroPage);
    setPage(Page_Networks, new CNetworksPage);
	setPage(Page_Accounts, new CAccountsPage);
	setPage(Page_Security, new CSecurityPage);
	setPage(Page_Advanced, new CAdvancedPage);
    setPage(Page_Conclusion, new CConclusionPage);

    setStartId(Page_Intro);
#ifndef Q_WS_MAC
    setWizardStyle(ModernStyle);
#endif
    setOption(HaveHelpButton, true);
	setOption(IndependentPages);
    setPixmap(QWizard::LogoPixmap, QPixmap(":/Icon"));

    connect(this, SIGNAL(helpRequested()), this, SLOT(showHelp()));

    setWindowTitle(tr("NeoLoader 1st Start Wizard"));
}

void CSetupWizard::showHelp()
{
    QDesktopServices::openUrl(QString("http://neoloader.to/?dest=firststart"));
}


class CSetupJob: public CInterfaceJobEx
{
public:
	CSetupJob(QVariant Options)
	{
		m_Request["Options"] = Options;

		m_Attempt = 1;
	}

	virtual QString			GetCommand()	{return "SetCore";}
	virtual void			HandleResponse(const QVariantMap& Response) {}

	virtual void			Finish(bool bOK)
	{
		if(bOK)
		{
			theGUI->Cfg()->SetSetting("Gui/Setup", 1);

			//if(!m_Request["Options"].toMap()["NeoCore/PeerWatch/IPFilter"].toString().isEmpty())
			//	CModernGUI::CoreAction("LoadIpFilter");
		}
		else if(theGUI)
		{
			QTimer::singleShot(500 * m_Attempt, this, SLOT(Retry()));
			return;
		}
		CInterfaceJob::Finish(bOK); // delete this
	}

	virtual void			Retry()
	{
		if(++m_Attempt > 3)
		{
			if(QMessageBox(tr("First Start Wizard"), tr("Settings couldn't be saved, Retry?"), QMessageBox::Question, QMessageBox::Retry | QMessageBox::Default, QMessageBox::Cancel | QMessageBox::Escape, QMessageBox::NoButton).exec() == QMessageBox::Cancel)
			{
				CInterfaceJob::Finish(false); // delete this
				return;
			}
		}
		theGUI->ScheduleJob(this);
	}

protected:
	int						m_Attempt;
};

void CSetupWizard::ShowWizard()
{
	CSetupWizard Wizard;
	if(Wizard.exec())
	{
		QVariantMap Options;

		Options.insert("NeoCore/Hoster/Enable", Wizard.field("Hosters").toBool());
		Options.insert("NeoCore/BitTorrent/Enable", Wizard.field("BitTorrent").toBool());
		Options.insert("NeoCore/Ed2kMule/Enable", Wizard.field("Ed2kMule").toBool());
		Options.insert("NeoCore/NeoShare/Enable", Wizard.field("NeoShare").toBool());

		Options.insert("NeoCore/Content/Incoming", Wizard.field("Incoming"));
		Options.insert("NeoCore/Content/Temp", Wizard.field("Temp"));

		Options.insert("NeoCore/NeoShare/Anonymity", Wizard.field("Anonymity"));

		Options.insert("NeoCore/Ed2kMule/ShareDefault", Wizard.field("AutoEd2k"));
		Options.insert("NeoCore/Bandwidth/DefaultNIC", Wizard.field("VPN"));
		Options.insert("NeoCore/Bandwidth/WebProxy", Wizard.field("Proxy"));
		Options.insert("NeoCore/BitTorrent/Encryption", Wizard.field("Encryption"));
		Options.insert("NeoCore/Ed2kMule/Obfuscation", Wizard.field("Encryption"));
		//Options.insert("NeoCore/PeerWatch/IPFilter", Wizard.field("IpFilter"));
		Options.insert("NeoCore/Ed2kMule/HashMode", Wizard.field("Hash").toBool() ? "Static-Secure" : "Random-Secure");

		Options.insert("NeoCore/Bandwidth/Download", Wizard.field("DownRate"));
		Options.insert("NeoCore/Bandwidth/Upload", Wizard.field("UpRate"));

		//if(Wizard.field("Advanced").toBool())
		{
			Options.insert("NeoCore/Bandwidth/MaxConnections", Wizard.field("MaxConnections"));
			Options.insert("NeoCore/Bandwidth/MaxNewPer5Sec", Wizard.field("MaxNewPer5Sec"));

			Options.insert("NeoCore/NeoKad/Port", Wizard.field("NeoPort"));
			Options.insert("NeoCore/BitTorrent/ServerPort", Wizard.field("TorrentPort"));
			Options.insert("NeoCore/Ed2kMule/TCPPort", Wizard.field("MuleTCP"));
			Options.insert("NeoCore/Ed2kMule/UDPPort", Wizard.field("MuleUDP"));

			Options.insert("NeoCore/Bandwidth/UseUPnP", Wizard.field("UPnP"));
		}

#ifdef WIN32
		if(Wizard.field("Register").toBool())
		{
			QString OpenPath = "\"" + QApplication::applicationFilePath().replace("/", "\\") + "\" \"%1\"";

			g_ShellSetup.InstallProtocol("magnet", OpenPath, false, "", "NeoLoader supported Protocol");
			g_ShellSetup.InstallProtocol("ed2k", OpenPath, false, "", "NeoLoader supported Protocol");
			g_ShellSetup.InstallProtocol("jd", OpenPath, false, "", "NeoLoader supported Protocol");
			g_ShellSetup.InstallProtocol("jdlist", OpenPath, false, "", "NeoLoader supported Protocol");
			g_ShellSetup.InstallProtocol("dlc", OpenPath, false, "", "NeoLoader supported Protocol");
			//g_ShellSetup.InstallProtocol("ccf", OpenPath, false, "", "NeoLoader supported Protocol");
			//g_ShellSetup.InstallProtocol("rsdf", OpenPath, false, "", "NeoLoader supported Protocol");
			g_ShellSetup.InstallType("dlc", OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");
			//g_ShellSetup.InstallType("ccf", OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");
			//g_ShellSetup.InstallType("rsdf", OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");
			g_ShellSetup.InstallType("torrent", OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");
			g_ShellSetup.InstallType("emulecollection", OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");

			emit g_ShellSetup.Changed();
		}
#endif

		CSetupJob* pSetupJob = new CSetupJob(Options);
		theGUI->ScheduleJob(pSetupJob);
	}
	else if(QMessageBox(tr("First Start Wizard"), tr("Setup has not been completed, show Wizard at next startup again?"), QMessageBox::Question, QMessageBox::Yes | QMessageBox::Default | QMessageBox::Escape, QMessageBox::No, QMessageBox::NoButton).exec() == QMessageBox::No)
		theGUI->Cfg()->SetSetting("Gui/Setup", 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CIntroPage

CIntroPage::CIntroPage(QWidget *parent)
: QWizardPage(parent)
{
    setTitle(tr("Introduction"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(":/SetupBanner"));

    m_pTopLabel = new QLabel(tr("This wizard will guid you through the first steps to setup NeoLoader."));
    m_pTopLabel->setWordWrap(true);

	m_pMainLabel = new QLabel(tr("Press Next to Continue."));

    QVBoxLayout* m_pLayout = new QVBoxLayout;
    m_pLayout->addWidget(m_pTopLabel);
    m_pLayout->addWidget(m_pMainLabel);
    setLayout(m_pLayout);
}

int CIntroPage::nextId() const
{
	return CSetupWizard::Page_Networks;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetworksPage

CNetworksPage::CNetworksPage(QWidget *parent)
: QWizardPage(parent)
{
    setTitle(tr("Setup Networks"));
	setSubTitle(tr("Select from which sources you want do download.<br>And choose a download Location."));

	m_pBitTorrent = new QCheckBox(tr("BitTorrent"));
	m_pBitTorrent->setChecked(true);
	m_pEd2kMule = new QCheckBox(tr("eMule/eDonkey2000"));
	m_pEd2kMule->setChecked(true);
	m_pNeoShare = new QCheckBox(tr("NeoShare (anonymouse)"));
	m_pNeoShare->setChecked(true);
	m_pAddHosters = new QCheckBox(tr("Extended One Click Hosters Support"));
	m_pAddHosters->setChecked(true);

	registerField("Hosters", m_pAddHosters);
	registerField("BitTorrent", m_pBitTorrent);
	registerField("Ed2kMule", m_pEd2kMule);
	registerField("NeoShare", m_pNeoShare);

	connect(m_pEd2kMule, SIGNAL(clicked(bool)), this, SLOT(OnEd2k(bool)));
	m_pAutoEd2k = new QCheckBox(tr("Automatically share files on ed2k network"));
	registerField("AutoEd2k", m_pAutoEd2k);
	
	m_pIncoming = new CPathEdit(true);
	connect(m_pIncoming, SIGNAL(textChanged(const QString &)), this, SLOT(OnIncoming(QString)));
	m_pTemp = new QLineEdit();
	m_pTemp->setReadOnly(true);
	m_pTemp->setDisabled(true);

	m_pIncoming->SetText(QDir::homePath() + "/Downloads/NeoLoader/Incoming");
    registerField("Incoming", m_pIncoming->GetEdit());
    registerField("Temp", m_pTemp);

	m_pRegister = new QCheckBox(tr("Register File Extensions"));
#ifdef WIN32
	m_pRegister->setChecked(true);
#else
	m_pRegister->setEnabled(false);
#endif

	registerField("Register", m_pRegister);

    QFormLayout* pLayout = new QFormLayout; int i = 0;
	pLayout->setWidget(i++, QFormLayout::SpanningRole, new QLabel(tr("Select Download Sources:")));
#ifdef _DEBUG
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pNeoShare);
#endif
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pBitTorrent);
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pEd2kMule);
	QWidget* pAutoEd2k = new QWidget();
	QHBoxLayout* pAutoEd2kLayout = new QHBoxLayout(pAutoEd2k);
	pAutoEd2kLayout->setMargin(0);
	pAutoEd2kLayout->setAlignment(Qt::AlignLeft);
	pAutoEd2kLayout->addWidget(new QLabel("  "));
	pAutoEd2kLayout->addWidget(m_pAutoEd2k);
	pLayout->setWidget(i++, QFormLayout::FieldRole, pAutoEd2k);
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pAddHosters);
	pLayout->setWidget(i++, QFormLayout::SpanningRole, new QLabel(tr("Set Download Directory:")));
	pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel(tr("Incoming")));
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pIncoming);
	pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel(tr("Temp")));
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pTemp);
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pRegister);
    setLayout(pLayout);
}

void CNetworksPage::OnEd2k(bool bChecked)
{
	m_pAutoEd2k->setEnabled(bChecked);
}

void CNetworksPage::OnIncoming(QString Path)
{
	if(Path.isEmpty())
		m_pTemp->setText("");
	else
	{
#ifdef WIN32
		Path.replace("\\", "/");
#endif
		m_pTemp->setText(Split2(Path, "/", true).first + "/Temp");
	}
}

int CNetworksPage::nextId() const
{
	if (m_pAddHosters->isChecked())
        return CSetupWizard::Page_Accounts;
    if(m_pNeoShare->isChecked() || m_pBitTorrent->isChecked() || m_pEd2kMule->isChecked())
		return CSetupWizard::Page_Security;
	return CSetupWizard::Page_Advanced;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAccountsPage

CAccountsPage::CAccountsPage(QWidget *parent)
: QWizardPage(parent)
{
    setTitle(tr("Setup Hoster Accounts"));
	setSubTitle(tr("Add One Click Hoster Accounts, for obtimal download speed Premium accounts are recommended."));

	//m_pHosters = new QComboBox();

	m_pAddAccount = new QPushButton(tr("Add Account"));
	m_pAddAccount->setMaximumWidth(100);
	connect(m_pAddAccount, SIGNAL(pressed()), this, SLOT(OnAddAccount()));

	m_pAccounts = new QTreeWidget();
	m_pAccounts->setHeaderLabels(QString("Domain|Username|Password|").split("|"));

    QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->setMargin(3);
	pLayout->addWidget(m_pAccounts);
	pLayout->addWidget(m_pAddAccount);
    setLayout(pLayout);
}

void CAccountsPage::initializePage()
{
	/*m_pHosters->clear();
	const QStringList& AllHosters = theGUI->GetKnownHosters();
	foreach(const QString& Hoster, AllHosters)
		m_pHosters->addItem(theGUI->GetHosterIcon(Hoster), Hoster);*/
}

void CAccountsPage::OnAddAccount()
{
	CNewAccount::SHosterAcc Account = CNewAccount::GetAccount("", this);
	if(Account.Hoster.isEmpty())
		return;

	CServicesWidget::AddAccount(Account.Hoster, Account.Username, Account.Password);

	QTreeWidgetItem* pAccount = new QTreeWidgetItem();
	pAccount->setText(0, Account.Hoster);
	pAccount->setText(1, Account.Username);
	pAccount->setText(2, Account.Password);
	m_pAccounts->addTopLevelItem(pAccount);
}

int CAccountsPage::nextId() const
{
	if(field("NeoShare").toBool() || field("BitTorrent").toBool() || field("Ed2kMule").toBool())
		return CSetupWizard::Page_Security;
	else
		return CSetupWizard::Page_Advanced;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSecurityPage

CSecurityPage::CSecurityPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Security Options"));
	setSubTitle(tr("Configure your security settings, to ensure the level or privacy that you prefer."));

	m_pSlider = new CAnonSlider(this);
	registerField("Anonymity", m_pSlider->GetSlider());

	/*QSlider* pNoTracking = new QSlider(Qt::Horizontal);
	pNoTracking->setMaximum(2);
	pNoTracking->setValue(1);
	pNoTracking->setTickInterval(1);
	pNoTracking->setTickPosition(QSlider::TicksBelow);
	m_pNoTracking = new QWidget(this);
	QVBoxLayout* pNoTrackingLayout = new QVBoxLayout();
	pNoTrackingLayout->setMargin(0);
	pNoTrackingLayout->addWidget(new QLabel(tr("Client Trackability for other P2P Networks")));
	pNoTrackingLayout->addWidget(pNoTracking);
		QWidget* pVs = new QWidget();
		QHBoxLayout* pVsL = new QHBoxLayout();
		pVsL->setMargin(0);
		pVsL->addWidget(new QLabel("High (Unrestricted)"));
		QWidget* pSpacer = new QWidget();
		pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		pVsL->addWidget(pSpacer);
		pVsL->addWidget(new QLabel("Low (Minimized)"));
		pVs->setLayout(pVsL);
	pNoTrackingLayout->addWidget(pVs);
	m_pNoTracking->setLayout(pNoTrackingLayout);
	registerField("NoTracking", pNoTracking);*/

	m_pNIC = new QNICCombo();
	m_pNIC->setMaximumWidth(300);

	m_pProxy = new CProxyEdit();
	m_pProxy->setMaximumWidth(300);

	m_pEncryption = new QComboBoxEx();
	m_pEncryption->setMaximumWidth(100);
	m_pEncryption->addItem(tr("Disable"), "Disable");
	m_pEncryption->addItem(tr("Support"), "Support");
	m_pEncryption->addItem(tr("Request"), "Request");
	m_pEncryption->addItem(tr("Require"), "Require");
	m_pEncryption->setCurrentIndex(2);

	//m_pIpFilter = new QComboBoxEx();
	//m_pIpFilter->setMaximumWidth(300);
	//m_pIpFilter->addItem("None");
	//m_pIpFilter->addItem("eMule Security", "http://upd.emule-security.org/ipfilter.zip");
	//m_pIpFilter->setCurrentIndex(1);

	m_pStaticHash = new QCheckBox(tr("allows to benefit from credit systems, but facilitates tracking"));
	m_pStaticHash->setChecked(true);

	registerField("VPN", m_pNIC, "Data");
	registerField("Encryption", m_pEncryption, "Data");
	registerField("Proxy", m_pProxy, "Data");
	//registerField("IpFilter", m_pIpFilter, "Data");
	registerField("Hash", m_pStaticHash);

    QFormLayout* pLayout = new QFormLayout(); int i = 0;
	pLayout->setWidget(i++, QFormLayout::SpanningRole, m_pSlider);
	//pLayout->setWidget(i++, QFormLayout::SpanningRole, m_pNoTracking);
	pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel(tr("Lock to VPN Adapter")));
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pNIC);
	pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel(tr("Obfuscate P2P traffic")));
	QWidget* pEncryption = new QWidget();
	QHBoxLayout* pEncryptionLayout = new QHBoxLayout(pEncryption);
	pEncryptionLayout->setMargin(0);
	pEncryptionLayout->setAlignment(Qt::AlignLeft);
	pEncryptionLayout->addWidget(m_pEncryption);
	pEncryptionLayout->addWidget(new QLabel(tr("set to 'Resuested' if your ISP if blocking P2P")));
	pLayout->setWidget(i++, QFormLayout::FieldRole, pEncryption);
	pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel(tr("Use Proxy for Hosters")));
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pProxy);
	pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel(tr("Static Hash")));
	pLayout->setWidget(i++, QFormLayout::FieldRole, m_pStaticHash);
	//pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel(tr("Use IpFilter")));
	//pLayout->setWidget(i++, QFormLayout::FieldRole, m_pIpFilter);
    setLayout(pLayout);
}

void CSecurityPage::initializePage()
{
	m_pSlider->setEnabled(field("NeoShare").toBool());
	//m_pNoTracking->setEnabled(field("Ed2kMule").toBool() || field("BitTorrent").toBool());
	m_pNIC->setEnabled(field("NeoShare").toBool() || field("BitTorrent").toBool() || field("Ed2kMule").toBool());
	m_pEncryption->setEnabled(field("BitTorrent").toBool() || field("Ed2kMule").toBool());
	//m_pIpFilter->setEnabled(field("BitTorrent").toBool() || field("Ed2kMule").toBool());
	m_pStaticHash->setEnabled(field("Ed2kMule").toBool());
}

int CSecurityPage::nextId() const
{
    return CSetupWizard::Page_Advanced;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAdvancedPage

CAdvancedPage::CAdvancedPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Advanced Options"));
	setSubTitle(tr("Setup is almost done, set your bandwitch limits and ports."));

	//m_pAdvanced = new QCheckBox(tr("Configure Advanced Options"));
	//connect(m_pAdvanced, SIGNAL(clicked(bool)), this, SLOT(OnAdvanced(bool)));
	//registerField("Advanced", m_pAdvanced);

	m_pDownRate = new CKbpsEdit();
	m_pDownRate->setMaximumWidth(100);
	m_pDownRate->SetValue(0);
	m_pUpRate = new CKbpsEdit();
	m_pUpRate->setMaximumWidth(100);
	m_pUpRate->SetValue(0);
	registerField("DownRate", m_pDownRate, "Value");
	registerField("UpRate", m_pUpRate, "Value");

	m_pUPnP = NULL;
	m_pNeoPort = NULL;
	m_pTorrentPort = NULL;
	m_pMuleTCP = NULL;
	m_pMuleUDP = NULL;

    m_pLayout = new QFormLayout;
	m_pLayout->setWidget(0, QFormLayout::LabelRole, new QLabel("Download (KB/s)"));
	m_pLayout->setWidget(0, QFormLayout::FieldRole, m_pDownRate);
	m_pLayout->setWidget(1, QFormLayout::LabelRole, new QLabel("Upload (KB/s)"));
	m_pLayout->setWidget(1, QFormLayout::FieldRole, m_pUpRate);
	//m_pLayout->setWidget(2, QFormLayout::SpanningRole, m_pAdvanced);
	m_pLayout->setWidget(2, QFormLayout::SpanningRole, new QLabel(""));
    setLayout(m_pLayout);
}

void CAdvancedPage::initializePage()
{
	OnAdvanced(true);
	//m_pAdvanced->setEnabled(field("NeoShare").toBool() || field("BitTorrent").toBool() || field("Ed2kMule").toBool());
}

void CAdvancedPage::OnAdvanced(bool bChecked)
{
	if(!m_pNeoPort)
	{
		m_pMaxCon = new QLineEdit();
		m_pMaxCon->setMaximumWidth(50);
		m_pMaxCon->setText("250");
		registerField("MaxConnections", m_pMaxCon);
		m_pMaxNew = new QLineEdit();
		m_pMaxNew->setMaximumWidth(50);
		m_pMaxNew->setText("50");
		registerField("MaxNewPer5Sec", m_pMaxNew);		

		m_pNeoPort = new QLineEdit();
		m_pNeoPort->setText(QString::number(GetRandomInt(9000, 9999)));
		m_pNeoPort->setMaximumWidth(50);
		registerField("NeoPort", m_pNeoPort);

		m_pTorrentPort = new QLineEdit();
		m_pTorrentPort->setText(QString::number(GetRandomInt(6000, 6999)));
		m_pTorrentPort->setMaximumWidth(50);
		registerField("TorrentPort", m_pTorrentPort);

		m_pMuleTCP = new QLineEdit();
		m_pMuleTCP->setText(QString::number(GetRandomInt(4000, 4999)));
		m_pMuleTCP->setMaximumWidth(50);
		m_pMuleUDP = new QLineEdit();
		m_pMuleUDP->setText(QString::number(GetRandomInt(5000, 5999)));
		m_pMuleUDP->setMaximumWidth(50);

		registerField("MuleTCP", m_pMuleTCP);
		registerField("MuleUDP", m_pMuleUDP);

		m_pUPnP = new QCheckBox(tr("Use UPnP to open ports automatically"));
		m_pUPnP->setChecked(true);
		registerField("UPnP", m_pUPnP);

		int i = 3;
		m_pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel("Max Connections"));
		m_pLayout->setWidget(i++, QFormLayout::FieldRole, m_pMaxCon);
		m_pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel("Max New Con / 5 Sec"));
		m_pLayout->setWidget(i++, QFormLayout::FieldRole, m_pMaxNew);

		m_pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel("NeoKad Port (UDP)"));
		m_pLayout->setWidget(i++, QFormLayout::FieldRole, m_pNeoPort);
		m_pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel("BitTorrent Port (TCP/UDP)"));
		m_pLayout->setWidget(i++, QFormLayout::FieldRole, m_pTorrentPort);
		m_pLayout->setWidget(i, QFormLayout::LabelRole, new QLabel("eMule/eDonkey2000 Ports (TCP/UDP)"));
		QWidget* pPorts = new QWidget();
		QHBoxLayout* pPortLayout = new QHBoxLayout();
		pPortLayout->setMargin(0);
		pPortLayout->setAlignment(Qt::AlignLeft);
		pPortLayout->addWidget(m_pMuleTCP);
		pPortLayout->addWidget(m_pMuleUDP);
		pPorts->setLayout(pPortLayout);
		m_pLayout->setWidget(i++, QFormLayout::FieldRole, pPorts);
		m_pLayout->setWidget(i++, QFormLayout::FieldRole, m_pUPnP);
	}

	bool bAny = field("NeoShare").toBool() || field("BitTorrent").toBool() || field("Ed2kMule").toBool();
	m_pMaxCon->setEnabled(bChecked && bAny);
	m_pMaxNew->setEnabled(bChecked && bAny);
	m_pNeoPort->setEnabled(bChecked);
	m_pTorrentPort->setEnabled(bChecked && field("BitTorrent").toBool());
	m_pMuleTCP->setEnabled(bChecked && field("Ed2kMule").toBool());
	m_pMuleUDP->setEnabled(bChecked && field("Ed2kMule").toBool());
	m_pUPnP->setEnabled(bChecked);
}

int CAdvancedPage::nextId() const
{
    return CSetupWizard::Page_Conclusion;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// CConclusionPage

CConclusionPage::CConclusionPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Setup Completed"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(":/SetupBanner"));

	m_pMainLabel = new QLabel(tr("NeoLoader is set up."));
	m_pMainLabel->setWordWrap(true);
    m_pBottomLabel = new QLabel(tr("Press Finish to Complete."));

    QVBoxLayout* pLayout = new QVBoxLayout;
	pLayout->addWidget(m_pMainLabel);
    pLayout->addWidget(m_pBottomLabel);
    setLayout(pLayout);
}

int CConclusionPage::nextId() const
{
	return -1;
}

