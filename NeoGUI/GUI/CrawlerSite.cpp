#include "GlobalHeader.h"
#include "CrawlerSite.h"
#include "SettingsView.h"
#include "../NeoGUI.h"
#include "PropertiesView.h"
#include "LogView.h"

#ifdef CRAWLER

CCrawlerSite::CCrawlerSite(QWidget *parent)
:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);

	m_pCrawlerTabs = new QTabWidget();

	QMultiMap<int, SField> Settings;
	SetupSite(Settings);
	m_pSettings = new CSettingsView(Settings);
	m_pCrawlerTabs->addTab(m_pSettings, tr("Settings"));

	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
	{
		m_pProperties = new CPropertiesView();
		m_pCrawlerTabs->addTab(m_pProperties, tr("Properties"));
	}
	else
		m_pProperties = NULL;

	if(theGUI->Cfg()->GetBool("Gui/ShowLog"))
	{
		m_pLogView = new CLogView();
		m_pCrawlerTabs->addTab(m_pLogView, tr("Log"));
	}
	else
		m_pLogView = NULL;

	m_pMainLayout->addWidget(m_pCrawlerTabs);

	m_pButtons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Reset, Qt::Horizontal, this);
	QObject::connect(m_pButtons, SIGNAL(clicked(QAbstractButton *)), this, SLOT(OnClicked(QAbstractButton*)));
	m_pMainLayout->addWidget(m_pButtons);

	setLayout(m_pMainLayout);
}

void CCrawlerSite::ShowSite(const QString& SiteName)
{
	m_SiteName = SiteName;
	LoadAll();
}

void CCrawlerSite::OnClicked(QAbstractButton* pButton)
{
	switch(m_pButtons->buttonRole(pButton))
	{
	case QDialogButtonBox::ApplyRole:
		SaveAll();
	case QDialogButtonBox::ResetRole: // reset after apply to check if all values ware accepted properly
		LoadAll();
		break;
	}
}

void CCrawlerSite::LoadAll()
{
	if(m_pProperties)
		m_pProperties->ShowSite(m_SiteName);

	if(m_pLogView)
		m_pLogView->ShowSite(m_SiteName);

	m_pSettings->ShowSite(m_SiteName);
}

void CCrawlerSite::SaveAll()
{
	if(m_pProperties && m_pProperties == m_pCrawlerTabs->currentWidget())
	{
		m_pProperties->ApplySettings();
		return;
	}

	m_pSettings->ApplySettings();
}

void CCrawlerSite::SetupSite(QMultiMap<int, SField>& Settings)
{
	int Counter = 0;

	Settings.insert(Counter, SField(new QLabel(tr("Site Crawling:")), QFormLayout::SpanningRole));

	Counter++;
	QLineEdit* pSiteEntry = new QLineEdit();
	pSiteEntry->setMaximumWidth(400);
	Settings.insert(Counter, SField(new QLabel(tr("Auto Crawling Url")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pSiteEntry, QFormLayout::FieldRole, "SiteEntry"));

	Counter++;
	QSpinBox* pCrawlInterval = new CSpinBoxEx(HR2S(1));
	pCrawlInterval->setMinimumWidth(100);
	pCrawlInterval->setMaximumWidth(100);
	pCrawlInterval->setMaximum(0x7FFFFFFF);
	Settings.insert(Counter, SField(new QLabel(tr("Auto Crawling Interval (h)")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pCrawlInterval, QFormLayout::FieldRole, "IndexInterval"));

	Counter++;
	QComboBox* pCrawlMode = new QComboBox();
	pCrawlMode->setMaximumWidth(100);
	pCrawlMode->addItem(tr("Incremental"), ""); // checks the first site and subsequential sites for new entries crawls only the new ones
	pCrawlMode->addItem(tr("Complete"), "Full"); // checks the first site and subsequential sites and recrawl all found entries even if thay already have been crawled
	Settings.insert(Counter, SField(new QLabel(tr("Auto Crawling Mode")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pCrawlMode, QFormLayout::FieldRole, "CrawlingMode"));

	Counter++;
	QSpinBox* pCrawlDepth = new QSpinBox();
	pCrawlDepth->setMaximumWidth(50);
	Settings.insert(Counter, SField(new QLabel(tr("Auto Crawling Depth")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pCrawlDepth, QFormLayout::FieldRole, "CrawlingDepth"));

	Counter++;
	QSpinBox* pCrawlTasks = new QSpinBox();
	pCrawlTasks->setMaximumWidth(50);
	Settings.insert(Counter, SField(new QLabel(tr("Max Crawling Tasks")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pCrawlTasks, QFormLayout::FieldRole, "CrawlingTasks"));

	Counter = 100;
	Settings.insert(Counter, SField(new QLabel(tr("Kad Publishing:")), QFormLayout::SpanningRole));

	Counter++;
	QSpinBox* pMaxPublishments = new QSpinBox();
	pMaxPublishments->setMaximumWidth(50);
	Settings.insert(Counter, SField(new QLabel(tr("Max Publishments")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pMaxPublishments, QFormLayout::FieldRole, "MaxPublishments"));

	Counter++;
	QSpinBox* pExpirationTime = new CSpinBoxEx(DAY2S(1));
	pExpirationTime->setMinimumWidth(100);
	pExpirationTime->setMaximumWidth(100);
	pExpirationTime->setMaximum(0x7FFFFFFF);
	Settings.insert(Counter, SField(new QLabel(tr("Expiration Time (d)")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pExpirationTime, QFormLayout::FieldRole, "ExpirationTime"));

	Counter++;
	QSpinBox* pSpreadCount = new QSpinBox();
	pSpreadCount->setMaximumWidth(50);
	Settings.insert(Counter, SField(new QLabel(tr("Spread Count")), QFormLayout::LabelRole));
	Settings.insert(Counter, SField(pSpreadCount, QFormLayout::FieldRole, "SpreadCount"));
}

#endif