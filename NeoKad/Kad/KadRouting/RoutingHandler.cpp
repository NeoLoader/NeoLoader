#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "../KadHandler.h"
#include "../KadNode.h"
#include "../Kademlia.h"
#include "../KadConfig.h"
#include "../FirewallHandler.h"
#include "../RoutingFork.h"
#include "../RoutingRoot.h"
#include "../PayloadIndex.h"
#include "../LookupManager.h"
#include "../KadLookup.h"
#include "FrameRelay.h"
#include "KadRelay.h"
#include "../KadTask.h"
#include "../../Common/FileIO.h"
#include "../KadEngine/KadEngine.h"

////////////////////////////////////////////////////////////////////////
// Route

UINT CKadHandler::SendRouteReq(CKadNode* pNode, CComChannel* pChannel, CKadRelay* pRelay, bool bRefresh)
{
	CVariant RouteReq(CVariant::EMap);

	if(!bRefresh) // do not send full route request if its only a refresh
	{
		RouteReq["TID"] = pRelay->GetID();
		RouteReq["LID"] = pRelay->GetLookupID();

		uint8 HopLimit = pRelay->GetHopLimit();
		if(HopLimit-- > 1) // 0 direct, 1 last hop, in booth cases we have to handle teh situation on our own
		{
			uint8 JumpCount = pRelay->GetJumpCount();
			if(JumpCount-- > 1)
				RouteReq["JMPS"] = Min(JumpCount, HopLimit);
			RouteReq["HOPS"] = HopLimit;
		}

		RouteReq["BRCH"] = uint8(pRelay->GetBrancheCount());

		RouteReq["EID"] = pRelay->GetEntityID(); // our entity is the sender

		CPublicKey* pPublicKey = pRelay->GetPublicKey();
		RouteReq["PK"] = CVariant(pPublicKey->GetKey(), pPublicKey->GetSize());
		if((pPublicKey->GetAlgorithm() & CAbstractKey::eHashFunkt) != 0)
			RouteReq["HK"] = CAbstractKey::Algorithm2Str((pPublicKey->GetAlgorithm() & CAbstractKey::eHashFunkt));

		if(pRelay->IsTraced())
			RouteReq["TRACE"] = CVariant();
	}

	if(UINT uID = pChannel->QueuePacket(KAD_ROUTE_REQUEST, RouteReq))
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRU"))
			LogLine(LOG_DEBUG, L"Sending 'Route Resuest' to %s", pNode->GetID().ToHex().c_str());
		return uID;
	}
	return 0;
}

void CKadHandler::HandleRouteReq(const CVariant& RouteReq, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRU"))
		LogLine(LOG_DEBUG, L"Recived 'Route Resuest' from %s", pNode->GetID().ToHex().c_str());

	CVariant RouteRes(CVariant::EMap);

	SKadData* pData = pChannel->GetData<SKadData>();
	CPointer<CKadRelay> pRelay = pData->pLookup->Cast<CKadRelay>();
	if(!pRelay)
	{
		if(!RouteReq.Has("TID")) // this is optional it is not send on a refresn
			throw CException(LOG_ERROR, L"Invalid Lookup Request");

		if(pData->pLookup)
			throw CException(LOG_ERROR, L"Recived Route Resuest for a lookup that is not a CKadRelay");

		CLookupManager* pLookupManager = GetParent<CKademlia>()->Manager();
		pRelay = pLookupManager->GetRelayEx(RouteReq["EID"], RouteReq["TID"]); // find already existing relay for this Entity and target combination
		//ASSERT(pRelay == pLookupManager->GetLookup(RouteReq["LID"])->Cast<CKadRelay>()); // lookup ID should be consistent

		if(!pRelay)
		{
			pRelay = new CKadRelay(RouteReq["TID"], pLookupManager);
			if(pRelay->InitRelay(RouteReq)) // if false it means the lookup is invalid
			{
				pRelay->SetHopLimit(RouteReq.Get("JMPS"));
				pRelay->SetJumpCount(RouteReq.Get("HOPS"));

				pRelay->SetBrancheCount(RouteReq.Get("BRCH"));

				pRelay->SetLookupID(RouteReq["LID"]);
				pLookupManager->StartLookup(pRelay.Obj());

				if(RouteReq.Has("TRACE"))
					pRelay->EnableTrace();
			}
		}

		// For this cahnnel the relay was new, setup BC
		pData->pLookup = CPointer<CKadLookup>(pRelay, true); // weak pointer
		pChannel->AddUpLimit(pData->pLookup->GetUpLimit());
		pChannel->AddDownLimit(pData->pLookup->GetDownLimit());
	}

	string Error = pRelay->AddDownLink(pNode, pChannel); // add or update
	if(!Error.empty())
		RouteRes["ERR"] = Error;

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRU"))
		LogLine(LOG_DEBUG, L"Sending 'Route Response' to %s", pNode->GetID().ToHex().c_str());
	pChannel->QueuePacket(KAD_ROUTE_RESPONSE, RouteRes);
}

void CKadHandler::HandleRouteRes(const CVariant& RouteRes, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRU"))
		LogLine(LOG_DEBUG, L"Recived 'Route Response' from %s", pNode->GetID().ToHex().c_str());

	SKadData* pData = pChannel->GetData<SKadData>();
	CKadRelay* pRelay = pData->pLookup->Cast<CKadRelay>();
	if(!pRelay)
		throw CException(LOG_ERROR, L"Recived Route Response for a lookup that is not a CKadRelay");

	// Note: this is for trace only, it does not have any deeper purpose
	if(pRelay->IsTraced())
	{
		CVariant Trace = RouteRes.Get("TRACE").Clone();
		Trace.Append(pNode->GetID());

		if(!pRelay->Inherits("CKadRoute")) // if its just a relay we have to relay the trace back
		{
			CVariant RouteRel;
			RouteRel["TID"] = RouteRes["TID"]; // pRelay->GetID()
			RouteRel["TRACE"] = Trace;

			if(CFrameRelay* pDownLink = pRelay->GetDownLink())
			{
				CFrameRelay::TRelayMap& DownNodes = pDownLink->GetNodes();
				for(CFrameRelay::TRelayMap::iterator I = DownNodes.begin(); I != DownNodes.end(); I++)
				{
					if(I->first.pChannel->IsConnected())
					{
						if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
							LogLine(LOG_DEBUG, L"Relaying 'Route Response' from %s to %s", pNode->GetID().ToHex().c_str(), I->first.pNode->GetID().ToHex().c_str());
						I->first.pChannel->QueuePacket(KAD_ROUTE_RESPONSE, RouteRel);
					}
				}
			}
		}
		else
			pRelay->RecivedTraceResults(Trace);
	}

	if(RouteRes.Has("TRACE")) // if this is set its a pro forma trace response ignore it
		return;
	
	pRelay->UpLinkResponse(pNode, pChannel, RouteRes.Get("ERR"));
}

////////////////////////////////////////////////////////////////////////
// Relay

bool CKadHandler::SendRelayReq(CKadNode* pNode, CComChannel* pChannel, const CVariant& Frame, uint64 TTL, CKadRelay* pRelay)
{
	CVariant RelayReq;

	RelayReq["TTL"] = TTL;

	RelayReq["FRM"] = Frame;

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRE"))
		LogLine(LOG_DEBUG, L"Sending 'Relay Resuest' to %s", pNode->GetID().ToHex().c_str());
	return pChannel->QueuePacket(KAD_RELAY_REQUEST, RelayReq);
}

void CKadHandler::HandleRelayReq(const CVariant& RelayReq, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRE"))
		LogLine(LOG_DEBUG, L"Recived 'Relay Resuest' from %s", pNode->GetID().ToHex().c_str());

	const CVariant& Frame = RelayReq["FRM"];

	CLookupManager* pLookupManager = GetParent<CKademlia>()->Manager();
	if(CPointer<CKadRelay> pDownRelay = pLookupManager->GetRelay(Frame["RID"])) // relay down to the source
	{
		// K-ToDo-Now: send trace down to source
		pDownRelay->RelayDown(Frame, RelayReq["TTL"], pNode, pChannel);
	}
	else if(CPointer<CKadRelay> pUpRelay = pLookupManager->GetRelay(Frame["EID"])) // relay up into the target area
	{
		pUpRelay->RelayUp(Frame, RelayReq["TTL"], pNode, pChannel);
	}
	else
	{
		LogLine(LOG_ERROR, L"Resived a relay request from %s: for an unknown destination or from an unknown source", pNode->GetID().ToHex().c_str());
		SendRelayRes(pNode, pChannel, Frame, "UnknownRoute"); // NACK - we can not route the packet
	}
}

bool CKadHandler::SendRelayRes(CKadNode* pNode, CComChannel* pChannel, const CVariant& Frame, const string& Error, const CVariant& Load)
{
	CVariant RelayRes;

	CVariant FrameRes;
	if(Frame.Has("TID"))
		FrameRes["TID"] = Frame["TID"];
	FrameRes["EID"] = Frame["EID"];
	FrameRes["RID"] = Frame["RID"];
	FrameRes["FID"] = Frame["FID"]; // peer to peer (local) ACK
	if(!Error.empty())
		FrameRes["ERR"] = Error;
	if(Load.IsValid())
		FrameRes["LOAD"] = Load;
	RelayRes["FRM"] = FrameRes;

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRE"))
		LogLine(LOG_DEBUG, L"Sending 'Relay Response' to %s", pNode->GetID().ToHex().c_str());
	return pChannel->QueuePacket(KAD_RELAY_RESPONSE, RelayRes);
}

void CKadHandler::HandleRelayRes(const CVariant& RelayRes, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRE"))
		LogLine(LOG_DEBUG, L"Recived 'Relay Response' from %s", pNode->GetID().ToHex().c_str());

	const CVariant& FrameRes = RelayRes["FRM"];
	//if(FrameRes.Has("ERR")) // errors are normal, Busy, NoNodes, stc...
	//	LogLine(LOG_ERROR, L"Resived a relay response from %s: with an error %s", pNode->GetID().ToHex().c_str(), FrameRes.At("ERR").To<wstring>().c_str());

	CLookupManager* pLookupManager = GetParent<CKademlia>()->Manager();
	if(CPointer<CKadRelay> pUpRelay = FrameRes.Has("TID") ? pLookupManager->GetRelayEx(FrameRes["EID"], FrameRes["TID"]) : pLookupManager->GetRelay(FrameRes["EID"])) // relay up into the target area
	{
		pUpRelay->AckUp(FrameRes);
	}
	if(CPointer<CKadRelay> pDownRelay = pLookupManager->GetRelay(FrameRes["RID"])) // relay down to the source
	{
		pDownRelay->AckDown(FrameRes);
	}
	//else 
	//	LogLine(LOG_ERROR, L"Resived a relay response from %s: for an unknown destination or from an unknown source", pNode->GetID().ToHex().c_str());	
}

bool CKadHandler::SendRelayRet(CKadNode* pNode, CComChannel* pChannel, const CVariant& Ack)
{
	CVariant RelayRet;

	RelayRet["ACK"] = Ack;

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRE"))
		LogLine(LOG_DEBUG, L"Sending 'Relay Return' to %s", pNode->GetID().ToHex().c_str());
	return pChannel->QueuePacket(KAD_RELAY_RETURN, RelayRet);
}

void CKadHandler::HandleRelayRet(const CVariant& RelayRet, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRE"))
		LogLine(LOG_DEBUG, L"Recived 'Relay Return' from %s", pNode->GetID().ToHex().c_str());

	CLookupManager* pLookupManager = GetParent<CKademlia>()->Manager();
	const CVariant& Ack = RelayRet["ACK"];
	if(CPointer<CKadRelay> pUpRelay = Ack.Has("TID") ? pLookupManager->GetRelayEx(Ack["EID"], Ack["TID"]) : pLookupManager->GetRelay(Ack["EID"])) // relay up into the target area
	{
		pUpRelay->AckUp(Ack, true);
	}
	if(CPointer<CKadRelay> pDownRelay = pLookupManager->GetRelay(Ack["RID"])) // relay down to the source
	{
		pDownRelay->AckDown(Ack, true);
	}
	//else 
	//	LogLine(LOG_ERROR, L"Resived a 'Relay Return' from %s: for an unknown destination or from an unknown source", pNode->GetID().ToHex().c_str());
}

void CKadHandler::SendRelayCtrl(CKadRelay* pRelay, const CVariant& Control)
{
	CVariant RelayCtrl;

	RelayCtrl["CTRL"] = Control;

	CFrameRelay* pUpLink = pRelay->GetUpLink();
	if(!pUpLink)
		return;
		
	CFrameRelay::TRelayMap& UpNodes = pUpLink->GetNodes();
	for(CFrameRelay::TRelayMap::iterator I = UpNodes.begin(); I != UpNodes.end(); I++)
	{
		if(I->first.pChannel->IsConnected())
		{
			if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
				LogLine(LOG_DEBUG, L"Sending/Relaying 'Relay Control' to %s", I->first.pNode->GetID().ToHex().c_str());
			I->first.pChannel->QueuePacket(KAD_ROUTE_RESPONSE, RelayCtrl);
		}
	}
}

void CKadHandler::HandleRelayCtrl(const CVariant& ControlReq, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRE"))
		LogLine(LOG_DEBUG, L"Recived 'Relay Control' from %s", pNode->GetID().ToHex().c_str());

	const CVariant& Control = ControlReq["CTRL"];

	CLookupManager* pLookupManager = GetParent<CKademlia>()->Manager();
	if(CPointer<CKadRelay> pUpRelay = pLookupManager->GetRelay(Control["EID"])) // relay up into the target area
	{
		// K-ToDo-Now: do something with it
		SendRelayCtrl(pUpRelay, Control); // relay up
	}
}

void CKadHandler::SendRelayStat(CKadRelay* pRelay, const CVariant& Status)
{
	CVariant RelayStat;

	RelayStat["STAT"] = Status;

	CFrameRelay* pDownLink = pRelay->GetDownLink();
	if(!pDownLink)
		return;

	CFrameRelay::TRelayMap& DownNodes = pDownLink->GetNodes();
	for(CFrameRelay::TRelayMap::iterator I = DownNodes.begin(); I != DownNodes.end(); I++)
	{
		if(I->first.pChannel->IsConnected())
		{
			if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
				LogLine(LOG_DEBUG, L"Sending/Relaying 'Relay Status' to %s", I->first.pNode->GetID().ToHex().c_str());
			I->first.pChannel->QueuePacket(KAD_ROUTE_RESPONSE, RelayStat);
		}
	}
}

void CKadHandler::HandleRelayStat(const CVariant& StatusReq, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRE"))
		LogLine(LOG_DEBUG, L"Recived 'Relay Status' from %s", pNode->GetID().ToHex().c_str());

	const CVariant& Status = StatusReq["STAT"];

	CLookupManager* pLookupManager = GetParent<CKademlia>()->Manager();
	if(CPointer<CKadRelay> pUpRelay = pLookupManager->GetRelay(Status["EID"])) // relay up into the target area
	{
		// K-ToDo-Now: do something with it
		SendRelayStat(pUpRelay, Status); // relay down
	}
}