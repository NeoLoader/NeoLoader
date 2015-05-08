#include "GlobalHeader.h"
#include "SettingsWindow.h"
#include "SettingsView.h"
#include "ShareWidget.h"
#include "PropertiesView.h"
#include "../NeoGUI.h"
#include "../Common/Dialog.h"

#ifdef WIN32
CShellSetup g_ShellSetup;
#endif

CSettingsWindow::CSettingsWindow(QWidget *parent)
//:QWidget(parent)
:QMainWindow(parent)
{
	m_pMainWidget = new QWidget();
	m_pMainLayout = new QGridLayout();
	//m_pMainLayout->setMargin(3);
	
	m_pGroupeTree = new QTreeWidget();
	//connect(m_pGroupeTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pGroupeTree, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));
	m_pGroupeTree->header()->hide();
	m_pGroupeTree->setMaximumWidth(150);
	m_pMainLayout->addWidget(m_pGroupeTree, 0, 0, 0, 1);

	m_pSettingsWidget = new QWidget();
	m_pSettingsLayout = new QStackedLayout(m_pSettingsWidget);

	// Interface
	QTreeWidgetItem* pInterface = new QTreeWidgetItem(QStringList(tr("Interface")));
	QPixmap InterfaceIcon(":/Icons/Interface");
	pInterface->setIcon(0, InterfaceIcon.scaled(16,16));
	m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pInterface);
	QMultiMap<int, SField> Interface;
	SetupInterface(Interface);
	m_pInterface = new CSettingsViewEx(Interface, tr("Interface Options"), QString(	"NeoLoader/Gui/Language|"
																					"NeoLoader/Gui/AdvancedControls|"
																					"NeoLoader/Gui/ShowLog|"
																					"NeoLoader/Gui/InternPlayer|"
																					"NeoLoader/Integration/SysTray|"
																					"NeoLoader/Gui/AutoSort|"
																					//"NeoLoader/Gui/VerboseGraphs|"
																					"NeoLoader/Gui/UnifyGrabber|"
																					"NeoLoader/Gui/Alternate|"
																					).split("|", QString::SkipEmptyParts));
	m_pSettingsLayout->addWidget(m_pInterface);
	//

	// General
	QTreeWidgetItem* pGeneral = new QTreeWidgetItem(QStringList(tr("General")));
	QPixmap GeneralIcon(":/Icons/Advanced");
	pGeneral->setIcon(0, GeneralIcon.scaled(16,16));
	m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pGeneral);
	QMultiMap<int, SField> General;
	SetupGeneral(General);
	m_pGeneral = new CSettingsView(General, tr("General Options"));
	m_pSettingsLayout->addWidget(m_pGeneral);
	//

	// Network
	QTreeWidgetItem* pNetwork = new QTreeWidgetItem(QStringList(tr("Network")));
	QPixmap NetworkIcon(":/Icons/Statistics");
	pNetwork->setIcon(0, NetworkIcon.scaled(16,16));
	m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pNetwork);
	QMultiMap<int, SField> Network;
	SetupNetwork(Network);
	m_pNetwork = new CSettingsView(Network, tr("Network Options"));
	m_pSettingsLayout->addWidget(m_pNetwork);
	//

	// Share
	QTreeWidgetItem* pShare = new QTreeWidgetItem(QStringList(tr("Share")));
	QPixmap ShareIcon(":/Icons/Share");
	pShare->setIcon(0, ShareIcon.scaled(16,16));
	m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pShare);
	QMultiMap<int, SField> Shared;
	SetupShared(Shared);
	m_pShare = new CShareWidget(Shared, tr("Shared Files"));
	m_pSettingsLayout->addWidget(m_pShare);
	//

	// Hoster
	QTreeWidgetItem* pHoster = new QTreeWidgetItem(QStringList(tr("Hoster")));
	QPixmap HosterIcon(":/Icons/Hosters");
	pHoster->setIcon(0, HosterIcon.scaled(16,16));
	m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pHoster);
	QMultiMap<int, SField> Hoster;
	SetupHosters(Hoster);
	m_pHoster = new CSettingsView(Hoster, tr("Hoster Options"));
	m_pSettingsLayout->addWidget(m_pHoster);
	// 

	// NeoShare
	QTreeWidgetItem* pNeoShare = new QTreeWidgetItem(QStringList(tr("Neo Share")));
	QPixmap NeoIcon(":/Icons/NeoShare");
	pNeoShare->setIcon(0, NeoIcon.scaled(16,16));
	m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pNeoShare);
	QMultiMap<int, SField> NeoShare;
	SetupNeoShare(NeoShare);
	m_pNeoShare = new CSettingsView(NeoShare, tr("Neo Share Options"));
	m_pSettingsLayout->addWidget(m_pNeoShare);
	// 

	// BitTorrent
	QTreeWidgetItem* pTorrent = new QTreeWidgetItem(QStringList(tr("BitTorrent")));
	QPixmap TorrentIcon(":/Icons/BitTorrent");
	pTorrent->setIcon(0, TorrentIcon.scaled(16,16));
	m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pTorrent);
	QMultiMap<int, SField> BitTorrent;
	SetupBitTorrent(BitTorrent);
	m_pBitTorrent = new CSettingsView(BitTorrent, tr("BitTorrent Options"));
	m_pSettingsLayout->addWidget(m_pBitTorrent);
	// 

	// Ed2kMule
	QTreeWidgetItem* pEd2kMule = new QTreeWidgetItem(QStringList(tr("ed2k/eMule")));
	QPixmap MuleIcon(":/Icons/Ed2kMule");
	pEd2kMule->setIcon(0, MuleIcon.scaled(16,16));
	m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pEd2kMule);
	QMultiMap<int, SField> Ed2kMule;
	SetupEd2kMule(Ed2kMule);
	m_pEd2kMule = new CSettingsView(Ed2kMule, tr("ed2k/eMule Options"));
	m_pSettingsLayout->addWidget(m_pEd2kMule);
	// 

	QTreeWidgetItem* pAdcanced = new QTreeWidgetItem(QStringList(tr("Advanced")));
	QPixmap AdvancedIcon(":/Icons/Settings");
	pAdcanced->setIcon(0, AdvancedIcon.scaled(16,16));
	m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pAdcanced);
	QMultiMap<int, SField> Adcanced;
	SetupAdvanced(Adcanced);
	m_pAdcanced = new CSettingsView(Adcanced, tr("Advanced Options"));
	m_pSettingsLayout->addWidget(m_pAdcanced);

	// Advanced
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
	{
		QTreeWidgetItem* pAdv = new QTreeWidgetItem(QStringList(tr("Properties")));
		QPixmap AdvIcon(":/Icons/Settings");
		pAdv->setIcon(0, AdvIcon.scaled(16,16));
		m_pGroupeTree->insertTopLevelItem(m_pGroupeTree->topLevelItemCount(),pAdv);
		m_pProperties = new CPropertiesView();
		m_pSettingsLayout->addWidget(m_pProperties);
	}
	else
		m_pProperties = NULL;
	//

	m_pSettingsWidget->setLayout(m_pSettingsLayout);

	m_pMainLayout->addWidget(m_pSettingsWidget, 0, 1);

	m_pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, Qt::Horizontal, this);
	QObject::connect(m_pButtons, SIGNAL(clicked(QAbstractButton *)), this, SLOT(OnClicked(QAbstractButton*)));
	m_pMainLayout->addWidget(m_pButtons, 1, 1);

	m_pMainWidget->setLayout(m_pMainLayout);

	setCentralWidget(m_pMainWidget);

	restoreGeometry(theGUI->Cfg()->GetBlob("Gui/Widget_Settings_Dialog"));
	int Index = theGUI->Cfg()->GetInt("Gui/Widget_Settings_Page");
	if(Index == -1)
		m_pSettingsLayout->setCurrentIndex(0);
	else if(QTreeWidgetItem* pItem = m_pGroupeTree->topLevelItem(Index))
	{
		pItem->setSelected(true);
		m_pSettingsLayout->setCurrentIndex(Index);
		Open();
	}
}

CSettingsWindow::~CSettingsWindow()
{
	theGUI->Cfg()->SetBlob("Gui/Widget_Settings_Dialog",saveGeometry());
	theGUI->Cfg()->SetSetting("Gui/Widget_Settings_Page", isVisible() ? m_pSettingsLayout->currentIndex() : -1);
}

void CSettingsWindow::Open()
{
	if(isVisible())
	{
		hide();
		show();
		return;
	}
	show();
	LoadAll();
}

void CSettingsWindow::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	m_pSettingsLayout->setCurrentIndex(m_pGroupeTree->indexOfTopLevelItem(pItem));

	if(m_pShare == m_pSettingsLayout->currentWidget())
		m_pShare->UpdateSettings();
}

void CSettingsWindow::OnSelectionChanged()
{
	if(QTreeWidgetItem* pItem = m_pGroupeTree->currentItem())
		OnItemClicked(pItem, 0);
}

void CSettingsWindow::OnClicked(QAbstractButton* pButton)
{
	switch(m_pButtons->buttonRole(pButton))
	{
	case QDialogButtonBox::RejectRole:
		this->hide();
		break;
	case QDialogButtonBox::AcceptRole:
		this->hide();
	case QDialogButtonBox::ApplyRole:
		SaveAll();
	case QDialogButtonBox::ResetRole: // reset after apply to check if all values ware accepted properly
		LoadAll();
		break;
	}
}

void CSettingsWindow::LoadAll()
{
	if(m_pProperties)
	{
		m_pProperties->UpdateSettings();
	}

	m_pInterface->UpdateSettings();
	m_pGeneral->UpdateSettings();
	m_pNetwork->UpdateSettings();
	m_pHoster->UpdateSettings();
	m_pNeoShare->UpdateSettings();
	m_pBitTorrent->UpdateSettings();
	m_pEd2kMule->UpdateSettings();
	m_pAdcanced->UpdateSettings();
}

void CSettingsWindow::SaveAll()
{
	if(m_pProperties && m_pProperties == m_pSettingsLayout->currentWidget())
	{
		m_pProperties->ApplySettings();
		return;
	}

	m_pInterface->ApplySettings();
	m_pGeneral->ApplySettings();
	m_pNetwork->ApplySettings();
	m_pHoster->ApplySettings();
	m_pNeoShare->ApplySettings();
	m_pBitTorrent->ApplySettings();
	m_pEd2kMule->ApplySettings();
	m_pAdcanced->ApplySettings();

	m_pShare->ApplySettings();
}

void CSettingsWindow::SetupInterface(QMultiMap<int, SField>& Interface)
{
	int Counter = 0;
	Interface.insert(Counter, SField(new QLabel(tr("GUI")), QFormLayout::SpanningRole));
#ifndef __APPLE__
	Counter++;
	QComboBox* pLanguage = new QComboBox();
	pLanguage->setMaximumWidth(200);
	pLanguage->addItem(QString::fromWCharArray(L"English"), "en");
	pLanguage->addItem(QString::fromWCharArray(L"Deutsch (thecoder2012)"), "de");
	pLanguage->addItem(QString::fromWCharArray(L"Italiano (Neo26)"), "it");
	pLanguage->addItem(QString::fromWCharArray(L"Español (petermrg)"), "es");
	pLanguage->addItem(QString::fromWCharArray(L"Polski (Zebra)"), "pl");
	pLanguage->addItem(QString::fromWCharArray(L"台灣正體中文"), "zh_tw");
	pLanguage->addItem(QString::fromWCharArray(L"大陆简体中文(Continuer)"), "zh");
	//pLanguage->addItem(QString::fromWCharArray(L"香港繁體中文"), "zh_hk");
	//pLanguage->addItem(QString::fromWCharArray(L"马新简体中文"), "zh_sg");

	Interface.insert(Counter, SField(new QLabel(tr("Language")), QFormLayout::LabelRole));
	Interface.insert(Counter, SField(pLanguage, QFormLayout::FieldRole, "NeoLoader/Gui/Language"));
#endif
	Counter++;
	QCheckBox* pClassic = new QCheckBox(tr("Show Simple ProgressBars"));
	Interface.insert(Counter, SField(pClassic, QFormLayout::FieldRole, "NeoLoader/Gui/SimpleBars"));
	Counter++;
	//QCheckBox* pGrabber = new QCheckBox(tr("Use Simple Grabber"));
	//Interface.insert(Counter, SField(pGrabber, QFormLayout::FieldRole, "NeoLoader/Gui/UnifyGrabber"));
	//Counter++;
	QCheckBox* pAdvanced = new QSecretCheckBox(tr("Show Advanced Controls"));
	Interface.insert(Counter, SField(pAdvanced, QFormLayout::FieldRole, "NeoLoader/Gui/AdvancedControls"));
	/*Counter++;
	QCheckBox* pVerbose = new QCheckBox(tr("Advanced Graphs"));
	pVerbose->setToolTip(tr("Solid Line '_______' File Payload\r\n"
							"Dash Line '-------' Protocol Overhead\r\n"
							"Dash Dot Line '-.-.-.-' IP Header Overhead\r\n"
							"Dash Dot Dot Line  '-..-..-' IP ACK Overhead\r\n"
							"Dot Line '.......' IP SYN Overhead"));
	Interface.insert(Counter, SField(pVerbose, QFormLayout::FieldRole, "NeoLoader/Gui/VerboseGraphs"));*/
	Counter++;
	QCheckBox* pLogs = new QCheckBox(tr("Show Log Tabs"));
	Interface.insert(Counter, SField(pLogs, QFormLayout::FieldRole, "NeoLoader/Gui/ShowLog"));
	Counter++;
	QComboBox* pSort = new QSortOptions();
	pSort->setMaximumWidth(150);
	Interface.insert(Counter, SField(new QLabel(tr("Auto Sort:")), QFormLayout::LabelRole));
	Interface.insert(Counter, SField(pSort, QFormLayout::FieldRole, "NeoLoader/Gui/AutoSort"));
	Counter++;
	QCheckBox* pAlternate = new QCheckBox(tr("Alternate list Background shade"));
	Interface.insert(Counter, SField(pAlternate, QFormLayout::FieldRole, "NeoLoader/Gui/Alternate"));
	Counter++;
	QCheckBox* pGrabber2 = new QCheckBox(tr("Skip Grabber"));
	Interface.insert(Counter, SField(pGrabber2, QFormLayout::FieldRole, "NeoLoader/Integration/SkipGrabber"));
	Counter++;
	QCheckBox* pControl = new QCheckBox(tr("Locks SubFile Status to Master"));
	Interface.insert(Counter, SField(pControl, QFormLayout::FieldRole, "NeoLoader/Gui/SubControl"));


	Counter++;
	QComboBox* pOpen = new QComboBox();
	pOpen->addItem(tr("Open File"), "Open");
	pOpen->addItem(tr("Show Details"), "Tabs");
	pOpen->addItem(tr("Togle Start/Stop"), "Togle");
	pOpen->setMaximumWidth(150);
	Interface.insert(Counter, SField(new QLabel(tr("On File Dbl Click:")), QFormLayout::LabelRole));
	Interface.insert(Counter, SField(pOpen, QFormLayout::FieldRole, "NeoLoader/Gui/OnDblClick"));
	Counter++;
	QComboBox* pPlayer = new QComboBox();
	pPlayer->setMaximumWidth(150);
	pPlayer->addItem(tr("Use VLC Player, path:"), false);
	pPlayer->addItem(tr("Use build in Player"), true);
	//QCheckBox* pPlayer = new QCheckBox(tr("Build in Player or:"));
	Interface.insert(Counter, SField(pPlayer, QFormLayout::LabelRole, "NeoLoader/Gui/InternPlayer"));
	CPathEdit* pPlayerEx = new CPathEdit();
	pPlayerEx->setMinimumWidth(200);
	Interface.insert(Counter, SField(pPlayerEx, QFormLayout::FieldRole, "NeoLoader/Gui/ExternPlayer"));

	Counter++;
	Interface.insert(Counter, SField(new QLabel(tr("Access")), QFormLayout::SpanningRole));
	Counter++;
	QLineEdit* pPasswod = new QLineEdit();
	pPasswod->setMaximumWidth(100);
	Interface.insert(Counter, SField(new QLabel(tr("Password")), QFormLayout::LabelRole));
	Interface.insert(Counter, SField(pPasswod, QFormLayout::FieldRole, "NeoLoader/Core/Password"));
	Counter++;
	QLineEdit* pHttpPort = new QLineEdit();
	pHttpPort->setMaximumWidth(100);
	Interface.insert(Counter, SField(new QLabel(tr("HTTP Port")), QFormLayout::LabelRole));
	Interface.insert(Counter, SField(pHttpPort, QFormLayout::FieldRole, "NeoLoader/HttpServer/Port"));

	Counter++;
	Interface.insert(Counter, SField(new QLabel(tr("Intergration")), QFormLayout::SpanningRole));
#ifdef WIN32
	Counter++;
	CShellIntegrateBox* pShellIntegrateBox = new CShellIntegrateBox(tr("Register File Extensions and Link Schemes"));
	pShellIntegrateBox->setToolTip(tr("Hold Ctrl during click for options"));
	Interface.insert(Counter, SField(pShellIntegrateBox, QFormLayout::FieldRole, ""));
#endif

	Counter++;
	QComboBox* pSysTray = new QComboBox();
	pSysTray->addItem(tr("No SysTray"), 0);
	pSysTray->addItem(tr("Enable SysTray"), 1);
	pSysTray->addItem(tr("Start in SysTray"), 2);
	Interface.insert(Counter, SField(pSysTray, QFormLayout::LabelRole, "NeoLoader/Integration/SysTray"));

	QComboBox* pTpTray = new QComboBox();
	pTpTray->setMaximumWidth(150);
	pTpTray->addItem(tr("Double Click Tray"), 0);
	pTpTray->addItem(tr("Minimize to Tray"), 1);
	pTpTray->addItem(tr("Close to Tray"), 2);
	Interface.insert(Counter, SField(pTpTray, QFormLayout::FieldRole, "NeoLoader/Integration/SysTrayTrigger"));

	/*Counter++;
	QCheckBox* pSeparate = new QCheckBox(tr("Run GUI in separate Process"));
	Interface.insert(Counter, SField(pSeparate, QFormLayout::FieldRole, "NeoLoader/Integration/Separate"));*/
}

void CSettingsWindow::SetupGeneral(QMultiMap<int, SField>& General)
{
	int Counter = 0;
	General.insert(Counter, SField(new QLabel(tr("Bandwidth:")), QFormLayout::SpanningRole));
	Counter++;
	CKbpsEdit* pDownRate = new CKbpsEdit();
	pDownRate->setMaximumWidth(100);
	General.insert(Counter, SField(new QLabel(tr("Download (KB/s)")), QFormLayout::LabelRole));
	General.insert(Counter, SField(pDownRate, QFormLayout::FieldRole, "NeoCore/Bandwidth/Download"));
	Counter++;
	CKbpsEdit* pUpRate = new CKbpsEdit();
	pUpRate->setMaximumWidth(100);
	General.insert(Counter, SField(new QLabel(tr("Upload (KB/s)")), QFormLayout::LabelRole));
	General.insert(Counter, SField(pUpRate, QFormLayout::FieldRole, "NeoCore/Bandwidth/Upload"));

	Counter++;
	General.insert(Counter, SField(new QLabel(tr("Content:")), QFormLayout::SpanningRole));
	Counter++;
	CFactorEdit* pShareRatio = new CFactorEdit(100);
	pShareRatio->setMaximumWidth(100);
	General.insert(Counter, SField(new QLabel(tr("Sharing Ratio")), QFormLayout::LabelRole));
	General.insert(Counter, SField(pShareRatio, QFormLayout::FieldRole, "NeoCore/Content/ShareRatio"));
	Counter++;
	General.insert(Counter, SField(new QCheckBox(tr("Add files paused")), QFormLayout::FieldRole, "NeoCore/Content/AddPaused"));
	Counter++;
	General.insert(Counter, SField(new QCheckBox(tr("Start new share files")), QFormLayout::FieldRole, "NeoCore/Content/ShareNew"));
	Counter++;
	General.insert(Counter, SField(new QCheckBox(tr("Auto Add Networks if file stalls")), QFormLayout::FieldRole, "NeoCore/Content/AutoP2P"));
	Counter++;
	QCheckBox* pPreview = new QCheckBox(tr("Download Preview sections first"));
	pPreview->setToolTip(tr("Requirerst file start stop or Neo restart to take effect"));
	General.insert(Counter, SField(pPreview, QFormLayout::FieldRole, "NeoCore/Content/PreparePreview"));

	Counter++;
	General.insert(Counter, SField(new QLabel(tr("Other:")), QFormLayout::SpanningRole));
	Counter++;
	General.insert(Counter, SField(new QCheckBox(tr("Save Search Results")), QFormLayout::FieldRole, "NeoCore/Content/SaveSearch"));

	Counter++;
	QCheckBox* pAutoUpdate = new QCheckBox(tr("Auto Update"));
	pAutoUpdate->setToolTip(tr("Semi Checked - Ask if to install or not"));
	pAutoUpdate->setTristate(true);
	QComboBox* pUpdateTime = new QComboBox();
	pUpdateTime->setMaximumWidth(150);
	pUpdateTime->addItem(tr("On Starup Only"), 0);
	pUpdateTime->addItem(tr("Every Day"), DAY2S(1));
	pUpdateTime->addItem(tr("Every 3 Days"), DAY2S(3));
	pUpdateTime->addItem(tr("Every 7 Days"), DAY2S(7));
	General.insert(Counter, SField(pAutoUpdate, QFormLayout::LabelRole, "NeoLoader/Updater/AutoUpdate"));
	General.insert(Counter, SField(pUpdateTime, QFormLayout::FieldRole, "NeoLoader/Updater/UpdateInterval"));

	//Counter++;
	//QCheckBox* pAutoStart = new QCheckBox(tr("Module Autostart"));
	//pAutoStart->setTristate();
	//General.insert(Counter, SField(pAutoStart, QFormLayout::FieldRole, "NeoCore/Modules/AutoStart"));
	Counter++;
	General.insert(Counter, SField(new QCheckBox(tr("Prevent Standby")), QFormLayout::FieldRole, "NeoCore/Other/PreventStandby"));
}

void CSettingsWindow::SetupNetwork(QMultiMap<int, SField>& Network)
{
	int Counter = 0;

	QLineEdit* pMaxCon = new QLineEdit();
	pMaxCon->setMaximumWidth(100);
	Network.insert(Counter, SField(new QLabel(tr("Max Connections")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pMaxCon, QFormLayout::FieldRole, "NeoCore/Bandwidth/MaxConnections"));
	Counter++;
	QLineEdit* pMaxNew = new QLineEdit();
	pMaxNew->setMaximumWidth(100);
	Network.insert(Counter, SField(new QLabel(tr("Max New Con / 5 Sec")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pMaxNew, QFormLayout::FieldRole, "NeoCore/Bandwidth/MaxNewPer5Sec"));
	Counter++;
	Network.insert(Counter, SField(new QCheckBox(tr("Use UPnP/NatPMP")), QFormLayout::FieldRole, "NeoCore/Bandwidth/UseUPnP"));

	Counter++;
	Network.insert(Counter, SField(new QLabel(tr("Upload Speed Sense:")), QFormLayout::SpanningRole));

	Counter++;
	CUSSEdit* pUSS = new CUSSEdit();
	pUSS->setMaximumWidth(150);
	Network.insert(Counter, SField(new QLabel(tr("Configuration")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pUSS, QFormLayout::FieldRole, "NeoCore/Pinger/ToleranceVal"));

	Counter++;
	QLineEdit* pUpDivider = new QLineEdit();
	pUpDivider->setMaximumWidth(100);
	Network.insert(Counter, SField(new QLabel(tr("Speed Up Factor")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pUpDivider, QFormLayout::FieldRole, "NeoCore/Pinger/UpDivider"));
	Counter++;
	QLineEdit* pDownDivider = new QLineEdit();
	pDownDivider->setMaximumWidth(100);
	Network.insert(Counter, SField(new QLabel(tr("Slow Down Factor")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pDownDivider, QFormLayout::FieldRole, "NeoCore/Pinger/DownDivider"));

	Counter++;
	Network.insert(Counter, SField(new QLabel(tr("Upload Management:")), QFormLayout::SpanningRole));
	Counter++;
	CKbpsEdit* pUpSlot = new CKbpsEdit();
	pUpSlot->setMaximumWidth(100);
	Network.insert(Counter, SField(new QLabel(tr("Slot Speed (KB/s)")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pUpSlot, QFormLayout::FieldRole, "NeoCore/Upload/SlotSpeed"));
	//Counter++;
	//Network.insert(Counter, SField(new QCheckBox(tr("Slot Focus [!]")), QFormLayout::FieldRole, "NeoCore/Upload/SlotFocus"));
	Counter++;
	QLineEdit* pTrickles = new QLineEdit();
	pTrickles->setMaximumWidth(100);
	Network.insert(Counter, SField(new QLabel(tr("Min Trickle [*] Slots")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pTrickles, QFormLayout::FieldRole, "NeoCore/Upload/TrickleVolume"));
	Counter++;
	Network.insert(Counter, SField(new QCheckBox(tr("Drop Blocking [#] Slots")), QFormLayout::FieldRole, "NeoCore/Upload/DropBlocking"));

	Counter++;
	Network.insert(Counter, SField(new QLabel(tr("Protection:")), QFormLayout::SpanningRole));
	Counter++;
	QComboBox* pNIC = new QNICCombo();
	pNIC->setMaximumWidth(300);
	Network.insert(Counter, SField(new QLabel(tr("Lock P2P to VPN Adapter")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pNIC, QFormLayout::FieldRole, "NeoCore/Bandwidth/DefaultNIC"));
	Counter++;
	CProxyEdit* pProxy = new CProxyEdit();
	pProxy->setMaximumWidth(300);
	Network.insert(Counter, SField(new QLabel(tr("Proxy for Hosters")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pProxy, QFormLayout::FieldRole, "NeoCore/Bandwidth/WebProxy"));
	Counter++;
	QLineEdit* pFilter = new QLineEdit();
	pFilter->setMaximumWidth(300);
	/*QCheckBox* pPeerWatch = new QCheckBox(tr("Enable IP Filter - URL"));
	pPeerWatch->setTristate(true);
	pPeerWatch->setToolTip(tr("Semi Checked - Filter only Banned Clients\r\nFully Checked - Use global IP Filter List"));
	Network.insert(Counter, SField(pPeerWatch, QFormLayout::LabelRole, "NeoCore/PeerWatch/Enable"));
	Network.insert(Counter, SField(new QLabel(tr("IP Filter URL:")), QFormLayout::LabelRole));
	Network.insert(Counter, SField(pFilter, QFormLayout::FieldRole, "NeoCore/PeerWatch/IPFilter"));*/

	Counter++;
	Network.insert(Counter, SField(new QLabel(tr("Advanced:")), QFormLayout::SpanningRole));
	Counter++;
	Network.insert(Counter, SField(new QCheckBox(tr("Apply bandwidth limit to transport overhead")), QFormLayout::FieldRole, "NeoCore/Bandwidth/TransportLimiting"));
}

void CSettingsWindow::SetupShared(QMultiMap<int, SField>& Shared)
{
	int Counter = 0;
	Shared.insert(Counter, SField(new QLabel(tr("Directories:")), QFormLayout::SpanningRole));
	Counter++;
	CPathEdit* pIncoming = new CPathEdit(true);
	Shared.insert(Counter, SField(new QLabel(tr("Incoming")), QFormLayout::LabelRole));
	Shared.insert(Counter, SField(pIncoming, QFormLayout::FieldRole, "NeoCore/Content/Incoming"));
	Counter++;
	CPathEdit* pTemp = new CPathEdit(true);
	Shared.insert(Counter, SField(new QLabel(tr("Temp")), QFormLayout::LabelRole));
	Shared.insert(Counter, SField(pTemp, QFormLayout::FieldRole, "NeoCore/Content/Temp"));
}

void CSettingsWindow::SetupHosters(QMultiMap<int, SField>& Hoster)
{
	int Counter = 0;
	QCheckBox* pEnable = new QCheckBox(tr("Enable Extended Hosters"));
	pEnable->setToolTip(tr("Extended hoster Support,\nincludes hoster cache,\nautomatic hoster upload\nas well as aotomatik link lookup in neo kad"));
	Hoster.insert(Counter, SField(pEnable, QFormLayout::FieldRole, "NeoCore/Hoster/Enable"));
	Counter++;
	Hoster.insert(Counter, SField(new QCheckBox(tr("Download parts also when Cpatcha is needed")), QFormLayout::FieldRole, "NeoCore/Hoster/UseCaptcha"));
	Counter++;
	Hoster.insert(Counter, SField(new QLabel(tr("Connection Settings:")), QFormLayout::SpanningRole));
	Counter++;
	QLineEdit* pMaxUp = new QLineEdit();
	pMaxUp->setMaximumWidth(100);
	Hoster.insert(Counter, SField(new QLabel(tr("Max parallel Uploads")), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pMaxUp, QFormLayout::FieldRole, "NeoCore/Hoster/MaxUploads"));
	Counter++;
	QLineEdit* pMaxDown = new QLineEdit();
	pMaxDown->setMaximumWidth(100);
	Hoster.insert(Counter, SField(new QLabel(tr("Max parallel Downloads")), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pMaxDown, QFormLayout::FieldRole, "NeoCore/Hoster/MaxDownloads"));
	Counter++;
	QLineEdit* pMaxRetry = new QLineEdit();
	pMaxRetry->setMaximumWidth(100);
	Hoster.insert(Counter, SField(new QLabel(tr("Transfer Retries")), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pMaxRetry, QFormLayout::FieldRole, "NeoCore/Hoster/MaxTransferRetry"));
	Counter++;
	QLineEdit* pRetryDelay = new QLineEdit();
	pRetryDelay->setMaximumWidth(100);
	Hoster.insert(Counter, SField(new QLabel(tr("Transfer Retry Delay")), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pRetryDelay, QFormLayout::FieldRole, "NeoCore/Hoster/ServerRetryDelay"));
	
	Counter++;
	Hoster.insert(Counter, SField(new QLabel(tr("Upload:")), QFormLayout::SpanningRole));
	Counter++;
	QComboBox* pEncr = new QComboBox();
	pEncr->setMaximumWidth(150);
	pEncr->addItem(tr("No"), "");
	pEncr->addItem(tr("RC4"), "RC4");
	pEncr->addItem(tr("AES"), "AES");
	Hoster.insert(Counter, SField(new QLabel(tr("Encrypt uploaded parts")), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pEncr, QFormLayout::FieldRole, "NeoCore/Hoster/EncryptUploads"));
	Counter++;
	Hoster.insert(Counter, SField(new QCheckBox(tr("Enable Auto ReUpload")), QFormLayout::FieldRole, "NeoCore/Hoster/AutpReUpload"));
	Counter++;
	Hoster.insert(Counter, SField(new QCheckBox(tr("Protect published Links")), QFormLayout::FieldRole, "NeoCore/Hoster/ProtectLinks"));
	Counter++;
	QComboBox* pUploadMode = new QComboBox();
	pUploadMode->setMaximumWidth(150);
	pUploadMode->addItem(tr("Disabled"), "Off");
	pUploadMode->addItem(tr("On Request"), "On");
	pUploadMode->addItem(tr("Preemptive"), "All");
	Hoster.insert(Counter, SField(new QLabel("Hoster Cache Upload:"), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pUploadMode, QFormLayout::FieldRole, "NeoCore/HosterCache/CacheMode"));
	Counter++;
	QComboBox* pSelectionMode = new QComboBox();
	pSelectionMode->setMaximumWidth(150);
	pSelectionMode->addItem(tr("All Hosters"), "All");
	pSelectionMode->addItem(tr("Automatic Selection"), "Auto");
	pSelectionMode->addItem(tr("Selected Hosters"), "Manual");
	Hoster.insert(Counter, SField(new QLabel("Upload Hoster Selection:"), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pSelectionMode, QFormLayout::FieldRole, "NeoCore/HosterCache/SelectionMode"));
	Counter++;
	CKbpsEdit* pPartSize = new CKbpsEdit(1024*1024);
	pPartSize->setMaximumWidth(150);
	Hoster.insert(Counter, SField(new QLabel(tr("Enforce Part Size (MB)")), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pPartSize, QFormLayout::FieldRole, "NeoCore/Hoster/PartSize"));

	Counter++;
	Hoster.insert(Counter, SField(new QLabel(tr("Archive Handling:")), QFormLayout::SpanningRole));
	Counter++;
	Hoster.insert(Counter, SField(new QCheckBox(tr("Remove Parts after extract")), QFormLayout::FieldRole, "NeoCore/Hoster/AutoCleanUp"));
	Counter++;
	Hoster.insert(Counter, SField(new QCheckBox(tr("Don't auto extract single part archives")), QFormLayout::FieldRole, "NeoCore/Hoster/KeepSingleArchives"));
	Counter++;
	QComboBox* pEncap = new QComboBox();
	pEncap->setMaximumWidth(150);
	pEncap->addItem(tr("7z"), "7z");
	pEncap->addItem(tr("rar"), "rar");
	Hoster.insert(Counter, SField(new QLabel(tr("Archive Upload Format")), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pEncap, QFormLayout::FieldRole, "NeoCore/Hoster/Encap"));
	Counter++;
	CPathEdit* pRarPath = new CPathEdit(true);
	pRarPath->setMaximumWidth(200);
	Hoster.insert(Counter, SField(new QLabel(tr("Set path to WinRar for rar format")), QFormLayout::LabelRole));
	Hoster.insert(Counter, SField(pRarPath, QFormLayout::FieldRole, "NeoCore/Hoster/RarPath"));
}

void CSettingsWindow::SetupNeoShare(QMultiMap<int, SField>& NeoShare)
{
	int Counter = 0;
	/*QPixmap Icon(":/Icons/NeoShare");
	QLabel* pIcon = new QLabel();
	pIcon->setPixmap(Icon.scaled(16,16));
	NeoShare.insert(Counter, SField(pIcon, QFormLayout::LabelRole));*/
	NeoShare.insert(Counter, SField(new QCheckBox(tr("Enable NeoShare")), QFormLayout::FieldRole, "NeoCore/NeoShare/Enable"));

	Counter++;
	CAnonSlider* pAnon = new CAnonSlider();
	NeoShare.insert(Counter, SField(pAnon, QFormLayout::SpanningRole, "NeoCore/NeoShare/Anonymity"));

	Counter++;
	NeoShare.insert(Counter, SField(new QLabel(tr("Source Management:")), QFormLayout::SpanningRole));
	Counter++;
	QLineEdit* pMaxEntities = new QLineEdit();
	pMaxEntities->setMaximumWidth(100);
	NeoShare.insert(Counter, SField(new QLabel(tr("Max Entities per File")), QFormLayout::LabelRole));
	NeoShare.insert(Counter, SField(pMaxEntities, QFormLayout::FieldRole, "NeoCore/NeoShare/MaxEntities"));
	Counter++;
	NeoShare.insert(Counter, SField(new QCheckBox(tr("Save Neo Entities")), QFormLayout::FieldRole, "NeoCore/NeoShare/SaveEntities"));

	Counter++;
	NeoShare.insert(Counter, SField(new QLabel(tr("Connection Settings:")), QFormLayout::SpanningRole));
	Counter++;
	QLineEdit* pPort = new QLineEdit();
	pPort->setMaximumWidth(100);
	NeoShare.insert(Counter, SField(new QLabel(tr("Kad Port (UDP)")), QFormLayout::LabelRole));
	NeoShare.insert(Counter, SField(pPort, QFormLayout::FieldRole, "NeoCore/NeoKad/Port"));
}

void CSettingsWindow::SetupBitTorrent(QMultiMap<int, SField>& BitTorrent)
{
	int Counter = 0;
	/*QPixmap Icon(":/Icons/BitTorrent");
	QLabel* pIcon = new QLabel();
	pIcon->setPixmap(Icon.scaled(16,16));
	BitTorrent.insert(Counter, SField(pIcon, QFormLayout::LabelRole));*/
	BitTorrent.insert(Counter, SField(new QCheckBox(tr("Enable BitTorrent")), QFormLayout::FieldRole, "NeoCore/BitTorrent/Enable"));
	Counter++;
	QLineEdit* pMax = new QLineEdit();
	pMax->setMaximumWidth(100);
	BitTorrent.insert(Counter, SField(new QLabel(tr("Max Active Torrents")), QFormLayout::LabelRole));
	BitTorrent.insert(Counter, SField(pMax, QFormLayout::FieldRole, "NeoCore/BitTorrent/MaxTorrents"));

	Counter++;
	BitTorrent.insert(Counter, SField(new QLabel(tr("Source Management:")), QFormLayout::SpanningRole));
	Counter++;
	QLineEdit* pMaxPeers = new QLineEdit();
	pMaxPeers->setMaximumWidth(100);
	BitTorrent.insert(Counter, SField(new QLabel(tr("Max Connections per File")), QFormLayout::LabelRole));
	BitTorrent.insert(Counter, SField(pMaxPeers, QFormLayout::FieldRole, "NeoCore/BitTorrent/MaxPeers"));
	Counter++;
	BitTorrent.insert(Counter, SField(new QCheckBox(tr("Save Torrent Peers")), QFormLayout::FieldRole, "NeoCore/BitTorrent/SavePeers"));
	Counter++;
	QComboBox* pTracking = new QComboBox();
	pTracking->setMaximumWidth(100);
	pTracking->addItem(tr("No Trackers"), "No-Trackers");
	pTracking->addItem(tr("One Tracker"), "One-Tracker");
	pTracking->addItem(tr("All Trackers"), "All-Trackers");
	pTracking->addItem(tr("All Tiers"), "All-Tiers");
	BitTorrent.insert(Counter, SField(new QLabel(tr("Torrent Tracking")), QFormLayout::LabelRole));
	BitTorrent.insert(Counter, SField(pTracking, QFormLayout::FieldRole, "NeoCore/BitTorrent/Tracking"));

	Counter++;
	BitTorrent.insert(Counter, SField(new QLabel(tr("Connection Settings:")), QFormLayout::SpanningRole));
	Counter++;
	QLineEdit* pPort = new QLineEdit();
	pPort->setMaximumWidth(100);
	BitTorrent.insert(Counter, SField(new QLabel(tr("Port TCP and UDP")), QFormLayout::LabelRole));
	BitTorrent.insert(Counter, SField(pPort, QFormLayout::FieldRole, "NeoCore/BitTorrent/ServerPort"));
	Counter++;
	QComboBox* pEncryption = new QComboBox();
	pEncryption->setMaximumWidth(100);
	pEncryption->addItem(tr("Disable"), "Disable");
	pEncryption->addItem(tr("Support"), "Support");
	pEncryption->addItem(tr("Request"), "Request");
	pEncryption->addItem(tr("Require"), "Require");
	pEncryption->setCurrentIndex(1); // default
	BitTorrent.insert(Counter, SField(new QLabel(tr("Connection Encryption")), QFormLayout::LabelRole));
	BitTorrent.insert(Counter, SField(pEncryption, QFormLayout::FieldRole, "NeoCore/BitTorrent/Encryption"));
}

void CSettingsWindow::SetupEd2kMule(QMultiMap<int, SField>& Ed2kMule)
{
	int Counter = 0;
	/*QPixmap Icon(":/Icons/Ed2kMule");
	QLabel* pIcon = new QLabel();
	pIcon->setPixmap(Icon.scaled(16,16));
	Ed2kMule.insert(Counter, SField(pIcon, QFormLayout::LabelRole));*/
	Ed2kMule.insert(Counter, SField(new QCheckBox(tr("Enable ed2k/eMule")), QFormLayout::FieldRole, "NeoCore/Ed2kMule/Enable"));
	Counter++;
	Ed2kMule.insert(Counter, SField(new QCheckBox(tr("Share new files automatically")), QFormLayout::FieldRole, "NeoCore/Ed2kMule/ShareDefault"));

	Counter++;
	Ed2kMule.insert(Counter, SField(new QLabel(tr("Source Management:")), QFormLayout::SpanningRole));
	Counter++;
	QLineEdit* pMaxSources = new QLineEdit();
	pMaxSources->setMaximumWidth(100);
	Ed2kMule.insert(Counter, SField(new QLabel(tr("Max Sources per File")), QFormLayout::LabelRole));
	Ed2kMule.insert(Counter, SField(pMaxSources, QFormLayout::FieldRole, "NeoCore/Ed2kMule/MaxSources"));
	Counter++;
	Ed2kMule.insert(Counter, SField(new QCheckBox(tr("Save ed2k/eMule Sources")), QFormLayout::FieldRole, "NeoCore/Ed2kMule/SaveSources"));

	Counter++;
	Ed2kMule.insert(Counter, SField(new QLabel(tr("Connection Settings:")), QFormLayout::SpanningRole));
	Counter++;
	QLineEdit* pTCP = new QLineEdit();
	pTCP->setMaximumWidth(100);
	Ed2kMule.insert(Counter, SField(new QLabel(tr("TCP Port")), QFormLayout::LabelRole));
	Ed2kMule.insert(Counter, SField(pTCP, QFormLayout::FieldRole, "NeoCore/Ed2kMule/TCPPort"));
	Counter++;
	QLineEdit* pUDP = new QLineEdit();
	pUDP->setMaximumWidth(100);
	Ed2kMule.insert(Counter, SField(new QLabel(tr("UDP Port")), QFormLayout::LabelRole));
	Ed2kMule.insert(Counter, SField(pUDP, QFormLayout::FieldRole, "NeoCore/Ed2kMule/UDPPort"));
	Counter++;
	QComboBox* pEncryption = new QComboBox();
	pEncryption->setMaximumWidth(100);
	pEncryption->addItem(tr("Disable"), "Disable");
	pEncryption->addItem(tr("Support"), "Support");
	pEncryption->addItem(tr("Request"), "Request");
	pEncryption->addItem(tr("Require"), "Require");
	pEncryption->setCurrentIndex(1); // default
	Ed2kMule.insert(Counter, SField(new QLabel(tr("Connection Obfuscation")), QFormLayout::LabelRole));
	Ed2kMule.insert(Counter, SField(pEncryption, QFormLayout::FieldRole, "NeoCore/Ed2kMule/Obfuscation"));

	Counter++;
	Ed2kMule.insert(Counter, SField(new QLabel(tr("Client Identification:")), QFormLayout::SpanningRole));
	Counter++;
	QComboBox* pTracking = new QComboBox();
	pTracking->setMaximumWidth(250);
	pTracking->addItem(tr("Secure Random"), "Random-Secure");
	pTracking->addItem(tr("Secure Static"), "Static-Secure");
	//pTracking->addItem(tr("Random"), "Random");
	//pTracking->addItem(tr("Static"), "Static");
	Ed2kMule.insert(Counter, SField(new QLabel(tr("Hash (restart required)")), QFormLayout::LabelRole));
	Ed2kMule.insert(Counter, SField(pTracking, QFormLayout::FieldRole, "NeoCore/Ed2kMule/HashMode"));

	Counter++;
	QLineEdit* pNick = new QLineEdit();
	pNick->setMaximumWidth(250);
	Ed2kMule.insert(Counter, SField(new QLabel(tr("Ed2k UserName")), QFormLayout::LabelRole));
	Ed2kMule.insert(Counter, SField(pNick, QFormLayout::FieldRole, "NeoCore/Ed2kMule/NickName"));

	Counter++;
	Ed2kMule.insert(Counter, SField(new QLabel(tr("Ed2k Server Options:")), QFormLayout::SpanningRole));
	Counter++;
	QComboBox* pServers = new QComboBox();
	pServers->setMaximumWidth(250);
	pServers->addItem(tr("No Servers"), "None");
	pServers->addItem(tr("Only Custom"), "Custom");
	pServers->addItem(tr("Only Static"), "Static");
	pServers->addItem(tr("Custom or Static"), "One");
	pServers->addItem(tr("Custom and Static"), "Booth");
	Ed2kMule.insert(Counter, SField(pServers, QFormLayout::FieldRole, "NeoCore/Ed2kMule/UseServers"));
}

void CSettingsWindow::SetupAdvanced(QMultiMap<int, SField>& Advanced)
{
	int Counter = 0;
	Advanced.insert(Counter, SField(new QLabel(tr("GUI")), QFormLayout::SpanningRole));

	Counter++;
	QComboBox* pLogs = new QComboBox();
	pLogs->setMaximumWidth(150);
	pLogs->addItem(tr("Basic"), 0);
	pLogs->addItem(tr("Normal"), 1);
	pLogs->addItem(tr("Verbose"), 2);
	pLogs->addItem(tr("ExtVerbose"), 3);
	Advanced.insert(Counter, SField(new QLabel(tr("Log Level:")), QFormLayout::LabelRole));
	Advanced.insert(Counter, SField(pLogs, QFormLayout::FieldRole, "NeoCore/Log/Level"));
}



#ifdef WIN32
CShellIntegrateBox::CShellIntegrateBox(const QString& Text) : QCheckBox(Text) 
{
	connect(this, SIGNAL(clicked(bool)), this, SLOT(OnClicked(bool)));

	m_OpenPath = "\"" + QApplication::applicationFilePath().replace("/", "\\") + "\" \"%1\"";

	OnChanged();

	connect(&g_ShellSetup, SIGNAL(Changed()), this, SLOT(OnChanged()));
}

void CShellIntegrateBox::OnChanged()
{
	m_bEd2k = g_ShellSetup.TestProtocol("ed2k", m_OpenPath)
	&& g_ShellSetup.TestType("emulecollection", m_OpenPath, "NeoLoader");

	m_bMagnet = g_ShellSetup.TestProtocol("magnet", m_OpenPath);

	m_bTorrent = g_ShellSetup.TestType("torrent", m_OpenPath, "NeoLoader");

	m_bHoster = g_ShellSetup.TestProtocol("jd", m_OpenPath)
	&& g_ShellSetup.TestProtocol("jdlist", m_OpenPath)
	&& g_ShellSetup.TestProtocol("dlc", m_OpenPath)
	//&& g_ShellSetup.TestProtocol("ccf", m_OpenPath)
	//&& g_ShellSetup.TestProtocol("rsdf", m_OpenPath)
	//&& g_ShellSetup.TestType("ccf", m_OpenPath, "NeoLoader")
	//&& g_ShellSetup.TestType("rsdf", m_OpenPath, "NeoLoader")
	&& g_ShellSetup.TestType("dlc", m_OpenPath, "NeoLoader");
		
	setTristate(false);
	if(m_bEd2k && m_bMagnet && m_bTorrent && m_bHoster)
		setChecked(true);
	else if(!m_bEd2k && !m_bMagnet && !m_bTorrent && !m_bHoster)
		setChecked(false);
	else
		setCheckState(Qt::PartiallyChecked);
}

class CIntegrationDialog : public QDialogEx
{
	//Q_OBJECT

public:
	CIntegrationDialog(QWidget *pMainWindow = NULL)
		: QDialogEx(pMainWindow)
	{
		setWindowTitle(CPropertiesView::tr("Shell Integration"));

		m_pMainLayout = new QFormLayout(this);

		m_pMagnet = new QCheckBox(tr("Magnet URI scheme"));
		m_pMainLayout->setWidget(0, QFormLayout::FieldRole, m_pMagnet);
		m_pEd2k = new QCheckBox(tr("Ed2k/Mule Links"));
		m_pMainLayout->setWidget(1, QFormLayout::FieldRole, m_pEd2k);
		m_pTorrent = new QCheckBox(tr("Torrent Files"));
		m_pMainLayout->setWidget(2, QFormLayout::FieldRole, m_pTorrent);
		m_pHoster = new QCheckBox(tr("Hoster Link Lists (JD)"));
		m_pMainLayout->setWidget(3, QFormLayout::FieldRole, m_pHoster);

		m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
		QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
		QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
		m_pMainLayout->setWidget(4, QFormLayout::FieldRole, m_pButtonBox);
	}

protected:
	friend class CShellIntegrateBox;

	QCheckBox*			m_pMagnet;
	QCheckBox*			m_pEd2k;
	QCheckBox*			m_pTorrent;
	QCheckBox*			m_pHoster;

	QDialogButtonBox*	m_pButtonBox;
	QFormLayout*		m_pMainLayout;
};

void CShellIntegrateBox::OnClicked(bool Checked)
{
	if(QApplication::keyboardModifiers() & Qt::ControlModifier)
	{
		CIntegrationDialog IntegrationDialog;

		IntegrationDialog.m_pEd2k->setChecked(m_bEd2k);
		IntegrationDialog.m_pMagnet->setChecked(m_bMagnet);
		IntegrationDialog.m_pTorrent->setChecked(m_bTorrent);
		IntegrationDialog.m_pHoster->setChecked(m_bHoster);

		if(!IntegrationDialog.exec())
		{
			OnChanged();
			return;
		}

		m_bEd2k = IntegrationDialog.m_pEd2k->isChecked();
		m_bMagnet = IntegrationDialog.m_pMagnet->isChecked();
		m_bTorrent = IntegrationDialog.m_pTorrent->isChecked();
		m_bHoster = IntegrationDialog.m_pHoster->isChecked();
	}
	else
	{
		if(Checked && (m_bEd2k || m_bMagnet || m_bTorrent || m_bHoster))
			Checked = false;

		m_bEd2k = Checked;
		m_bMagnet = Checked;
		m_bTorrent = Checked;
		m_bHoster = Checked;
	}

	if(m_bEd2k)
	{
		g_ShellSetup.InstallProtocol("ed2k", m_OpenPath, false, "", "NeoLoader supported Protocol");
		g_ShellSetup.InstallType("emulecollection", m_OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");
	}
	else
	{
		g_ShellSetup.UninstallProtocol("ed2k");
		g_ShellSetup.UninstallType("emulecollection", false, "NeoLoader");
	}

	if(m_bMagnet)
		g_ShellSetup.InstallProtocol("magnet", m_OpenPath, false, "", "NeoLoader supported Protocol");
	else
		g_ShellSetup.UninstallProtocol("magnet");

	if(m_bTorrent)
		g_ShellSetup.InstallType("torrent", m_OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");
	else
		g_ShellSetup.UninstallType("torrent", false, "NeoLoader");

	if(m_bHoster)
	{
		g_ShellSetup.InstallProtocol("jd", m_OpenPath, false, "", "NeoLoader supported Protocol");
		g_ShellSetup.InstallProtocol("jdlist", m_OpenPath, false, "", "NeoLoader supported Protocol");
		g_ShellSetup.InstallProtocol("dlc", m_OpenPath, false, "", "NeoLoader supported Protocol");
		//g_ShellSetup.InstallProtocol("ccf", m_OpenPath, false, "", "NeoLoader supported Protocol");
		//g_ShellSetup.InstallProtocol("rsdf", m_OpenPath, false, "", "NeoLoader supported Protocol");
		//g_ShellSetup.InstallType("ccf", m_OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");
		//g_ShellSetup.InstallType("rsdf", m_OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");
		g_ShellSetup.InstallType("dlc", m_OpenPath, false, "", "NeoLoader supported File", "", "NeoLoader");
	}
	else
	{
		g_ShellSetup.UninstallProtocol("jd");
		g_ShellSetup.UninstallProtocol("jdlist");
		g_ShellSetup.UninstallProtocol("dlc");
		//g_ShellSetup.UninstallProtocol("ccf");
		//g_ShellSetup.UninstallProtocol("rsdf");
		//g_ShellSetup.UninstallType("ccf", false, "NeoLoader");
		//g_ShellSetup.UninstallType("rsdf", false, "NeoLoader");
		g_ShellSetup.UninstallType("dlc", false, "NeoLoader");
	}

	emit g_ShellSetup.Changed();
}

#endif
