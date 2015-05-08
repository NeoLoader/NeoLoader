#include "GlobalHeader.h"
#include "SearchList.h"
#include "../NeoKad.h"
#include "../../Framework/Settings.h"
#include "../Kad/KadHeader.h"
#include "../Kad/Kademlia.h"
#include "../Kad/LookupManager.h"
#include "../Kad/KadLookup.h"
#include "../Kad/KadTask.h"
#include "../Kad/KadRouting/KadRoute.h"
#include "../Kad/KadRouting/FrameRelay.h"
#include "../Kad/KadRouting/KadRelay.h"
#include "../Kad/KadEngine/KadScript.h"
#include "../Kad/KadEngine/KadOperator.h"
#include "../Kad/KadEngine/KadEngine.h"

CSearchList::CSearchList(QWidget *parent)
: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(0);

	m_Headers[eLookupLine]	= QString("Number	|Key|Type		|Name		|Status	|Routes			|Load		|Hops/Jumps								|Shares			|Results				|Nodes						|Upload	|Download").split("|");
	m_Headers[eLinksLine]	= QString("Number	|	|			|			|		|Routes			|			|Queued Frames							|Relayed Frames	|Dropped Frames			|Test						|		|		").split("|");
	m_Headers[eLinkLine]	= QString("Number	|Key|			|			|		|				|			|Queued Frames							|Relayed Frames	|Dropped/Lost Frames	|RTT (Div), Timeout, Window	|Upload	|Download").split("|");
	m_Headers[eTableLine]	= QString("Number	|Key|			|			|		|				|			|Queued Frames							|Relayed Frames	|Dropped/Lost Frames	|Test						|		|		").split("|");
	m_Headers[eListLine]	= QString("Number	|Key|			|			|		|				|			|										|				|						|Test						|		|		").split("|");
	m_Headers[eConLine]		= QString("Number	|Key|			|Entity		|Status	|Index In/Out	|Processed	|Pending/Queued Frames, Queued Packets	|Relayed Frames	|Dropped/Lost Frames	|RTT (Div), Timeout, Window	|Upload	|Download").split("|");
	m_Headers[eTraceLine]	= QString("Direction|FID|RID/EID	|			|		|				|			|										|				|						|Test						|		|		").split("|");

	m_pLookupList = new QTreeWidget(NULL);
	m_pLookupList->setExpandsOnDoubleClick(false);
	m_pLookupList->setHeaderLabels(m_Headers[eLookupLine]);
	m_pLookupList->setSortingEnabled(true);
	connect(m_pLookupList, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemClicked(QTreeWidgetItem*, int)));
	connect(m_pLookupList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)));

	m_pMainLayout->addWidget(m_pLookupList);
	setLayout(m_pMainLayout);

	m_pLookupList->header()->restoreState(theKad->Cfg()->GetBlob("Gui/LookupList"));
}

CSearchList::~CSearchList()
{
	theKad->Cfg()->SetBlob("Gui/LookupList",m_pLookupList->header()->saveState());

	foreach(SLookup* pInfo, m_Lookups)
		delete pInfo;
}

QVariant CSearchList::GetCurID()
{
	QTreeWidgetItem* pCurItem = m_pLookupList->currentItem();
	return pCurItem ? pCurItem->data(eNumber, Qt::UserRole) : QVariant();
}

int CSearchList::DumpLinks(CFrameRelay* pLink, QTreeWidgetItem* pItem)
{
	ASSERT(pItem);
	const CFrameRelay::TRelayMap& Relays = pLink->GetNodes();

	pItem->setText(ePendingFrames, tr("%1").arg(pLink->GetStats().PendingFrames));
	pItem->setText(eRelayedFrames, tr("%1/%2").arg(pLink->GetStats().RelayedFrames).arg(pLink->GetStats().DeliveredFrames));
	pItem->setText(eDroppedFrames, tr("%1").arg(pLink->GetStats().DroppedFrames));

	QMap<QString, QTreeWidgetItem*> NodeList;
	for(int i=0; i < pItem->childCount();i++)
	{
		QTreeWidgetItem* pSubItem = pItem->child(i);
		QString NodeID = pSubItem->data(eNumber, Qt::UserRole).toString();
		ASSERT(!NodeList.contains(NodeID));
		NodeList.insert(NodeID,pSubItem);
	}

	for(CFrameRelay::TRelayMap::const_iterator I = Relays.begin(); I != Relays.end(); I++)
	{
		CKadNode* pNode = I->first.pNode;
		const CRelayStats* pRelay = I->second;
		QString NodeID = QString::fromStdWString(pNode->GetID().ToHex());
		QTreeWidgetItem* pSubItem = NodeList.take(NodeID);
		if(!pSubItem)
		{
			pSubItem = new QTreeWidgetItem();
			pSubItem->setData(eNumber, Qt::UserRole, NodeID);
			pSubItem->setData(eKey, Qt::UserRole, eLinkLine);
			pSubItem->setText(eNumber, tr("Node"));
			pSubItem->setText(eKey, NodeID);
			pItem->addChild(pSubItem);
			pSubItem->setExpanded(true);
		}	

		pSubItem->setText(ePendingFrames, tr("%1").arg(pRelay->GetStats().PendingFrames));
		pSubItem->setText(eRelayedFrames, tr("%1/%2").arg(pRelay->GetStats().RelayedFrames).arg(pRelay->GetStats().DeliveredFrames));
		pSubItem->setText(eDroppedFrames, tr("%1/%2").arg(pRelay->GetStats().DroppedFrames).arg(pRelay->GetStats().LostFrames));
		pSubItem->setText(eFramesStats, tr("%1 (%2), %3, %4").arg(pRelay->GetStats().GetEstimatedRTT()).arg(pRelay->GetStats().GetRTTDeviation()).arg(pRelay->GetTimeOut()).arg(pRelay->GetWindowSize()));

		pSubItem->setText(eUpload, QString::number(double(pNode->GetUpLimit()->GetRate())/1024.0, 'f', 2) + "kB/s");
		pSubItem->setText(eDownload, QString::number(double(pNode->GetDownLimit()->GetRate())/1024.0, 'f', 2) + "kB/s");

		QMap<QString, QTreeWidgetItem*> RouteMap;
		for(int i=0; i < pSubItem->childCount();i++)
		{
			QTreeWidgetItem* pTmpItem = pSubItem->child(i);
			QString RID = pTmpItem->data(eNumber, Qt::UserRole).toString();
			ASSERT(!RouteMap.contains(RID));
			RouteMap.insert(RID,pTmpItem);
		}

		const CRelayStats::TRoutingMap &Routing = pRelay->GetRouting();

		for(CRelayStats::TRoutingMap::const_iterator I = Routing.begin(); I != Routing.end(); I++)
		{
			const SRoutingStat &Stat = I->second;
			QString RID = QString::fromStdWString(ToHex(I->first.GetData(), I->first.GetSize()));
			QTreeWidgetItem* pTmpItem = RouteMap.take(RID);
			if(!pTmpItem)
			{
				pTmpItem = new QTreeWidgetItem();
				pTmpItem->setData(eNumber, Qt::UserRole, RID);
				pTmpItem->setData(eKey, Qt::UserRole, eTableLine);
				pTmpItem->setText(eNumber, tr("Entry"));
				pTmpItem->setText(eKey, RID);
				pSubItem->addChild(pTmpItem);
				//pTmpItem->setExpanded(true);
			}	

			pTmpItem->setText(eRelayedFrames, tr("%1 (%2)").arg(Stat.RelayedFrames).arg(Stat.RecentlyDelivered.GetSumm()));
			pTmpItem->setText(eDroppedFrames, tr("%1 (%2)").arg(Stat.DroppedFrames).arg(Stat.RecentlyLost.GetSumm()));
			pTmpItem->setText(eFramesStats, tr("%1 (%2)").arg(Stat.GetEstimatedRTT()).arg(Stat.GetRTTDeviation()));
		}

		foreach(QTreeWidgetItem* pTmpItem, RouteMap)
			delete pTmpItem;
	}

	foreach(QTreeWidgetItem* pSubItem, NodeList)
		delete pSubItem;

	return (int)Relays.size();
}

int CSearchList::DumpSessions(CKadRoute* pRoute, QTreeWidgetItem* pItem)
{
	ASSERT(pItem);
	CKadRoute::SessionMap& Sessions = pRoute->GetSessions();

	QMap<QString, QTreeWidgetItem*> SessionList;
	for(int i=0; i < pItem->childCount();i++)
	{
		QTreeWidgetItem* pSubItem = pItem->child(i);
		QString XID = pSubItem->data(eNumber, Qt::UserRole).toString();
		ASSERT(!SessionList.contains(XID));
		SessionList.insert(XID,pSubItem);
	}

	for(CKadRoute::SessionMap::const_iterator I = Sessions.begin(); I != Sessions.end(); I++)
	{
		CRouteSession* pSession = I->second;

		QString XID = QString::fromStdWString(ToHex(I->first.GetData(), I->first.GetSize())) + "/" + QString::fromStdWString(ToHex(pSession->GetSessionID().GetData(), pSession->GetSessionID().GetSize()));
		QTreeWidgetItem* pSubItem = SessionList.take(XID);
		if(!pSubItem)
		{
			pSubItem = new QTreeWidgetItem();
			pSubItem->setData(eNumber, Qt::UserRole, XID);
			pSubItem->setText(eNumber, tr("Session"));
			pSubItem->setData(eKey, Qt::UserRole, eConLine);
			pSubItem->setText(eKey, XID);
			pItem->addChild(pSubItem);
			//pSubItem->setExpanded(true);
		}	

		pSubItem->setText(eStreamStats, tr("%1 (%2), %3").arg(pSession->GetRecvOffset()).arg(pSession->GetIncompleteCount()).arg(pSession->GetSendOffset()));
		pSubItem->setText(eTotalFrames, tr("%1").arg(pSession->GetStats().ProcessedFrames));
		pSubItem->setText(ePendingFrames, tr("%1/%2").arg(pSession->GetStats().PendingFrames).arg(pSession->GetQueuedFrameCount()));
		pSubItem->setText(eRelayedFrames, tr("%1").arg(pSession->GetStats().RelayedFrames));
		pSubItem->setText(eDroppedFrames, tr("%1").arg(pSession->GetStats().DroppedFrames));
		pSubItem->setText(eFramesStats, tr("%1 (%2), %3, %4").arg(pSession->GetStats().GetEstimatedRTT()).arg(pSession->GetStats().GetRTTDeviation()).arg(pSession->GetTimeOut()).arg(pSession->GetWindowSize()));
	}

	foreach(QTreeWidgetItem* pSubItem, SessionList)
		delete pSubItem;

	return (int)Sessions.size();
}

void CSearchList::DumpTrace(CKadRoute* pRoute, QTreeWidgetItem* pItem)
{
	ASSERT(pItem);
	QMap<uint64, QTreeWidgetItem*> FrameList;
	for(int i=0; i<pItem->childCount();i++)
	{
		QTreeWidgetItem* pSubItem = pItem->child(i);
		uint64 AbsID = pSubItem->data(eNumber, Qt::UserRole).toULongLong();
		ASSERT(!FrameList.contains(AbsID));
		FrameList.insert(AbsID,pSubItem);
	}

	int Index = 0;
	for(FrameTraceList::const_iterator I = pRoute->GetFrameHistory().begin(); I != pRoute->GetFrameHistory().end();I++)
	{
		const SFrameTrace &FrameTrace = *I;

		uint64 AbsID = I->AbsID;
		QTreeWidgetItem* pSubItem = FrameList.take(AbsID);
		if(!pSubItem)
		{
			pSubItem = new QTreeWidgetItem();
			pSubItem->setData(eNumber, Qt::UserRole, AbsID);
			pSubItem->setData(eKey, Qt::UserRole, eTraceLine);
			pItem->addChild(pSubItem);
			//pSubItem->setExpanded(true);
		}

		Index++;
		if(FrameTrace.AckTime == -1)
			pSubItem->setText(eDirection, tr("%1 <---").arg(Index));
		else if(FrameTrace.AckTime != 0)
			pSubItem->setText(eDirection, tr("%1 --->").arg(Index));
		else
			pSubItem->setText(eDirection, tr("%1 ...>").arg(Index));

		pSubItem->setText(eFID, "FID:" + QByteArray((char*)I->Frame["FID"].GetData(), I->Frame["FID"].GetSize()).toHex());

		if(FrameTrace.AckTime == -1)
			pSubItem->setText(eXID, "EID:" + QByteArray((char*)I->Frame["EID"].GetData(), I->Frame["EID"].GetSize()).toHex());
		else
			pSubItem->setText(eXID, "RID:" + QByteArray((char*)I->Frame["RID"].GetData(), I->Frame["RID"].GetSize()).toHex());
	}

	foreach(QTreeWidgetItem* pSubItem, FrameList)
		delete pSubItem;
}

void CSearchList::DumpSearchList(map<CVariant, CPointer<CKadLookup> >& AuxMap)
{
	CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();
	if(!pLookupManager)
		return;

	CKadEngine* pEngine = theKad->Kad()->GetChild<CKadEngine>();

	QMap<CVariant, SLookup*>	OldLookups = m_Lookups;

	LookupMap Lookups = pLookupManager->GetLookups();

	for(map<CVariant, CPointer<CKadLookup> >::iterator I = AuxMap.begin(); I != AuxMap.end(); I++)
		Lookups.insert(LookupMap::value_type(I->first, I->second));

	for(LookupMap::const_iterator I = Lookups.begin(); I != Lookups.end();I++)
	{
		CKadLookup* pLookup = I->second;

		CVariant ID = pLookup->GetLookupID();
		SLookup* pInfo = OldLookups.take(ID);
		if(!pInfo)
		{
			pInfo = new SLookup();
			m_Lookups.insert(ID, pInfo);
			pInfo->pItem = new QTreeWidgetItem();
			pInfo->pItem->setData(eNumber, Qt::UserRole, ID.ToQVariant());
			pInfo->pItem->setText(eNumber, QString::fromStdWString(ToHex(ID.GetData(), ID.GetSize())));
			pInfo->pItem->setData(eKey, Qt::UserRole, eLookupLine);
			pInfo->pItem->setText(eKey, QString::fromStdWString(pLookup->GetID().ToHex()));
			m_pLookupList->addTopLevelItem(pInfo->pItem);
			pInfo->pItem->setExpanded(true);

			if(CKadRelay* pRelay = pLookup->Cast<CKadRelay>())
			{
				QTreeWidgetItem* &pUp = pInfo->Sub[eUpLinks];
				pUp = new QTreeWidgetItem();
				pUp->setText(eNumber, tr("UpLinks"));
				pUp->setData(eKey, Qt::UserRole, eLinksLine);
				pInfo->pItem->addChild(pUp);
				pUp->setExpanded(true);

				if(!pRelay->Inherits("CKadBridge") && !pRelay->Inherits("CKadRoute"))
				{
					QTreeWidgetItem* &pDown = pInfo->Sub[eDownLinks];
					pDown = new QTreeWidgetItem();
					pDown->setText(eNumber, tr("DownLinks"));
					pDown->setData(eKey, Qt::UserRole, eLinksLine);
					pInfo->pItem->addChild(pDown);
					pDown->setExpanded(true);
				}
			}
			if(CKadRoute* pRoute = pLookup->Cast<CKadRoute>())
			{
				QTreeWidgetItem* &pList = pInfo->Sub[eSessions];
				pList = new QTreeWidgetItem();
				pList->setText(eNumber, tr("Sessions"));
				pList->setData(eKey, Qt::UserRole, eListLine);
				pInfo->pItem->addChild(pList);
				pList->setExpanded(true);

				if(pRoute->IsTraced())
				{
					QTreeWidgetItem* &pTrace = pInfo->Sub[eFrames];
					pTrace = new QTreeWidgetItem();
					pTrace->setText(eNumber, tr("Trace"));
					pTrace->setData(eKey, Qt::UserRole, eTraceLine);
					pInfo->pItem->addChild(pTrace);
					pTrace->setExpanded(true);
				}
			}
		}

		QString Type;
		if(pLookup->Inherits("CKadRoute"))
			Type = "KadRoute: " + QByteArray((char*)pLookup->Cast<CKadRoute>()->GetEntityID().GetData(), pLookup->Cast<CKadRoute>()->GetEntityID().GetSize()).toHex();
		else if(pLookup->Inherits("CKadBridge"))
			Type = "KadBridge: " + QByteArray((char*)pLookup->Cast<CKadBridge>()->GetEntityID().GetData(), pLookup->Cast<CKadBridge>()->GetEntityID().GetSize()).toHex();
		else if(pLookup->Inherits("CKadRelay"))
			Type = "KadRelay: " + QByteArray((char*)pLookup->Cast<CKadRelay>()->GetEntityID().GetData(), pLookup->Cast<CKadRelay>()->GetEntityID().GetSize()).toHex();
		else if(CKadOperation* pKadLookup = pLookup->Cast<CKadOperation>())
		{
			if(CKadTask* pKadTask = pLookup->Cast<CKadTask>())
			{
				pInfo->pItem->setText(eName, QString::fromStdWString(pKadTask->GetName()));
				Type = tr("KadLookup");
			}
			else if(pLookup->Inherits("CLookupProxy"))
				Type = "LookupProxy";
			
			CKadScript* pKadScript = NULL;
			if(CKadOperator* pOperation = pKadLookup->GetOperator())
				pKadScript = pOperation->GetScript();
			else if(pKadLookup->GetCodeID().IsValid())
				pKadScript = pEngine->GetScript(pKadLookup->GetCodeID());

			if(pKadScript)
				Type += " (" + QString::fromStdWString(pKadScript->GetName()) + ")";

			if(pKadLookup->IsManualMode())
				Type += " *";
		}
		else
			Type = "NodeLookup";

		pInfo->pItem->setText(eType, Type);

		QString Status;
		if(pLookup->IsStopped())			
			Status = "Finished";
		else if(pLookup->GetStartTime())
		{
			Status = "Running";
			if(pLookup->IsLookingForNodes())
				Status += "*";
			if(uint64 uTime = pLookup->GetTimeRemaining())
				Status += QString(" (%1)").arg(uTime/1000);
		}
		else							
			Status = "Pending";

		pInfo->pItem->setText(eStatus, Status);

		//pItem->setText(eLoad, );
		pInfo->pItem->setText(eHops, tr("%1/%2").arg(pLookup->GetHopLimit()).arg(pLookup->GetJumpCount()));
		
		if(CKadOperation* pKadLookup = pLookup->Cast<CKadOperation>())
		{
			pInfo->pItem->setText(eShares, tr("%1/%2").arg(pKadLookup->GetSpreadShare()).arg(pKadLookup->GetSpreadCount()));
			pInfo->pItem->setText(eResults, tr("%1/%2 (%3)").arg(pKadLookup->GetTotalDoneJobs()).arg(pKadLookup->GetTotalJobs()).arg(pKadLookup->GetTotalReplys()));
			pInfo->pItem->setText(eNodes, tr("%1 - %2").arg(pKadLookup->GetActiveCount()).arg(pKadLookup->GetFailedCount()));
		}

		pInfo->pItem->setText(eUpload, QString::number(double(pLookup->GetUpLimit()->GetRate())/1024.0, 'f', 2) + "kB/s");
		pInfo->pItem->setText(eDownload, QString::number(double(pLookup->GetDownLimit()->GetRate())/1024.0, 'f', 2) + "kB/s");

		if(CKadRelay* pRelay = pLookup->Cast<CKadRelay>())
		{
			int Up = 0;
			if(CFrameRelay* pUpLink = pRelay->GetUpLink())
			{
				QTreeWidgetItem* pSubItem = pInfo->Sub[eUpLinks];
				Up = DumpLinks(pUpLink, pSubItem);
				pSubItem->setText(eRoutes, tr("Up: %1").arg(Up));
			}
			int Down = 0;
			if(CFrameRelay* pDownLink = pRelay->GetDownLink())
			{
				QTreeWidgetItem* pSubItem = pInfo->Sub[eDownLinks];
				Down = DumpLinks(pDownLink, pSubItem);
				pSubItem->setText(eRoutes, tr("Down: %1").arg(Down));
			}
			pInfo->pItem->setText(eRoutes, tr("Up: %1; Down: %2").arg(Up).arg(Down));
		}

		if(CKadRoute* pRoute = pLookup->Cast<CKadRoute>())
		{
			QTreeWidgetItem* pSubItem = pInfo->Sub[eSessions];
			DumpSessions(pRoute, pSubItem);
			if(pInfo->Sub.contains(eFrames))
			{
				QTreeWidgetItem* pSubItem = pInfo->Sub[eFrames];
				DumpTrace(pRoute, pSubItem);
			}
		}
	}

	foreach(SLookup* pInfo, OldLookups)
	{
		m_Lookups.remove(m_Lookups.key(pInfo));
		delete pInfo->pItem;
		delete pInfo;
	}
}

void CSearchList::OnItemClicked(QTreeWidgetItem* pItem, int Column)
{
	if(pItem)
		m_pLookupList->setHeaderLabels(m_Headers[(ELevels)pItem->data(eKey, Qt::UserRole).toInt()]);
}
