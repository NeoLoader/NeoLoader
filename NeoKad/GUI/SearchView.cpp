#include "GlobalHeader.h"
#include "SearchView.h"
#include "../NeoKad.h"
#include "../Kad/KadHeader.h"
#include "../Kad/Kademlia.h"
#include "../Kad/KadHandler.h"
#include "../Kad/LookupManager.h"
#include "../Kad/KadLookup.h"
#include "../Kad/RoutingRoot.h"
#include "../Kad/FirewallHandler.h"
#include "../Kad/KadRouting/KadRelay.h"
#include "../Kad/KadRouting/FrameRelay.h"
#include "RouteGraph.h"

CSearchView::CSearchView(QWidget *parent)
: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setMargin(0);

	m_pRouteGraph = new CRouteGraph();

	m_pMainLayout->addWidget(m_pRouteGraph);
	setLayout(m_pMainLayout);
}

void CSearchView::DumpSearch(QVariant ID, map<CVariant, CPointer<CKadLookup> >& AuxMap)
{
	CLookupManager* pLookupManager = theKad->Kad()->GetChild<CLookupManager>();
	CRoutingRoot* pRoot = theKad->Kad()->GetChild<CRoutingRoot>();
	if(!pLookupManager)
		return;
	
	CVariant vID;
	vID.FromQVariant(ID);
	CKadLookup* pLookup = pLookupManager->GetLookup(vID);
	if(!pLookup)
	{
		map<CVariant, CPointer<CKadLookup> >::iterator I = AuxMap.find(vID);
		if(I != AuxMap.end())
			pLookup = I->second;
	}
	if(!pLookup)
		return;

	if(m_CurID != ID)
	{
		m_CurID = ID;
		m_pRouteGraph->Reset();
	}

	QList<SRouteEntry> Entrys;

	for(LookupNodeMap::const_iterator I = pLookup->GetHistory()->GetNodes().begin(); I != pLookup->GetHistory()->GetNodes().end(); I++)
	{
		const SLookupNode &Node = I->second;

		//if(!m_pShowAll->isChecked() && Node.AskedCloser == 0 && Node.AskedResults == 0 && Node.RemoteResults.empty())
		//	continue;

		SRouteEntry Entry;
		Entry.ID = I->first;
		//if(m_pShowAll->isChecked())
		{
			if(Node.FoundByIDs.empty())
				Entry.ByIDs[SELF_NODE] = Qt::gray;
			else
			{
				for(list<CUInt128>::const_iterator J = Node.FoundByIDs.begin(); J != Node.FoundByIDs.end(); J++)
					Entry.ByIDs[*J] = Qt::gray;
			}
		}
		if(Node.FromID != 0) // asked
			Entry.ByIDs[Node.FromID] = Node.FoundResults ? Qt::green : Qt::red;

		int RemoteResults = 0;
		for(map<CUInt128, SLookupNode::SRemoteResults>::const_iterator J = Node.RemoteResults.begin(); J != Node.RemoteResults.end(); J++)
		{
			//if(J->second.TimeOut > GetCurTick())
			RemoteResults += J->second.Count;
			if(J->second.Count)
				Entry.ByIDs[J->first] = Qt::green;
			else //if(m_pShowAll->isChecked())
				Entry.ByIDs[J->first] = Qt::red;
		}
						
		CUInt128 uDistance = I->first ^ pLookup->GetID();
		Entry.Distance = uDistance;
						
		Entry.Time = Node.GraphTime;

		if(Max(Node.AskedCloser, Node.AskedResults) == 0 && Node.RemoteResults.empty())
			Entry.Color = Qt::white;
		else if(Node.AnsweredCloser == -1 || Node.AnsweredResults == -1) // -1 marked as timmed out
			Entry.Color = Qt::red;
		else if((Node.AskedCloser && !Node.AnsweredCloser) || (Node.AskedResults && !Node.AnsweredResults)) // asked but yet not answered
			Entry.Color = Qt::yellow;
		else if(Node.FoundCloser > 0 && (Node.FoundResults + RemoteResults) > 0)
			Entry.Color = Qt::cyan;
		else if(Node.FoundCloser > 0)
			Entry.Color = Qt::blue;
		else if((Node.FoundResults + RemoteResults) > 0)
			Entry.Color = Qt::green;
		else
			Entry.Color = Qt::magenta;

		QString Addresses = "Unknown (no in local routing table)";
		if(CKadNode* pNode = pRoot->GetNode(I->first))
			Addresses = ListAddr(pNode->GetAddresses());

		Entry.Tipp = tr("Node ID: %1\r\nTarget Distance: %2\r\nAddresses: %3\r\n")
			.arg(QString::fromStdWString(I->first.ToHex())).arg(QString::fromStdWString(uDistance.ToBin())).arg(Addresses);
		Entry.Tipp += tr("-------------------------------\r\n");
		if(Node.AskedCloser)
			Entry.Tipp += tr("Found Closer: %1\r\n").arg(Node.FoundCloser);
		if(Node.AskedResults)
			Entry.Tipp += tr("Found Results: %1\r\n").arg(Node.FoundResults);
		
		Entrys.append(Entry);
	}

	SRouteEntry SelfEntry;
	SelfEntry.ID = SELF_NODE;
	CUInt128 uDistance = pRoot->GetID() ^ pLookup->GetID();
	SelfEntry.Distance = uDistance;
	SelfEntry.Time = pLookup->GetStartTime();
	SelfEntry.Color = Qt::gray;
	CFirewallHandler* pFirewallHandler = theKad->Kad()->GetChild<CFirewallHandler>();
	SelfEntry.Tipp = tr("Node ID: %1\r\nTarget Distance: %2\r\nAddresses: %3")
		.arg(QString::fromStdWString(pRoot->GetID().ToHex())).arg(QString::fromStdWString(uDistance.ToBin())).arg(ListAddr(pFirewallHandler->AddrPool()));

	if(CKadRelay* pRelay = pLookup->Cast<CKadRelay>())
	{
		if(CFrameRelay* pDownLink = pRelay->GetDownLink())
		{
			CFrameRelay::TRelayMap& DownNodes = pDownLink->GetNodes();
			for(CFrameRelay::TRelayMap::iterator I = DownNodes.begin(); I != DownNodes.end(); I++)
			{
				const SKadNode &Node = I->first;

				SelfEntry.ByIDs[Node.pNode->GetID()] = Qt::green;

				bool bFound = false;
				foreach(const SRouteEntry& Entry, Entrys)
				{
					if(Entry.ID == Node.pNode->GetID())
					{
						bFound = true;
						break;
					}
				}
				if(bFound)
					continue;

				SRouteEntry Entry;
				Entry.ID = Node.pNode->GetID();
						
				CUInt128 uDistance = Entry.ID ^ pLookup->GetID();
				Entry.Distance = uDistance;
						
				Entry.Time = pLookup->GetStartTime();

				Entry.Color = Qt::green;


				QString Addresses = "Unknown (no in local routing table)";
				Addresses = ListAddr(Node.pNode->GetAddresses());

				Entry.Tipp = tr("Node ID: %1\r\nTarget Distance: %2\r\nAddresses: %3\r\n")
					.arg(QString::fromStdWString(Entry.ID.ToHex())).arg(QString::fromStdWString(uDistance.ToBin())).arg(Addresses);
			
				Entrys.append(Entry);
			}
		}
	}

	Entrys.append(SelfEntry);

	m_pRouteGraph->ShowRoute(Entrys);
}