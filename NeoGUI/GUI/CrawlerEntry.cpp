#include "GlobalHeader.h"
#include "CrawlerEntry.h"
#include "../NeoGUI.h"
#include "PropertiesView.h"
#include "CoverView.h"

#ifdef CRAWLER

CCrawlerEntry::CCrawlerEntry(QWidget *parent)
:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);

	m_pCrawlerTabs = new QTabWidget();

	m_pDetailWidget = new QWidget();
	m_pDetailLayout = new QFormLayout();

	m_pFileName = new QLineEdit();
	m_pFileName->setReadOnly(true);
	m_pFileName->setMaximumWidth(400);
	m_pDetailLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(tr("FileName:")));
	m_pDetailLayout->setWidget(0, QFormLayout::FieldRole, m_pFileName);

	m_pHashes = new QTableWidget();
	m_pHashes->setMaximumWidth(400);
	m_pHashes->horizontalHeader()->hide();
	m_pHashes->setSelectionMode(QAbstractItemView::NoSelection);
	//m_pHashes->setEditTriggers(QTableWidget::NoEditTriggers);
	m_pDetailLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(tr("Hashes:")));
	m_pDetailLayout->setWidget(1, QFormLayout::FieldRole, m_pHashes);

	m_pDescription = new QTextEdit();
	m_pDescription->setMaximumWidth(400);
	m_pDetailLayout->setWidget(2, QFormLayout::LabelRole, new QLabel(tr("Description:")));
	m_pDetailLayout->setWidget(2, QFormLayout::FieldRole, m_pDescription);

	m_pDetailWidget->setLayout(m_pDetailLayout);

	m_pSummary = new QWidget();
	m_pSummaryLayout = new QHBoxLayout();

	m_pSummaryLayout->addWidget(m_pDetailWidget);

	m_pCoverView = new CCoverView();
	m_pSummaryLayout->addWidget(m_pCoverView);

	m_pSummary->setLayout(m_pSummaryLayout);

	m_pTransferTree = new QTreeWidget();
	m_pTransferTree->setHeaderLabels(tr("URL").split("|"));

	m_pCrawlerTabs->addTab(m_pSummary, tr("Summary"));
	m_pCrawlerTabs->addTab(m_pTransferTree, tr("Links"));
	
	if(theGUI->Cfg()->GetInt("Gui/AdvancedControls") == 1)
	{
		m_pProperties = new CPropertiesView();
		m_pCrawlerTabs->addTab(m_pProperties, tr("Properties"));
	}
	else
		m_pProperties = NULL;

	m_pMainLayout->addWidget(m_pCrawlerTabs);

	setLayout(m_pMainLayout);
}

void CCrawlerEntry::ShowEntry(const QString& FileName, const QMap<QString, QString>& HasheMap, const QString& Description, const QString& Cover, const QStringList& Links, const QVariantMap& Details)
{
	m_pFileName->setText(FileName);

	m_pHashes->setColumnCount(1);
#if QT_VERSION < 0x050000
    m_pHashes->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
#else
    m_pHashes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
#endif
	m_pHashes->setRowCount(HasheMap.count());
	QStringList Hashes;
	foreach(const QString& Key, HasheMap.uniqueKeys())
	{
		QTableWidgetItem* pItem = new QTableWidgetItem(HasheMap[Key]);
		m_pHashes->setItem(Hashes.count(), 0, pItem);
		m_pHashes->resizeRowToContents(Hashes.count());
		Hashes.append(Key);
	}
	m_pHashes->setVerticalHeaderLabels(Hashes);
			
	if(int Count = m_pHashes->rowCount())
		m_pHashes->setMaximumHeight((m_pHashes->rowHeight(0) * Count) + 2);
	else
		m_pHashes->setMaximumHeight(30);

	m_pDescription->setText(Description);

	m_pCoverView->ShowCover(0, Cover);

	QList<QTreeWidgetItem*> NewItems;
	foreach(const QString& Link, Links)
	{
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setText(0, Link);
		NewItems.append(pItem);
	}
	m_pTransferTree->clear();
	m_pTransferTree->addTopLevelItems(NewItems);

	if(m_pProperties)
		m_pProperties->ShowReadOnly(Details);
}

#endif