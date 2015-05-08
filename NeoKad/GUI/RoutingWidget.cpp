#include "GlobalHeader.h"
#include "RoutingWidget.h"
#include "../NeoKad.h"
#include "../../Framework/Settings.h"
#include "../Kad/KadHeader.h"
#include "../Kad/Kademlia.h"
#include "../Kad/KadNode.h"
#include "../Kad/KadHandler.h"
#include "../Kad/RoutingZone.h"
#include "../Kad/RoutingBin.h"
#include "../Kad/RoutingFork.h"
#include "../Kad/RoutingRoot.h"
#include "../Kad/KadConfig.h"
#include "../Kad/FirewallHandler.h"
#include "../Networking/BandwidthControl/BandwidthLimit.h"


CRoutingWidget::CRoutingWidget(QWidget *parent)
: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(0);

	m_pCopyMenu = new QMenu(tr("&Copy Text"), this);

		m_pCopyCell = new QAction(tr("Copy &Cell"), this);
		connect(m_pCopyCell, SIGNAL(triggered()), this, SLOT(OnCopyCell()));
		m_pCopyMenu->addAction(m_pCopyCell);

		m_pCopyRow = new QAction(tr("Copy &Row"), this);
		connect(m_pCopyRow, SIGNAL(triggered()), this, SLOT(OnCopyRow()));
		m_pCopyMenu->addAction(m_pCopyRow);

		m_pCopyColumn = new QAction(tr("Copy Co&lumn"), this);
		connect(m_pCopyColumn, SIGNAL(triggered()), this, SLOT(OnCopyColumn()));
		m_pCopyMenu->addAction(m_pCopyColumn);

		m_pCopyPanel = new QAction(tr("Copy &Panel"), this);
		connect(m_pCopyPanel, SIGNAL(triggered()), this, SLOT(OnCopyPanel()));
		m_pCopyMenu->addAction(m_pCopyPanel);

	m_pRoutingTable = new QTreeWidget(NULL);
	m_pRoutingTable->setExpandsOnDoubleClick(false);
#ifdef WIN32
	m_pRoutingTable->setStyle(QStyleFactory::create("windowsxp"));
#endif
	m_pRoutingTable->setHeaderLabels(QString("ID|Type|Distance|Address|Crypto|Upload|Download|Software|Expire").split("|"));
	m_pRoutingTable->setSortingEnabled(true);
	connect(m_pRoutingTable, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));

	m_pRoutingTable->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pRoutingTable, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	m_pMainLayout->addWidget(m_pRoutingTable);

	setLayout(m_pMainLayout);

	m_pRoutingTable->header()->restoreState(theKad->Cfg()->GetBlob("Gui/RoutingTable"));
}

CRoutingWidget::~CRoutingWidget()
{
	theKad->Cfg()->SetBlob("Gui/RoutingTable",m_pRoutingTable->header()->saveState());
}

void CRoutingWidget::OnItemDoubleClicked(QTreeWidgetItem* pItem, int iColumn)
{
	QString ID = pItem->data(eID, Qt::UserRole).toString();

	CUInt128 uID;
	uID.FromHex(ID.toStdWString());

	CRoutingRoot* pRoot = theKad->Kad()->GetChild<CRoutingRoot>();
	CKadHandler* pHandler = theKad->Kad()->GetChild<CKadHandler>();
	if(CKadNode* pNode = pRoot->GetNode(uID))
		pHandler->CheckoutNode(pNode);
}

void CRoutingWidget::DumpRoutignTable(QTreeWidgetItem* pForkItem, CRoutingZone* pPrev)
{
	QMap<QString, QTreeWidgetItem*> RoutingMap;
	for(int i=0; i<pForkItem->childCount();i++)
	{
		QTreeWidgetItem* pItem = pForkItem->child(i);
		QString ID = pItem->data(eID, Qt::UserRole).toString();
		//ASSERT(!RoutingMap.contains(Address));
		RoutingMap.insert(ID,pItem);
	}

	CRoutingRoot* pRoot = theKad->Kad()->GetChild<CRoutingRoot>();
	CKadHandler* pHandler = theKad->Kad()->GetChild<CKadHandler>();

	if(CRoutingBin* pBin = pPrev->Cast<CRoutingBin>())
	{
		const NodeList& Nodes = pBin->GetNodes();

		for(NodeList::const_iterator I = Nodes.begin(); I != Nodes.end();I++)
		{
			CKadNode* pNode = *I;

			// ID|Type|Distance|Address
			QString ID = QString::fromStdWString(pNode->GetID().ToHex());
			//if(!pNode->GetID().GetKey())
			//	ID.prepend("~ ");
			QTreeWidgetItem* pItem = RoutingMap.take(ID);
			if(!pItem)
			{
				pItem = new QTreeWidgetItem();
				pItem->setData(eID, Qt::UserRole, ID);
				pItem->setText(eID, ID);
				pForkItem->addChild(pItem);
				pItem->setExpanded(true);
			}

			pItem->setText(eType, tr("Node Class %1%2").arg(pNode->GetClass()).arg(pNode->HasFailed() ? tr(" (failed)") : ""));

			CUInt128 uDistance = pNode->GetID() ^ pRoot->GetID();
			pItem->setText(eDistance, QString::fromStdWString(uDistance.ToBin()));
			pItem->setText(eAddress, ListAddr(pNode->GetAddresses()));

			QString CryptoStr;
			if (pHandler->ExchangePending(pNode))
				CryptoStr = "# ";
			if(CTrustedKey* pTrustedKey = pNode->GetTrustedKey())
			{
				if(!pTrustedKey->IsAuthenticated())
					CryptoStr += "~";
				CryptoStr += QString::number(pTrustedKey->GetFingerPrint(), 16);
			}
			pItem->setText(eCrypto, CryptoStr);

			time_t Expire = pNode->GetLastSeen();
			if(pPrev->IsDistantZone())
				Expire += theKad->Kad()->GetChild<CKadConfig>()->GetInt64("RetentionTime");
			else
				Expire += pNode->GetTimeToKeep();

			if(Expire > GetTime())
				Expire -= GetTime();
			else
				Expire = 0;

			QFont Font = pItem->font(eID);

			bool bConnected = pNode->GetUpLimit()->Count() != 0; // if one is set booth are set
			Font.setBold(bConnected);

			pItem->setFont(eID, Font);

			pItem->setText(eUpload, bConnected ? QString::number(double(pNode->GetUpLimit()->GetRate())/1024.0, 'f', 2) + "kB/s" : "");
			pItem->setText(eDownload, bConnected ? QString::number(double(pNode->GetDownLimit()->GetRate())/1024.0, 'f', 2) + "kB/s" : "");
			pItem->setText(eSoftware, QString::fromStdString(pNode->GetVersion()));
			pItem->setText(eExpire, QString("%1:%2").arg(Expire/60).arg(Expire%60));
		}
	}
	else if(CRoutingFork* pFork = pPrev->Cast<CRoutingFork>())
	{
		ZoneList Zones = pFork->GetZones();

		for(ZoneList::const_iterator I = Zones.begin(); I != Zones.end();I++)
		{
			CRoutingZone* pZone = *I;

			// ID|Type|Distance|Address
			//QByteArray ID = QString::fromStdWString(uZone.ToHex());
			QString ID = QString::fromStdWString(pZone->GetPrefix().ToHex());
			QTreeWidgetItem* pItem = RoutingMap.take(ID);
			if(!pItem)
			{
				pItem = new QTreeWidgetItem();
				pItem->setData(eID, Qt::UserRole, ID);
				pItem->setText(eID, ID);
				pForkItem->addChild(pItem);
				pItem->setExpanded(true);
			}

			pItem->setText(eType, tr("Zone Level %1").arg(pZone->GetLevel()));

			CUInt128 uDistance = pZone->GetPrefix();
			pItem->setText(eDistance, QString::fromStdWString(uDistance.ToBin()));

			bool bLooking = false;
			if(CRoutingBin* pBin = pZone->Cast<CRoutingBin>())
			{
				bLooking = pBin->IsLooking();
				if(bLooking)
					ID = QString::fromStdWString(pBin->GetRandomID().ToHex());

				uint64 Next = pBin->GetNextLooking();
				if(Next > GetCurTick())
					Next -= GetCurTick();
				else
					Next = 0;

				Next /= 1000;
				pItem->setText(eExpire, QString("%1:%2").arg(Next/60).arg(Next%60));
			}
			else
				pItem->setText(eExpire, "");

			QFont Font = pItem->font(eID);
			if(Font.bold() != bLooking)
			{
				Font.setBold(bLooking);
				pItem->setFont(eID, Font);
				pItem->setText(eID, ID);
			}

			if(pZone->IsDistantZone())
			{
				QBrush Brush (Qt::gray);
				pItem->setForeground(eID , Brush );
			}

			DumpRoutignTable(pItem, pZone);
		}
	}

	// whats left is to be deleted 
	foreach(QTreeWidgetItem* pItem, RoutingMap)
		delete pItem;
}

void CRoutingWidget::DumpRoutignTable()
{
	CRoutingRoot* pRoot = theKad->Kad()->GetChild<CRoutingRoot>();
	if(!pRoot)
		return;

	QTreeWidgetItem* pItem = NULL;
	if(m_pRoutingTable->topLevelItemCount() == 0)
	{
		QString ID = QString::fromStdWString(pRoot->GetID().ToHex());
		pItem = new QTreeWidgetItem();
		pItem->setData(eID, Qt::UserRole, ID);
		pItem->setText(eID, ID);
		m_pRoutingTable->addTopLevelItem(pItem);
		pItem->setExpanded(true);

		pItem->setText(eType, "Root");

		pItem->setText(eDistance, QString::fromStdWString(CUInt128(0).ToBin()));
	}
	else
	{
		ASSERT(m_pRoutingTable->topLevelItemCount() == 1);
		pItem = m_pRoutingTable->topLevelItem(0);
	}

	if(CFirewallHandler* pFirewallHandler = theKad->Kad()->GetChild<CFirewallHandler>())
		pItem->setText(eAddress, ListAddr(pFirewallHandler->AddrPool()));

	if(CSmartSocket* pSocket = theKad->Kad()->GetChild<CSmartSocket>())
	{
		pItem->setText(eUpload, QString::number(double(pSocket->GetUpLimit()->GetRate())/1024.0, 'f', 2) + "kB/s");
		pItem->setText(eDownload, QString::number(double(pSocket->GetDownLimit()->GetRate())/1024.0, 'f', 2) + "kB/s");
	}

	QFont Font = pItem->font(eID);
	if(Font.bold() != pRoot->IsLooking())
	{
		Font.setBold(pRoot->IsLooking());
		pItem->setFont(eID, Font);
	}

	uint64 Next = pRoot->GetNextLooking();
	if(Next > GetCurTick())
		Next -= GetCurTick();
	else
		Next = 0;

	Next /= 1000;
	pItem->setText(eExpire, QString("%1:%2").arg(Next/60).arg(Next%60));

	DumpRoutignTable(pItem, pRoot);
}

void CRoutingWidget::OnMenuRequested(const QPoint &point)
{
	m_pCopyMenu->popup(QCursor::pos());
}

void CRoutingWidget::OnCopyCell()
{
	QTreeWidget* pTree = NULL;
	if(m_pRoutingTable->hasFocus())
		pTree = m_pRoutingTable;

	if(pTree)
	{
		if(QTreeWidgetItem* pItem = pTree->currentItem())
		{
			QString Text = pItem->text(pTree->currentColumn());
			QApplication::clipboard()->setText(Text);
		}
	}
}

QString MergeRaw(QTreeWidget* pTree, QTreeWidgetItem* pItem)
{
	QString Text;
	for(int i=0; i < pTree->columnCount(); i++)
	{
		if(i > 0)
			Text.append("\t");
		Text.append(pItem->text(i));
	}
	return Text;
}

void CRoutingWidget::OnCopyRow()
{
	QTreeWidget* pTree = NULL;
	if(m_pRoutingTable->hasFocus())
		pTree = m_pRoutingTable;

	if(pTree)
	{
		if(QTreeWidgetItem* pItem = pTree->currentItem())
			QApplication::clipboard()->setText(MergeRaw(pTree, pItem));
	}
}

void CopyAll(QTreeWidget* pTree, int Column = -1)
{
	QString Text;
	for(int i=0; i < pTree->topLevelItemCount(); i++)
	{
		QList<QPair<QTreeWidgetItem*, int> > ItemStack;
		ItemStack.append(QPair<QTreeWidgetItem*, int> (pTree->topLevelItem(i), 0));
		while(!ItemStack.isEmpty())
		{
			QTreeWidgetItem* pItem = ItemStack.last().first;
			if(ItemStack.last().second == 0)
			{
				if(Column == -1)
					Text.append(MergeRaw(pTree, pItem));
				else
					Text.append(pItem->text(Column));
				Text.append("\r\n");
			}
			if(ItemStack.last().second < pItem->childCount())
				ItemStack.append(QPair<QTreeWidgetItem*, int> (pItem->child(ItemStack.last().second++), 0));
			else
				ItemStack.removeLast();
		}
	}
	QApplication::clipboard()->setText(Text);
}

void CRoutingWidget::OnCopyColumn()
{
	QTreeWidget* pTree = NULL;
	if(m_pRoutingTable->hasFocus())
		pTree = m_pRoutingTable;

	if(pTree)
		CopyAll(pTree, pTree->currentColumn());
}

void CRoutingWidget::OnCopyPanel()
{
	QTreeWidget* pTree = NULL;
	if(m_pRoutingTable->hasFocus())
		pTree = m_pRoutingTable;

	if(pTree)
		CopyAll(pTree);
}