#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "KadRelay.h"
#include "../Kademlia.h"
#include "../KadConfig.h"
#include "../KadNode.h"
#include "../KadHandler.h"
#include "../RoutingRoot.h"
#include "FrameRelay.h"
#include "../LookupManager.h"
#include "KadRoute.h"

IMPLEMENT_OBJECT(CKadRelay, CKadLookup)

CKadRelay::CKadRelay(const CUInt128& ID, CObject* pParent)
 : CKadLookup(ID, pParent) 
{
	m_TimeOut = -1; // Note: relay lookups live for as long as the route exist, no timeout

	m_BrancheCount = GetParent<CKademlia>()->Cfg()->GetInt("BrancheCount");

	m_PendingRelays = 0;

	m_LastReset = GetCurTick();

	m_UpLink = NULL;
	m_DownLink = NULL;
}

void CKadRelay::SetBrancheCount(int BrancheCount)
{
	m_BrancheCount = Min(BrancheCount, GetParent<CKademlia>()->Cfg()->GetInt64("MaxBrancheCount"));
}

bool CKadRelay::InitRelay(const CVariant& RouteReq)
{
	m_EntityID = RouteReq["EID"];

	if(!RouteReq.Has("PK"))
		return false;
	
	const CVariant& KeyValue = RouteReq["PK"];
	CScoped<CPublicKey> pKey = new CPublicKey();	
	if(!pKey->SetKey(KeyValue.GetData(), KeyValue.GetSize()))
	{
		LogLine(LOG_ERROR, L"Recived Invalid Key for Entity");
		return false;
	}

	CVariant TestID((byte*)NULL, m_EntityID.GetSize());
	UINT eHashFunkt = RouteReq.Has("HK") ? CAbstractKey::Str2Algorithm(RouteReq["HK"]) & CAbstractKey::eHashFunkt : CAbstractKey::eUndefined;
	CKadID::MakeID(pKey, TestID.GetData(), TestID.GetSize(), eHashFunkt);

	if(m_EntityID == TestID)
	{
		m_pPublicKey = pKey.Detache();
		if(eHashFunkt != CAbstractKey::eUndefined)
			m_pPublicKey->SetAlgorithm(m_pPublicKey->GetAlgorithm() | eHashFunkt);
	}
	else
	{
		LogLine(LOG_ERROR, L"Recived Invalid Key for Entity");
		return false;
	}

	m_UpLink = new CFrameRelay(this);
	m_DownLink = new CFrameRelay(this);
	return true;
}

bool CKadRelay::SearchingRelays()
{
	return m_HopLimit == 1 && (m_PendingRelays > 0 || m_LookupState == eLookupActive);
}

void CKadRelay::Process(UINT Tick)
{
	CKadLookup::Process(Tick); // this looks for more nodes

	if(m_DownLink)
		m_DownLink->Process(Tick); // back to the source

	ASSERT(m_UpLink); // there must always be an uplink
	m_UpLink->Process(Tick); // into the target area

	if((Tick & E10PerSec) == 0)
		return;

	if(m_LookupState == eLookupActive)
		return; // we are looking for closer nodes right now - just wait a second

	// Note: A kad route is a strange type of lookup, 
	// it runs indefinetly and may want to retry nodes that ware already asked in the past.
	// So we have to redo a node lookup in reasonable intervals
	// If we are in jumping mode we never look for closer nodes so no reset needed
	if(GetCurTick() - m_LastReset > SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("RetentionTime") / 2))
	{
		m_LastReset = GetCurTick();

		// remove all disconnected nodes, we may want to retry them
		for(TNodeStatusMap::iterator I = m_Nodes.begin(); I != m_Nodes.end();)
		{
			if(!I->first.pChannel)
				I = m_Nodes.erase(I);
			else
				I++;
		}

		// restart closer node lookup if needed
		InitOperation();
	} 

	int AskForRelay = 0;
	AskForRelay = m_BrancheCount;
	AskForRelay -= Min(AskForRelay, m_PendingRelays + m_UpLink->GetNodes().size());
	ASSERT(AskForRelay >= 0);

	CRoutingZone::SIterator Iter(m_JumpCount > 0 ? 0 : 1);
	for(CKadNode* pNode; (pNode = GetParent<CKademlia>()->Root()->GetClosestNode(Iter, m_ID)) && AskForRelay > 0;)
	{
		// Note: if we are not in jumping mode we can only go to nodes that are closer than we are
		if(m_JumpCount == 0 && ((pNode->GetID() ^ m_ID) >= (GetParent<CKademlia>()->Root()->GetID() ^ m_ID)))
			break; // since we got the nodes ordered by proximity, any further noder will be further away, so we break here

		if(IsNodeUsed(pNode))
			continue;

		if(RequestUpLink(pNode, GetChannel(pNode)))
			AskForRelay--;
	}

	// are we done, arn't any more potential relays we might want
	if(m_PendingRelays == 0) // we dispose right away with all nodes we dont need anymore, used nodes have ben moved to lists inside the relay objects
	{
		for(TNodeStatusMap::iterator I = m_Nodes.begin(); I != m_Nodes.end(); I++)
		{
			if(I->first.pChannel)
			{
				I->first.pChannel->Close();
				I->first.pChannel = NULL;
			}
		}
	}
}

bool CKadRelay::OnCloserNodes(CKadNode* pNode, CComChannel* pChannel, int CloserCount)
{
	if(!CKadLookup::OnCloserNodes(pNode, pChannel, CloserCount))
		return false;

	if(!(m_JumpCount > 1 || ((pNode->GetID() ^ m_ID) < (GetParent<CKademlia>()->Root()->GetID() ^ m_ID))))
		return true;
	if(IsNodeUsed(pNode))
		return true;

	RequestUpLink(pNode, pChannel);
	return true;
}

bool CKadRelay::RequestUpLink(CKadNode* pNode, CComChannel* pChannel)
{
	SNodeStatus* pStatus = GetStatus(pNode);
	if(!pStatus || !pChannel || pStatus->uTimeOut != -1) // error or node already in use
		return false;

	if(GetParent<CKademlia>()->Handler()->SendRouteReq(pNode, pChannel, this) == 0)
		return false;

	pStatus->uTimeOut = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("RequestTimeOut"));
	m_PendingRelays++;

	if(m_LookupHistory)
		m_LookupHistory->ExpectResponse(pNode->GetID());

	return true;
}

void CKadRelay::UpLinkResponse(CKadNode* pNode, CComChannel* pChannel, const string& Error)
{
	TNodeStatusMap::iterator I = m_Nodes.find(SKadNode(pNode));
	if(I == m_Nodes.end()) // It seams to be an unsolicitated response
	{
		pChannel->Close();
		return;
	}
	ASSERT(I->first.pChannel == pChannel);

	ASSERT(I->second->uTimeOut != -1);
	if(I->second->uTimeOut && m_PendingRelays > 0)
		m_PendingRelays--;
	I->second->uTimeOut = -1;

	((SRelayState*)I->second)->Error = Error;

	if(!Error.empty())
		LogLine(LOG_ERROR, L"Recived error: '%S' on route lookup from: %s", Error.c_str(), pNode->GetID().ToHex().c_str());
	else
	{
		ASSERT(m_UpLink);
		m_UpLink->Add(pNode, pChannel);

		delete I->second;
		m_Nodes.erase(I); // do not keep the nodes in this list longer than nececery

		if(m_LookupHistory)
			m_LookupHistory->RegisterResponse(pNode->GetID());
	}
}

bool CKadRelay::IsNodeUsed(CKadNode* pNode)
{
	if(m_UpLink->Has(pNode))
		return true;
	if(m_DownLink && m_DownLink->Has(pNode))
		return true;
	if(SRelayState* pStatus = GetState(pNode))
		return !pStatus->Error.empty() || (pStatus->uTimeOut != -1);
	return false;
}

bool CKadRelay::AcceptDownLink(CKadNode* pNode)
{
	// Note: it is important to prevent creation self holding circles
	//			this is achived by allowing only one direction of information flow
	if(m_JumpCount > 0) // jump nodes are allowed only to have one downlink node
		return m_DownLink->IsEmpty(true); 
	// target nodes accept only downlinks that are further away than them selves
	return (pNode->GetID() ^ m_ID) > (GetParent<CKademlia>()->Root()->GetID() ^ m_ID);
}

string CKadRelay::AddDownLink(CKadNode* pNode, CComChannel* pChannel)
{
	if(!m_DownLink) // make the origin node behave kien a normal jump node
		return "Busy";

	if(m_DownLink->Has(pNode))
		return ""; // its already in the list

	if(!AcceptDownLink(pNode))
		return "Busy";

	if(!m_pPublicKey)
		return "KeyError";
	
	m_DownLink->Add(pNode, pChannel);
	return ""; // accepted
}

bool CKadRelay::RelayUp(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel)	
{
	if(!m_UpLink)
		return false;

	// if this is an out of route request, we are not jumping anymore and we are close enough
	if(Frame.Has("TID") && m_JumpCount == 0 && m_ID != Frame["TID"] && (m_ID ^ GetParent<CKademlia>()->Root()->GetID()) <= GetParent<CKademlia>()->Root()->GetMaxDistance())
	{
		CLookupManager* pLookupManager = GetParent<CKademlia>()->Manager();
		CPointer<CKadBridge> pUpBridge = pLookupManager->GetBridge(Frame["EID"], Frame["TID"]);
		if(!pUpBridge)
		{
			pUpBridge = new CKadBridge(Frame.At("TID"), pLookupManager);
			pUpBridge->InitBridge(this); // this always successes as we just copy the data form the already validated relay

			if(IsTraced())
				pUpBridge->EnableTrace();

			pLookupManager->StartLookup(pUpBridge.Obj());
		}

		return pUpBridge->RelayUp(Frame, TTL, pFromNode, pChannel);
	}

	return m_UpLink->Relay(Frame, TTL, pFromNode, pChannel);
}

bool CKadRelay::RelayDown(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel)
{
	ASSERT(m_DownLink);
	return m_DownLink->Relay(Frame, TTL, pFromNode, pChannel);
}

bool CKadRelay::AckUp(const CVariant& Ack, bool bDelivery)
{
	ASSERT(m_UpLink);
	return m_UpLink->Ack(Ack, bDelivery);
}

bool CKadRelay::AckDown(const CVariant& Ack, bool bDelivery)
{
	if(!m_DownLink)
		return false;
	return m_DownLink->Ack(Ack, bDelivery);
}

void CKadRelay::Refresh()
{
	// Note: if we call refresh even once it wil make the relay temporary and vanish as soon as the tuneout passes without the next refresh
	uint64 uRouteTimeout = SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("RouteTimeout"));
	SetTimeOut(uRouteTimeout, true);
}

bool CKadRelay::ReadyToStop()
{
	bool bTimedOut = HasTimedOut();
	return bTimedOut || (m_DownLink && m_DownLink->IsEmpty()); // if we dont have any valid down link the relay is broken and must be removed
}

///////////////////////////////////////////////////////////////////////////
//

IMPLEMENT_OBJECT(CKadBridge, CKadRelay)

CKadBridge::CKadBridge(const CUInt128& ID, CObject* pParent)
 : CKadRelay(ID,  pParent)
{
}

void CKadBridge::InitBridge(CKadRelay* pRelay)
{
	Refresh(); // make this type temporary

	m_pRelay = pRelay;
	m_pRelay.SetWeak(); // when the parrent relay is removed kill the bridge

	m_pPublicKey = new CPublicKey();
	m_pPublicKey->SetKey(pRelay->GetPublicKey());

	m_EntityID = CVariant((byte*)NULL, KEY_64BIT);
	CKadID::MakeID(m_pPublicKey, m_EntityID.GetData(), m_EntityID.GetSize());

	m_UpLink = new CFrameRelay(this);
	m_UpLink->AllowDelay();
	// m_DownLink == NULL;

	SetHopLimit(1); // bridges are always direct
	SetJumpCount(0);

	SetBrancheCount(pRelay->GetBrancheCount());
}

void CKadBridge::Process(UINT Tick)
{
	ASSERT(m_pRelay); // there always must be a relay!

	CKadRelay::Process(Tick);
}

bool CKadBridge::RelayUp(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel)
{
	Refresh();
	return CKadRelay::RelayUp(Frame, TTL, pFromNode, pChannel);
}

bool CKadBridge::AckUp(const CVariant& Ack, bool bDelivery)
{
	if(bDelivery) // If we are the initiatio of the route we can not do an implicit handlof, we must do this explicitly on site
	{
		if(CKadRoute* pRoute = m_pRelay->Cast<CKadRoute>())
			pRoute->AckUp(Ack, bDelivery);
	}
	return CKadRelay::AckUp(Ack, bDelivery);
}

// Note: Bridging is a bit wired, when we send a frame the actual relay object is retrived,
//			we see that the target ID is wrong and than we look for a bridge (or create a new one),
//			than we pass the frame to the bridge as if it whould be the relay.
//			The bridge will relay the ack to the previuse node, the relay will not be used.
//		 When the remote side sends us a packet from the relay that corresponds to the bridge we have,
//			the bridge wont be retrived but the actuall relay for which we setup the bridge,
//			it will handle the request and the ack.
//		 So the bridge is kind of a one way workaround.

bool CKadBridge::ReadyToStop()
{
	ASSERT(!m_DownLink);
	return CKadRelay::ReadyToStop() || !m_pRelay || m_pRelay->ReadyToStop();
}	