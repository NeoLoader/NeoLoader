#include "GlobalHeader.h"
#include "LookupWidget.h"
#include "../NeoKad.h"
#include "../../Framework/Cryptography/AbstractKey.h"
#include "../../Framework/Settings.h"
#include "../../Framework/Xml.h"
#include "../Kad/KadHeader.h"
#include "../Kad/Kademlia.h"
#include "../Kad/LookupManager.h"
#include "../Kad/KadLookup.h"
#include "../Kad/KadTask.h"
#include "../Kad/KadRouting/KadRoute.h"
#include "../Kad/KadEngine/KadScript.h"
#include "../Kad/KadEngine/KadOperator.h"

CLookupWidget::CLookupWidget(QWidget *parent)
: QWidget(parent)
{
	m_pLookupBar = new QToolBar(tr("Manual"), this);

	m_pLookupStop = new QAction(tr("Stop"),m_pLookupBar);
	connect(m_pLookupStop, SIGNAL(triggered()), this, SLOT(OnStopLookup()));
	m_pLookupBar->addAction(m_pLookupStop);

	m_pLookupID = new QLineEdit(m_pLookupBar);
	m_pLookupID->setText("");
	m_pLookupID->setMaximumSize(250, QWIDGETSIZE_MAX);
	m_pLookupID->setMaxLength(32);
	m_pLookupBar->addWidget(m_pLookupID);

	m_pLookupStart = new QAction(tr("Start"),m_pLookupBar);
	connect(m_pLookupStart, SIGNAL(triggered()), this, SLOT(OnStartLookup()));
	m_pLookupBar->addAction(m_pLookupStart);

	m_pLookupBar->addSeparator();

	m_pLookupMore = new QAction(tr("More"),m_pLookupBar);
	connect(m_pLookupMore, SIGNAL(triggered()), this, SLOT(OnMoreLookup()));
	m_pLookupBar->addAction(m_pLookupMore);

	m_pLookupClear = new QAction(tr("Clear"),m_pLookupBar);
	connect(m_pLookupClear, SIGNAL(triggered()), this, SLOT(OnClearLookup()));
	m_pLookupBar->addAction(m_pLookupClear);

	m_pLookupBar->addSeparator();

	//m_pLookupAuxWidget = new QWidget();
	//m_pLookupAuxLayout = new QFormLayout(m_pLookupAuxWidget);

	m_pLookupJumps = new QLineEdit();
	m_pLookupJumps->setMaximumSize(50, QWIDGETSIZE_MAX);
	//m_pLookupAuxLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(tr("Jumps")));
	//m_pLookupAuxLayout->setWidget(0, QFormLayout::FieldRole, m_pLookupJumps);
	m_pLookupBar->addWidget(new QLabel(tr("  Jumps: ")));
	m_pLookupBar->addWidget(m_pLookupJumps);

	m_pLookupHops = new QLineEdit();
	m_pLookupHops->setMaximumSize(50, QWIDGETSIZE_MAX);
	//m_pLookupAuxLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(tr("Hops")));
	//m_pLookupAuxLayout->setWidget(1, QFormLayout::FieldRole, m_pLookupHops);
	m_pLookupBar->addWidget(new QLabel(tr("  Hops: ")));
	m_pLookupBar->addWidget(m_pLookupHops);

	m_pLookupSpread = new QLineEdit();
	m_pLookupSpread->setMaximumSize(50, QWIDGETSIZE_MAX);
	//m_pLookupAuxLayout->setWidget(3, QFormLayout::LabelRole, new QLabel(tr("Spread")));
	//m_pLookupAuxLayout->setWidget(3, QFormLayout::FieldRole, m_pLookupSpread);
	m_pLookupBar->addWidget(new QLabel(tr("  Spread: ")));
	m_pLookupBar->addWidget(m_pLookupSpread);

	m_pLookupJumps->setText("0");
	m_pLookupHops->setText("1");
	m_pLookupSpread->setText("1");

	m_pLookupBar->addSeparator();

	m_pLookupTab = new QTabWidget(NULL);
	//m_pLookupTab->setTabsClosable(true);

	m_pLookupLayout = new QVBoxLayout();
	m_pLookupLayout->setContentsMargins(0, 0, 0, 0);
	m_pLookupLayout->addWidget(m_pLookupBar);
	//m_pLookupLayout->addWidget(m_pLookupAuxWidget);
	m_pLookupLayout->addWidget(m_pLookupTab);
	setLayout(m_pLookupLayout);
}

void CLookupWidget::OnStopLookup()
{
	CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();

	CVariant vID;
	vID.FromQVariant(m_CurID);
	map<CVariant, CPointer<CKadLookup> >::iterator I = m_Lookups.find(vID);
	if(I == m_Lookups.end())
		return;

	pLookupManager->StopLookup(I->second);
	
	m_Lookups.erase(I);
}

class CKadRouteTest: public CKadRouteImpl
{
public:
	CKadRouteTest(const CUInt128& ID, CPrivateKey* pEntityKey = NULL, CObject* pParent = NULL)
		: CKadRouteImpl(ID, pEntityKey, pParent) {}

	virtual void		Process(UINT Tick)
	{
		CKadRouteImpl::Process(Tick);

		Refresh(); // We are the source we are always fresh
	}
};

void CLookupWidget::OnStartLookup()
{
	QString sID = m_pLookupID->text();

	CUInt128 ID;
	if(!ID.FromHex(sID.toStdWString()))
		return;

	CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();

	CRouteWidget* pRouteWidget = NULL;
	for(int i=0; i < m_pLookupTab->count(); i++)
	{
		if(pRouteWidget = qobject_cast<CRouteWidget*>(m_pLookupTab->widget(i)))
			break;
	}

	CPointer<CKadLookup> pLookup;
	if(pRouteWidget)
	{
		CKadRoute* pKadRoute = new CKadRouteTest(ID,NULL,pLookupManager);
		pRouteWidget->m_pEntityID->setText(QByteArray((char*)pKadRoute->GetEntityID().GetData(), pKadRoute->GetEntityID().GetSize()).toHex());
		pLookup = pKadRoute;

		if(!m_pLookupSpread->text().isEmpty())
			pKadRoute->SetBrancheCount(m_pLookupSpread->text().toUInt());
	}
	else
	{
		CKadTask* pKadLookup = new CKadTask(ID,pLookupManager);
		pKadLookup->SetName(L"*Test Task*");
		pLookup = pKadLookup;

		for(int i=0; i < m_pLookupTab->count(); i++)
		{
			if(CPaylaodWidget* pPaylaodWidget = qobject_cast<CPaylaodWidget*>(m_pLookupTab->widget(i)))
			{
				QString Op = Split2(m_pLookupTab->tabText(i)," ").first;

				CVariant Request;
				Request.FromQVariant(CXml::Parse(pPaylaodWidget->m_pRequest->toPlainText()));
				
				if(Op == "Execute")
				{
					CBuffer CodeID = QByteArray::fromHex(pPaylaodWidget->m_pValue->text().toLatin1());
					pKadLookup->SetupScript(CodeID);

					CVariant Request;
					Request.FromQVariant(CXml::Parse(pPaylaodWidget->m_pRequest->toPlainText()));

					CVariant XID = GetRand64() & MAX_FLOAT;
					pKadLookup->AddCall(Request["FX"],Request["ARG"], XID);
				}
				if(Op == "Store")
					pKadLookup->Store(pPaylaodWidget->m_pValue->text().toStdString(), Request);
				else if(Op == "Retrieve")
					pKadLookup->Load(pPaylaodWidget->m_pValue->text().toStdString());
			}
		}

		if(!m_pLookupSpread->text().isEmpty())
			pKadLookup->SetSpreadCount(m_pLookupSpread->text().toUInt());
	}

	if(!m_pLookupJumps->text().isEmpty())
	{
		pLookup->SetJumpCount(m_pLookupJumps->text().toUInt());
		pLookup->SetHopLimit(Max(10, pLookup->GetJumpCount() + 1));
	}
	if(!m_pLookupHops->text().isEmpty())
		pLookup->SetHopLimit(m_pLookupHops->text().toUInt());

	pLookup->EnableTrace();
	CPointer<CKadLookup> Lookup(pLookup);
	m_Lookups.insert(map<CVariant, CPointer<CKadLookup> >::value_type(pLookupManager->StartLookup(Lookup), pLookup));
}

void CLookupWidget::OnMoreLookup()
{
	QString Op = QInputDialog::getItem (this, tr("New Operation"), tr("Operation"), QString("Execute|Route|Store|Retrieve").split("|"), 0, false, NULL);
	if(Op.isEmpty())
		return;

	if(Op == "Route")
	{
		if(m_pLookupTab->count() == 0)
		{
			CRouteWidget* pRouteWidget = new CRouteWidget();
			pRouteWidget->m_pData->setPlainText("<?xml version=\"1.0\"?>\r\n<Variant Type=\"String\">Some Data</Variant>");
			m_pLookupTab->addTab(pRouteWidget, QString("Route"));
			QObject::connect(pRouteWidget->m_pButtonBox, SIGNAL(accepted()), this, SLOT(OnSendPacket()));
		}
	}
	else
	{
		for(int i=0; i < m_pLookupTab->count(); i++)
		{
			if(Op == "Execute")
			{
				QString CurOp = Split2(m_pLookupTab->tabText(i)," ").first;
				if(CurOp == "Execute")
					return;
			}
			if(qobject_cast<CRouteWidget*>(m_pLookupTab->widget(i)) != NULL)
				return;
		}

		CPaylaodWidget* pPaylaodWidget = new CPaylaodWidget();
		if(Op == "Execute")
		{
			pPaylaodWidget->m_pRequest->setPlainText("<?xml version=\"1.0\"?>\r\n<Variant Type=\"Map\">\r\n\t<FX Type=\"String\">echo</FX>\r\n\t<ARG Type=\"String\">Some Data</ARG>\r\n</Variant>");
			pPaylaodWidget->m_pLabel->setText(tr("CodeID"));
			pPaylaodWidget->m_pValue->setText("0000000000000000");
		}
		else
		{
			pPaylaodWidget->m_pRequest->setPlainText("<?xml version=\"1.0\"?>\r\n<Variant Type=\"String\">Some Data</Variant>");
			pPaylaodWidget->m_pLabel->setText(tr("PayloadID"));
			pPaylaodWidget->m_pValue->setText("TEMP");
		}
		m_pLookupTab->addTab(pPaylaodWidget, QString("%1 (%2)").arg(Op).arg(m_pLookupTab->count() + 1));
	}
}

void CLookupWidget::OnSendPacket()
{
	CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();
	if(!pLookupManager)
		return;

	CRouteWidget* pRouteWidget = NULL;
	for(int i=0; i < m_pLookupTab->count(); i++)
	{
		if(pRouteWidget = qobject_cast<CRouteWidget*>(m_pLookupTab->widget(i)))
			break;
	}
	
	CBuffer EntityIDbuf(QByteArray::fromHex(pRouteWidget->m_pEntityID->text().toLatin1()));
	CVariant EntityID = EntityIDbuf;

	if(CKadRoute* pKadRoute = pLookupManager->GetRelay(EntityID)->Cast<CKadRoute>())
	{
		CBuffer ReceiverIDbuf(QByteArray::fromHex(pRouteWidget->m_pReceiverID->text().toLatin1()));
		CVariant ReceiverID = ReceiverIDbuf;

		CVariant Data;
		Data.FromQVariant(CXml::Parse(pRouteWidget->m_pData->toPlainText()));

		QString sID = pRouteWidget->m_pTargetID->text();

		CUInt128 ID;
		if(!ID.FromHex(sID.toStdWString()))
			ID = pKadRoute->GetID();

		/*switch(pRouteWidget->m_pAsSession->checkState())
		{
			case Qt::Checked:
				pKadRoute->QueuePacket(ReceiverID, ID, "Test", Data);
				break;
			case Qt::PartiallyChecked:
				pKadRoute->QueuePacket(ReceiverID, ID, "Test", Data, false);
				break;
			case Qt::Unchecked:
				pKadRoute->SendRawFrame(ReceiverID, ID, Data);
				break;
		}*/
		pKadRoute->SendRawFrame(ReceiverID, ID, Data);
	}
}

void CLookupWidget::OnClearLookup()
{
	m_pLookupTab->clear();
}

void CLookupWidget::OnLoadLookup(QTreeWidgetItem* pItem, int column)
{
	m_pLookupTab->clear();

	uint32 ID = pItem->data(0, Qt::UserRole).toUInt();
	CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();
	CKadLookup* pLookup = pLookupManager->GetLookup(ID);;
	if(!pLookup)
	{
		map<CVariant, CPointer<CKadLookup> >::iterator I = m_Lookups.find(ID);
		if(I != m_Lookups.end())
			pLookup = I->second;
	}
	
	if(!pLookup)
		return;

	m_pLookupID->setText(QString::fromStdWString(pLookup->GetID().ToHex()));

	m_pLookupJumps->setText(QString::number(pLookup->GetJumpCount()));
	m_pLookupHops->setText(QString::number(pLookup->GetHopLimit()));
	
	if(CKadRelay* pKadRelay = pLookup->Cast<CKadRelay>())
	{
		m_pLookupSpread->setText(QString::number(pKadRelay->GetBrancheCount()));

		CRouteWidget* pRouteWidget = new CRouteWidget();
		m_pLookupTab->addTab(pRouteWidget, QString("Route"));
		QObject::connect(pRouteWidget->m_pButtonBox, SIGNAL(accepted()), this, SLOT(OnSendPacket()));
		pRouteWidget->m_pEntityID->setText(QByteArray((char*)pKadRelay->GetEntityID().GetData(), pKadRelay->GetEntityID().GetSize()).toHex());

		if(pItem->data(1, Qt::UserRole).isValid())
		{
			pRouteWidget->m_pReceiverID->setText(pItem->text(2));
			pRouteWidget->m_pData->setPlainText(CXml::Serialize(pItem->data(1, Qt::UserRole)));
		}
	}
	else if(CKadTask* pKadLookup = pLookup->Cast<CKadTask>())
	{
		m_pLookupSpread->setText(QString::number(pKadLookup->GetSpreadCount()));

		if(CKadOperator* pOperator = pKadLookup->GetOperator())
		{
			CPaylaodWidget* pPaylaodWidget = new CPaylaodWidget();
			QByteArray CodeID((char*)pOperator->GetScript()->GetCodeID().GetData(), pOperator->GetScript()->GetCodeID().GetSize());
			pPaylaodWidget->m_pLabel->setText("CodeID");
			pPaylaodWidget->m_pValue->setText(CodeID.toHex());

			CVariant Requests;
			const CKadOperator::TRequestMap& ResuestMap = pOperator->GetRequests();
			for(CKadOperator::TRequestMap::const_iterator I = ResuestMap.begin(); I != ResuestMap.end(); I++)
			{
				CKadRequest* pRequest = I->second.pRequest;

				CVariant Request;
				Request["FX"] = pRequest->GetName();
				Request["ARG"] = pRequest->GetArguments();
				Request["XID"] = pRequest->GetXID();
				//Request["LOAD"] = // K-ToDo-Now: abbort publishing if teh load gets to high
				Requests.Append(Request);
			}

			pPaylaodWidget->m_pRequest->setPlainText(CXml::Serialize(Requests.ToQVariant()));

			m_pLookupTab->addTab(pPaylaodWidget, QString("Execute (%1)").arg(m_pLookupTab->count() + 1));

			const TRetMap& Results = pKadLookup->GetResults();
			for(TRetMap::const_iterator I = Results.begin(); I != Results.end(); I++)
			{
				pPaylaodWidget->m_pResponse->append(QString::fromStdString(I->first) + CXml::Serialize(I->second.Return.ToQVariant()) + "\r\n\r\n");
			}
		}

		const CKadOperation::TStoreOpMap& Store = pKadLookup->GetStoreReq();
		for(CKadOperation::TStoreOpMap::const_iterator I = Store.begin(); I != Store.end(); I++)
		{
			CPaylaodWidget* pPaylaodWidget = new CPaylaodWidget();
			pPaylaodWidget->m_pLabel->setText("PayloadID");
			pPaylaodWidget->m_pValue->setText(QString::fromStdString(I->first));
			pPaylaodWidget->m_pRequest->setPlainText(CXml::Serialize(I->second.Payload.ToQVariant()));
			m_pLookupTab->addTab(pPaylaodWidget, QString("Store (%1)").arg(m_pLookupTab->count() + 1));
		}

		const CKadOperation::TLoadOpMap& Load = pKadLookup->GetLoadReq();
		const TLoadedMap Loaded = pKadLookup->GetLoadRes();
		for(CKadOperation::TLoadOpMap::const_iterator I = Load.begin(); I != Load.end(); I++)
		{
			CPaylaodWidget* pPaylaodWidget = new CPaylaodWidget();
			pPaylaodWidget->m_pLabel->setText("PayloadID");
			pPaylaodWidget->m_pValue->setText(QString::fromStdString(I->first));
			pPaylaodWidget->m_pRequest->setPlainText("");
			for(TLoadedMap::const_iterator J = Loaded.find(I->first); J != Loaded.end() && J->first == I->first; J++)
				pPaylaodWidget->m_pResponse->append(CXml::Serialize(J->second.Data.ToQVariant()) + "\r\n\r\n");
			m_pLookupTab->addTab(pPaylaodWidget, QString("Retrieve (%1)").arg(m_pLookupTab->count() + 1));
		}
	}
}