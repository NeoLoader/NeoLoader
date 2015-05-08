#include "GlobalHeader.h"
#include "IndexWidget.h"
#include "../NeoKad.h"
#include "../../Framework/Cryptography/AbstractKey.h"
#include "../../Framework/Settings.h"
#include "../../Framework/Xml.h"
#include "../Kad/KadHeader.h"
#include "../Kad/Kademlia.h"
#include "../Kad/PayloadIndex.h"

CIndexWidget::CIndexWidget(QWidget *parent)
: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(0);

	m_pToolBar = new QToolBar();

	m_pToolBar->addWidget(new QLabel(tr("Entries:")));
	m_pLimit = new QSpinBox();
	m_pLimit->setMaximum(10000);
	m_pLimit->setMinimum(10);
	m_pLimit->setValue(100);
	connect(m_pLimit, SIGNAL(valueChanged(int)), this, SLOT(OnValueChanged(int)));
	m_pToolBar->addWidget(m_pLimit);
	m_pToolBar->addWidget(new QLabel(tr(" @ ")));

	m_pPage = new QSpinBox();
	m_pPage->setMinimum(1);
	connect(m_pPage, SIGNAL(valueChanged(int)), this, SLOT(OnValueChanged(int)));
	m_pToolBar->addWidget(m_pPage);

	m_pPages = new QLabel(tr(" / 0"));
	m_pToolBar->addWidget(m_pPages);

	m_pToolBar->addSeparator();

	m_pToolBar->addWidget(new QLabel(tr("Filter:")));
	m_pFilter = new QLineEdit();
	m_pFilter->setMaximumWidth(300);
	connect(m_pFilter, SIGNAL(returnPressed()), this, SLOT(OnReturnPressed()));
	m_pToolBar->addWidget(m_pFilter);

	m_pMainLayout->addWidget(m_pToolBar);

	m_pIndexSplitter = new QSplitter(NULL);
	m_pIndexSplitter->setOrientation(Qt::Vertical);

	m_pIndexTree = new QTreeWidget(NULL);
	m_pIndexTree->setExpandsOnDoubleClick(false);
	m_pIndexTree->setHeaderLabels(QString("ID|Expire|Control").split("|"));
	m_pIndexTree->setSortingEnabled(true);
	connect(m_pIndexTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnPayload(QTreeWidgetItem*, int)));
	m_pIndexSplitter->addWidget(m_pIndexTree);

	m_pIndexEdit = new QTextEdit(NULL);
	m_pIndexSplitter->addWidget(m_pIndexEdit);

	m_pMainLayout->addWidget(m_pIndexSplitter);
	setLayout(m_pMainLayout);

	m_pIndexTree->header()->restoreState(theKad->Cfg()->GetBlob("Gui/IndexList"));
}

CIndexWidget::~CIndexWidget()
{
	theKad->Cfg()->SetBlob("Gui/IndexList",m_pIndexTree->header()->saveState());
}

void CIndexWidget::DumpLocalIndex()
{
	CPayloadIndex* pIndex = theKad->Kad()->GetChild<CPayloadIndex>();
	if(!pIndex)
		return;

	int Count = pIndex->CountEntries(m_pFilter->text().toStdString());
	m_pPages->setText(tr(" / %1").arg(Count / m_pLimit->value() + 1));

	QMap<CVariant, QTreeWidgetItem*> IDIndex;
	for(int i=0; i<m_pIndexTree->topLevelItemCount();i++)
	{
		QTreeWidgetItem* pItem = m_pIndexTree->topLevelItem(i);
		CVariant ID;
		ID.FromQVariant(pItem->data(0, Qt::UserRole));
		ASSERT(!IDIndex.contains(ID));
		IDIndex.insert(ID,pItem);
	}

	multimap<CUInt128, SKadEntryInfoEx> Entries;
	pIndex->DumpEntries(Entries, m_pFilter->text().toStdString(), (m_pPage->value() - 1) * m_pLimit->value(), m_pLimit->value());

	for(multimap<CUInt128, SKadEntryInfoEx>::iterator I = Entries.begin(); I != Entries.end();)
	{
		//ID|Path|Expire|Control
		CVariant ID(I->first.GetData(), I->first.GetSize());
		QTreeWidgetItem* pItem = IDIndex.take(ID);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, ID.ToQVariant());
			pItem->setText(0, QString::fromStdWString(I->first.ToHex()));
			m_pIndexTree->addTopLevelItem(pItem);
			pItem->setExpanded(true);
		}

		QMap<uint64, QTreeWidgetItem*> NumIndex;
		for(int i=0; i<pItem->childCount();i++)
		{
			QTreeWidgetItem* pSubItem = pItem->child(i);
			uint64 Index = pSubItem->data(0, Qt::UserRole).toInt();
			ASSERT(!NumIndex.contains(Index));
			NumIndex.insert(Index,pSubItem);
		}

		CUInt128 CurID = I->first;
		for(;I != Entries.end() && CurID == I->first; I++)
		{
			SKadEntryInfoEx& Info = I->second;

			QTreeWidgetItem* pSubItem = NumIndex.take(Info.Index);
			if(!pSubItem)
			{
				pSubItem = new QTreeWidgetItem();
				pSubItem->setData(0, Qt::UserRole, Info.Index);
				pSubItem->setText(0, QString::fromStdString(Info.Path));
				pItem->addChild(pSubItem);
				pSubItem->setExpanded(true);
			}

			pSubItem->setText(1, QDateTime::fromTime_t(Info.Expire).toLocalTime().toString());

			if(Info.ExclusiveCID.IsValid())
			{
				pSubItem->setText(2, "Exclusive: " + QString::fromStdWString(ToHex(Info.ExclusiveCID.GetData(), Info.ExclusiveCID.GetSize())));
				pSubItem->setData(2, Qt::UserRole, Info.ExclusiveCID.ToQVariant());
			}
			else if(Info.pAccessKey)
				pSubItem->setText(2, "Protected");
		}

		// whats left is to be deleted 
		foreach(QTreeWidgetItem* pValItem, NumIndex)
			delete pValItem;
	}

	// whats left is to be deleted 
	foreach(QTreeWidgetItem* pItem, IDIndex)
		delete pItem;
}

void CIndexWidget::OnPayload(QTreeWidgetItem* pItem, int column)
{
	CPayloadIndex* pIndex = theKad->Kad()->GetChild<CPayloadIndex>();
	if(!pIndex)
		return;

	QTreeWidgetItem* pParent = pItem->parent();
	if(!pParent)
		return;

	uint64 Index = pItem->data(0, Qt::UserRole).toULongLong();

	CVariant ExclusiveCID;
	if(pItem->data(2, Qt::UserRole).isValid())
		ExclusiveCID.FromQVariant(pItem->data(2, Qt::UserRole));

	QVariant Payload = pIndex->Load(Index, ExclusiveCID).ToQVariant();
	m_pIndexEdit->setPlainText(CXml::Serialize(Payload));
}