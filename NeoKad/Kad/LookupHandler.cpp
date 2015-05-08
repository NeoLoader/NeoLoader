#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadHandler.h"
#include "KadNode.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "FirewallHandler.h"
#include "RoutingFork.h"
#include "RoutingRoot.h"
#include "PayloadIndex.h"
#include "LookupManager.h"
#include "KadLookup.h"
#include "KadRouting/KadRelay.h"
#include "KadTask.h"
#include "../Common/FileIO.h"
#include "../Common/v8Engine/JSScript.h"
#include "KadEngine/KadEngine.h"
#include "KadEngine/KadScript.h"
#include "KadEngine/KadOperator.h"

CKadScript* CKadHandler::GetKadScript(const CVariant& LookupReq)
{
	CKadScript* pKadScript = GetParent<CKademlia>()->Engine()->GetScript(LookupReq["CID"]);
	if(pKadScript)
    {
		if (LookupReq.Has("AUTH") && !pKadScript->IsAuthenticated()) // If the script is not authenticated
			return NULL;
		if(pKadScript->GetVersion() < LookupReq.Get("VER").To<uint32>())
			return NULL;

		// Note: this should only be used for debugging unauthenticated scripts
		if(LookupReq.Has("CSH")) 
		{
			UINT eHashFunkt = LookupReq.Has("HK") ? CAbstractKey::Str2Algorithm(LookupReq["HK"]) & CAbstractKey::eHashFunkt : CAbstractKey::eSHA256;
			CHashFunction Hash(eHashFunkt);
			if(!Hash.IsValid())
				return NULL;
			const string &Source = pKadScript->GetSource();
			Hash.Add((byte*)Source.data(), Source.size());
			Hash.Finish();
			if(LookupReq["CSH"] != CVariant(Hash.GetKey(), Hash.GetSize(), CVariant::EBytes))
				return NULL;
		}
	}
	return pKadScript;
}

bool CKadHandler::SetKadScript(CKadOperation* pLookup, CVariant& LookupReq)
{
	CKadScript* pKadScript;
	if(CKadOperator* pOperator = pLookup->GetOperator())
	{
		pKadScript = pOperator->GetScript();
		ASSERT(pKadScript);
	}
	else if(pLookup->GetCodeID().IsValid())
	{
		pKadScript = GetParent<CKademlia>()->Engine()->GetScript(pLookup->GetCodeID());
		ASSERT(pKadScript);
	}
	
	if(!pKadScript)
		return false;

	LookupReq["CID"] = pKadScript->GetCodeID();
	LookupReq["VER"] = pKadScript->GetVersion(); // request at least the current version
	if(pKadScript->IsAuthenticated())
		LookupReq["AUTH"] = CVariant(); // request authenticated script if this one is authenticated to
	else // Note: this should only be used for debugging scripts 
	{
		CHashFunction Hash(CAbstractKey::eSHA256);
		const string &Source = pKadScript->GetSource();
		Hash.Add((byte*)Source.data(), Source.size());
		Hash.Finish();
		LookupReq["CSH"] = CVariant(Hash.GetKey(), Hash.GetSize(), CVariant::EBytes);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////
// Lookup

UINT CKadHandler::SendProxyReq(CKadNode* pNode, CComChannel* pChannel, CKadOperation* pLookup, int SpreadShare, const CVariant &InitParam)
{
	CVariant LookupReq;
	LookupReq["TID"] = pLookup->GetID();
	LookupReq["LID"] = pLookup->GetLookupID();

	uint8 HopLimit = pLookup->GetHopLimit();
	if(HopLimit-- > 1) // 0 direct, 1 last hop, in booth cases we have to handle teh situation on our own
	{
		uint8 JumpCount = pLookup->GetJumpCount();
		if(JumpCount-- > 1)
			LookupReq["JMPS"] = Min(JumpCount, HopLimit);
		LookupReq["HOPS"] = HopLimit;
	}

	uint64 TimeOut = pLookup->GetTimeOut();
	if(TimeOut != -1)
	{
		uint64 Duration = GetCurTick() - pLookup->GetStartTime();
		ASSERT(Duration < TimeOut);
		uint32 TimeOutLeft = (TimeOut - Duration);
		LookupReq["TIMO"] = TimeOutLeft; // Lookup timeout im ms
	}

	LookupReq["SPRD"] = uint8(pLookup->GetSpreadCount());
	LookupReq["SHRE"] = SpreadShare;

	if(SetKadScript(pLookup, LookupReq))
	{
		if(InitParam.IsValid())
			LookupReq["INIT"] = InitParam;
	}

	CVariant AK = pLookup->GetAccessKey();
	if(AK.IsValid())
		LookupReq["AK"] = AK;

	if(pLookup->IsTraced())
		LookupReq["TRACE"] = CVariant();

	if(UINT uID = pChannel->QueuePacket(KAD_PROXY_REQUEST, LookupReq))
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Sending 'Proxy Resuest' to %s", pNode->GetID().ToHex().c_str());
		return uID;
	}
	return 0;
}

void CKadHandler::HandleProxyReq(const CVariant& LookupReq, CKadNode* pNode, CComChannel* pChannel, bool bDelayed)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Recived 'Proxy Resuest' from %s", pNode->GetID().ToHex().c_str());

	if(!LookupReq.Has("TID"))
		throw CException(LOG_ERROR, L"Invalid Proxy Request");

	CVariant LookupRes(CVariant::EMap);

	SKadData* pData = pChannel->GetData<SKadData>();
	if(pData->pLookup && !pData->pLookup->Cast<CLookupProxy>())
		throw CException(LOG_ERROR, L"Recived Proxy Resuest for a lookup that is not a CLookupProxy");

	CKadScript* pKadScript = NULL;
	if(LookupReq.Has("CID"))
	{
		pKadScript = GetKadScript(LookupReq);
		// If we dont have the desired script hold the packet back and request the script
		if(!pKadScript)
		{
			ASSERT(!bDelayed);
			if(!bDelayed)
			{
				SendCodeReq(pNode, pChannel, LookupReq["CID"]);
				pData->DelayedPackets.push_back(make_pair(KAD_PROXY_REQUEST, LookupReq));	
			}
			return;
		}
		LookupRes["VER"] = pKadScript->GetVersion();
	}

	CLookupManager* pLookupManager = GetParent<CKademlia>()->Manager();
	CKadLookup* pLookup = pLookupManager->GetLookup(LookupReq["LID"]);
	if(!pLookup)
	{
		CPointer<CLookupProxy> pProxy = new CLookupProxy(LookupReq["TID"], pLookupManager);
		pProxy->InitProxy(pNode, pChannel);

		pProxy->SetHopLimit(LookupReq.Get("HOPS"));
		pProxy->SetJumpCount(LookupReq.Get("JMPS"));

		pProxy->SetTimeOut(LookupReq.Get("TIMO"));

		pProxy->SetSpreadCount(LookupReq.Get("SPRD"));
		pProxy->SetSpreadShare(LookupReq.Get("SHRE"));

		string Error;
		if(pKadScript)
		{
			if(LookupReq.Has("INIT")) // do we want the script to run on this node, if so we must have specifyed a initialisation parameter
			{
				Error = pProxy->SetupScript(pKadScript);
				pProxy->SetInitParam(LookupReq["INIT"]);
			}
			else
				pProxy->SetCodeID(pKadScript->GetCodeID());
		}

		if(Error.empty())
		{
			if(LookupReq.Has("AK"))
				pProxy->SetAccessKey(LookupReq["AK"]);

			pProxy->SetLookupID(LookupReq["LID"]);
			pLookupManager->StartLookup(pProxy.Obj());

			if(LookupReq.Has("TRACE"))
				pProxy->EnableTrace();

			// For this cahnnel the proxy was new, setup BC
			pData->pLookup = CPointer<CKadLookup>(pProxy, true); // weak pointer
			pChannel->AddUpLimit(pData->pLookup->GetUpLimit());
			pChannel->AddDownLimit(pData->pLookup->GetDownLimit());
		}
		else
			LookupRes["ERR"] = Error;
	}
	else // Note: if we are already handling this particular lookup answer with a busy error.
		LookupRes["ERR"] = "Busy";

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Sending 'Proxy Response' to %s", pNode->GetID().ToHex().c_str());
	pChannel->QueuePacket(KAD_PROXY_RESPONSE, LookupRes);
}

void CKadHandler::HandleProxyRes(const CVariant& LookupRes, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Recived 'Proxy Response' from %s", pNode->GetID().ToHex().c_str());

	SKadData* pData = pChannel->GetData<SKadData>();
	if(!pData->pLookup)
		return;
	CPointer<CKadOperation> pLookup = pData->pLookup->Cast<CKadOperation>();
	if(!pLookup)
		throw CException(LOG_ERROR, L"Recived Proxy Response for a lookup that is not a CKadOperation");

	// Relay the results back to the requesting client
	if(pLookup->IsTraced()) // dont send back completly filtred resposnes, unless for trace
	{
		CVariant Trace = LookupRes.Get("TRACE").Clone();
		Trace.Append(pNode->GetID());

		if(CLookupProxy* pProxy = pLookup->Cast<CLookupProxy>())
		{
			CVariant LookupRel;
			LookupRel["TID"] = pLookup->GetID();
			LookupRel["LID"] = pProxy->GetLookupID();
			LookupRel["TRACE"] = Trace;
			
			if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
				LogLine(LOG_DEBUG, L"Relaying 'Proxy Response' from %s to %s", pNode->GetID().ToHex().c_str(), pProxy->GetReturnNode()->GetID().ToHex().c_str());
			pProxy->GetReturnChannel()->QueuePacket(KAD_PROXY_RESPONSE, LookupRel);
		}
		else
			pLookup->RecivedTraceResults(Trace);
	}

	if(LookupRes.Has("TRACE")) // if this is set its a pro forma response ignore it
		return;

	pLookup->ProxyingResponse(pNode, pChannel, LookupRes.Get("ERR")); 
}

UINT CKadHandler::SendCodeReq(CKadNode* pNode, CComChannel* pChannel, const CVariant& CID)
{
	CVariant CodeReq;
	CodeReq["CID"] = CID;
	
	if(UINT uID = pChannel->QueuePacket(KAD_CODE_REQUEST, CodeReq))
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Sending 'Code Resuest' to %s", pNode->GetID().ToHex().c_str());
		return uID;
	}
	return 0;
}

void CKadHandler::HandleCodeReq(const CVariant& LookupReq, CKadNode* pNode, CComChannel* pChannel)
{
	CKadScript* pKadScript = GetKadScript(LookupReq);
	if(!pKadScript)
	{
		pChannel->Close();
		return; // fail silently this should not happen - the remote side should only request code we asked for and thos one we must have ourselvs
	}

	CVariant LookupRes;
	LookupRes["CID"] = pKadScript->GetCodeID();
	if(pKadScript->IsAuthenticated())
		LookupRes["AUTH"] = pKadScript->GetAuthentication();
	LookupRes["SRC"] = pKadScript->GetSource();

	if(UINT uID = pChannel->QueuePacket(KAD_CODE_RESPONSE, LookupRes))
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Sending 'Code Response' to %s", pNode->GetID().ToHex().c_str());
	}
}

void CKadHandler::HandleCodeRes(const CVariant& LookupRes, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Recived 'Code Response' from %s", pNode->GetID().ToHex().c_str());

	SKadData* pData = pChannel->GetData<SKadData>();

	string Error = GetParent<CKademlia>()->Engine()->Install(LookupRes);
	if(Error.empty())
	{		
		for(list<pair<string, CVariant> >::iterator I = pData->DelayedPackets.begin(); I != pData->DelayedPackets.end(); I = pData->DelayedPackets.erase(I))
		{
			if(I->first.compare(KAD_PROXY_REQUEST) == 0)		HandleProxyReq(I->second, pNode, pChannel, true);
			else if(I->first.compare(KAD_EXECUTE_REQUEST) == 0)	HandleExecuteReq(I->second, pNode, pChannel, true);
		}
	}
	else // if we failed instalation still send a proper reply to delayed packets
	{
		for(list<pair<string, CVariant> >::iterator I = pData->DelayedPackets.begin(); I != pData->DelayedPackets.end(); I = pData->DelayedPackets.erase(I))
		{
			CVariant LookupAux;
			LookupAux["ERR"] = Error;

			if(I->first.compare(KAD_PROXY_REQUEST) == 0)
			{
				if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
					LogLine(LOG_DEBUG, L"Sending 'Proxy Response' to %s", pNode->GetID().ToHex().c_str());
				pChannel->QueuePacket(KAD_PROXY_RESPONSE, LookupAux);
			}
			else if(I->first.compare(KAD_EXECUTE_REQUEST) == 0)
			{
				if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
					LogLine(LOG_DEBUG, L"Sending 'Execute Response' to %s", pNode->GetID().ToHex().c_str());
				pChannel->QueuePacket(KAD_EXECUTE_RESPONSE, LookupAux);
			}
		}
	}
}

///////////////////////
// Messaging

UINT CKadHandler::SendMessagePkt(CKadNode* pNode, CComChannel* pChannel, const string& Name, const CVariant& Data)
{
	CVariant Packet;
	Packet["NAME"] = Name;
	Packet["DATA"] = Data;
	
	if(UINT uID = pChannel->QueuePacket(KAD_LOOKUP_MESSAGE, Packet))
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Sending 'Lookup Message' to %s", pNode->GetID().ToHex().c_str());
		return uID;
	}
	return 0;
}

void CKadHandler::HandleMessagePkt(const CVariant& Packet, CKadNode* pNode, CComChannel* pChannel)
{
	string Name = Packet["NAME"];
	const CVariant& Data = Packet["DATA"];

	SKadData* pData = pChannel->GetData<SKadData>();
	CPointer<CKadOperation> pLookup = pData->pLookup->Cast<CLookupProxy>();
	if(pLookup)
		pLookup->ProcessMessage(Name, Data, pNode, pChannel);
}

///////////////////////
// Call

UINT CKadHandler::SendExecuteReq(CKadNode* pNode, CComChannel* pChannel, CKadOperation* pLookup, const CVariant& Requests, bool bStateless)
{
	CVariant ExecuteReq;

	// Stateless Mode
	if(bStateless)
	{
		ExecuteReq["TID"] = pLookup->GetID();
		SetKadScript(pLookup, ExecuteReq);
	}

	ExecuteReq["RUN"] = Requests;

	if(UINT uID = pChannel->QueuePacket(KAD_EXECUTE_REQUEST, ExecuteReq))
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Sending 'Execute Resuest' to %s", pNode->GetID().ToHex().c_str());
		return uID;
	}
	return 0;
}

class CReportProxy: public CReportLogger
{
public:
	CReportProxy(CLookupProxy* pProxy) {m_pProxy = pProxy;}

	virtual void			LogReport(UINT Flags, const wstring& ErrorMessage, const string& Error = ""){
		m_pProxy->LogReport(Flags, ErrorMessage, Error);
	}

protected:
	CLookupProxy*	m_pProxy;
};

class CReportSender: public CReportLogger
{
public:
	CReportSender(CKadHandler* pHandler, CComChannel* pChannel) {m_pHandler = pHandler; m_pChannel = pChannel;}
	virtual void			LogReport(UINT Flags, const wstring& ErrorMessage, const string& Error = ""){
		m_pHandler->SendReportPkt(m_pChannel, Flags, ErrorMessage, Error);
	}

protected:
	CKadHandler*	m_pHandler;
	CComChannel*	m_pChannel;
};

void CKadHandler::HandleExecuteReq(const CVariant& ExecuteReq, CKadNode* pNode, CComChannel* pChannel, bool bDelayed)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Recived 'Execute Resuest' from %s", pNode->GetID().ToHex().c_str());

	CVariant ExecuteRes;

	SKadData* pData = pChannel->GetData<SKadData>();
	CPointer<CLookupProxy> pProxy = pData->pLookup->Cast<CLookupProxy>();
	if(pProxy)
	{
		// Note: in countrary to load/store this may actualy inttercept requests
		CVariant Results = pProxy->AddCallReq(ExecuteReq["RUN"]); // the to be executed requests are copyed to pOperator->GetRequests()

		CKadOperator* pOperator = pProxy->GetOperator();
		CKadScript* pKadScript = pOperator ? pOperator->GetScript() : GetParent<CKademlia>()->Engine()->GetScript(pProxy->GetCodeID());
		if(pKadScript)
		{
			if(pProxy->IsInRange()) // if we are in range let the local index evaluate the request as well
			{
				CReportProxy ReportProxy(pProxy);

				if(pOperator)
					ExecuteRes["RET"] = pKadScript->Execute(pOperator->GetRequests(), pProxy->GetID(), &ReportProxy);
				else // if no operator is present we use the original request list as nothing could have been intercepted
					ExecuteRes["RET"] = pKadScript->Execute(ExecuteReq["RUN"], pProxy->GetID(), &ReportProxy);

				// we check and filter our own results
				ExecuteRes = pProxy->AddCallRes(ExecuteRes, NULL); // report own results back to the script and filter them if needed

				ExecuteRes["RET"].Merge(Results);
			}
			else
				ExecuteRes["RET"] = Results;

			if(ExecuteRes["RET"].Count() == 0)
				return; // dont send back empty replys - we may eider be still out of range or have filtered everything
		}
		else
			ExecuteRes["ERR"] = "NoCode";
	}
	else // Stateless Mode
	{
		if(!ExecuteReq.Has("TID"))
			throw CException(LOG_ERROR, L"Invalid Execute Request");

		CUInt128 uTargetID = ExecuteReq["TID"];
		//if((uTargetID ^ GetParent<CKademlia>()->Root()->GetID()) > GetParent<CKademlia>()->Root()->GetMaxDistance())
		//	ExecuteRes["ERR"] = "ToFarAway";
		//else
		{
			CKadScript* pKadScript = GetKadScript(ExecuteReq);
			// If we dont have the desired script hold the packet back and request the script
			if(!pKadScript)
			{
				ASSERT(!bDelayed);
				if(!bDelayed)
				{	
					SendCodeReq(pNode, pChannel, ExecuteReq["CID"]);
					pData->DelayedPackets.push_back(make_pair(KAD_EXECUTE_REQUEST, ExecuteReq));
				}
				return;
			}
			ExecuteRes["VER"] = pKadScript->GetVersion();

			CReportSender ReportSender(this, pChannel);

			ExecuteRes["RET"] = pKadScript->Execute(ExecuteReq["RUN"], uTargetID, &ReportSender);
		}
	}

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Sending 'Execute Response' to %s", pNode->GetID().ToHex().c_str());
	pChannel->QueuePacket(KAD_EXECUTE_RESPONSE, ExecuteRes);
}

bool CKadHandler::SendExecuteRes(CLookupProxy* pProxy, const CVariant& Results)
{
	CVariant ExecuteRes;

	ExecuteRes["RET"] = Results;
	
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Sending Custom 'Execute Response' to %s", pProxy->GetReturnNode()->GetID().ToHex().c_str());
	return pProxy->GetReturnChannel()->QueuePacket(KAD_EXECUTE_RESPONSE, ExecuteRes);
}

void CKadHandler::HandleExecuteRes(const CVariant& ExecuteRes, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Recived 'Execute Response' from %s", pNode->GetID().ToHex().c_str());

	SKadData* pData = pChannel->GetData<SKadData>();
	if(!pData->pLookup)
		return;
	CPointer<CKadOperation> pLookup = pData->pLookup->Cast<CKadOperation>();
	if(!pLookup)
		throw CException(LOG_ERROR, L"Recived Execute Response for a lookup that is not a CKadOperation");

	if(ExecuteRes.Has("ERR"))
	{
		pLookup->HandleError(pNode);
		LogLine(LOG_DEBUG | LOG_ERROR, L"lookup call error: %s", ExecuteRes["ERR"].To<wstring>().c_str());
		if(CLookupProxy* pProxy = pLookup->Cast<CLookupProxy>())
			SendReportPkt(pProxy->GetReturnChannel(), LOG_ERROR, L"Execute Error", ExecuteRes["ERR"], pProxy->IsTraced() ? pNode->GetID() : CUInt128());
		return;
	}

	//ExecuteRes["VER"] // only with stataless calls
	CVariant ExecuteRel = pLookup->AddCallRes(ExecuteRes, pNode);

	// Relay the results back to the requesting client
	if(CLookupProxy* pProxy = pLookup->Cast<CLookupProxy>())
	{
		if(ExecuteRel["RET"].Count() == 0)
			return; // dont send back empty replys - we may filter everything

		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Relaying 'Execute Response' from %s to %s", pNode->GetID().ToHex().c_str(), pProxy->GetReturnNode()->GetID().ToHex().c_str());
		pProxy->GetReturnChannel()->QueuePacket(KAD_EXECUTE_RESPONSE, ExecuteRel);
	}
}

///////////////////////
// Store

UINT CKadHandler::SendStoreReq(CKadNode* pNode, CComChannel* pChannel, CKadOperation* pLookup, const CVariant& Payload, bool bStateless)
{
	CVariant StoreReq;

	// Stateless Mode
	if(bStateless)
	{
		StoreReq["TID"] = pLookup->GetID();
	
		CVariant AK = pLookup->GetAccessKey();
		if(AK.IsValid())
			StoreReq["AK"] = AK;
	}

	if(uint32 StoreTTL = pLookup->GetStoreTTL())
		StoreReq["TTL"] = StoreTTL;

	StoreReq["REQ"] = Payload;

	if(UINT uID = pChannel->QueuePacket(KAD_STORE_REQUEST, StoreReq))
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Sending 'Store Resuest' to %s", pNode->GetID().ToHex().c_str());
		return uID;
	}
	return 0;
}

void CKadHandler::HandleStoreReq(const CVariant& StoreReq, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Recived 'Store Resuest' from %s", pNode->GetID().ToHex().c_str());

	uint32 TTL = StoreReq.Get("TTL");

	CVariant StoreRes(CVariant::EMap);

	SKadData* pData = pChannel->GetData<SKadData>();
	CPointer<CLookupProxy> pProxy = pData->pLookup->Cast<CLookupProxy>();
	if(pProxy)
	{
		pProxy->SetStoreTTL(TTL);
		pProxy->AddStoreReq(StoreReq["REQ"]);

		if(pProxy->IsInRange())
		{
			StoreRes = GetParent<CKademlia>()->Index()->DoStore(pProxy->GetID(), StoreReq, TTL, pProxy->GetAccessKey());
			StoreRes = pProxy->AddStoreRes(StoreRes, NULL);
		}
		else // if its a recursive operation its ok no error
			return; // dont send back empty replys
	}
	else // Stateless Mode
	{
		if(!StoreReq.Has("TID"))
			throw CException(LOG_ERROR, L"Invalid Store Request");

		CUInt128 uTargetID = StoreReq["TID"];
		//if((uTargetID ^ GetParent<CKademlia>()->Root()->GetID()) > GetParent<CKademlia>()->Root()->GetMaxDistance())
		//	StoreRes["ERR"] = "ToFarAway";
		//else
			StoreRes = GetParent<CKademlia>()->Index()->DoStore(uTargetID, StoreReq, TTL, StoreReq.Get("AK"));
	}

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Sending 'Store Response' to %s", pNode->GetID().ToHex().c_str());
	pChannel->QueuePacket(KAD_STORE_RESPONSE, StoreRes);
}

void CKadHandler::HandleStoreRes(const CVariant& StoreRes, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Recived 'Store Response' from %s", pNode->GetID().ToHex().c_str());

	SKadData* pData = pChannel->GetData<SKadData>();
	if(!pData->pLookup)
		return;
	CPointer<CKadOperation> pLookup = pData->pLookup->Cast<CKadOperation>();
	if(!pLookup)
		throw CException(LOG_ERROR, L"Recived Store Response for a lookup that is not a CKadOperation");

	if(StoreRes.Has("ERR"))
	{
		pLookup->HandleError(pNode);
		LogLine(LOG_DEBUG | LOG_ERROR, L"lookup store error: %s", StoreRes["ERR"].To<wstring>().c_str());
		if(CLookupProxy* pProxy = pLookup->Cast<CLookupProxy>())
			SendReportPkt(pProxy->GetReturnChannel(), LOG_ERROR, L"Store Error", StoreRes["ERR"], pProxy->IsTraced() ? pNode->GetID() : CUInt128());
		return;
	}

	CVariant StoreRel = pLookup->AddStoreRes(StoreRes, pNode);

	// Relay the results back to the requesting client
	if(CLookupProxy* pProxy = pLookup->Cast<CLookupProxy>())
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Relaying 'Store Response' from %s to %s", pNode->GetID().ToHex().c_str(), pProxy->GetReturnNode()->GetID().ToHex().c_str());
		pProxy->GetReturnChannel()->QueuePacket(KAD_STORE_RESPONSE, StoreRel);
	}
}

///////////////////////
// Load

UINT CKadHandler::SendLoadReq(CKadNode* pNode, CComChannel* pChannel, CKadOperation* pLookup, const CVariant& Request, bool bStateless)
{
	CVariant LoadReq;

	// Stateless Mode
	if(bStateless)
	{
		LoadReq["TID"] = pLookup->GetID();
	}

	if(uint32 LoadCount = pLookup->GetLoadCount())
		LoadReq["CNT"] = LoadCount;

	LoadReq["REQ"] = Request;

	if(UINT uID = pChannel->QueuePacket(KAD_LOAD_REQUEST, LoadReq))
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Sending 'Load Resuest' to %s", pNode->GetID().ToHex().c_str());
		return uID;
	}
	return 0;
}

void CKadHandler::HandleLoadReq(const CVariant& LoadReq, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Recived 'Load Resuest' from %s", pNode->GetID().ToHex().c_str());

	uint32 Count = LoadReq.Get("CNT");

	CVariant LoadRes(CVariant::EMap);

	SKadData* pData = pChannel->GetData<SKadData>();
	CPointer<CLookupProxy> pProxy = pData->pLookup->Cast<CLookupProxy>();
	if(pProxy)
	{
		pProxy->SetLoadCount(Count);
		pProxy->AddLoadReq(LoadReq["REQ"]); // as this is signed we can only repalce the ols one

		if(pProxy->IsInRange())
		{
			LoadRes = GetParent<CKademlia>()->Index()->DoLoad(pProxy->GetID(), LoadReq, Count);
			LoadRes = pProxy->AddLoadRes(LoadRes, NULL);
		}
		else // if its a recursive operation its ok no error
			 return; // dont send back empty replys
	}
	else // Stateless Mode
	{
		if(!LoadReq.Has("TID"))
			throw CException(LOG_ERROR, L"Invalid Load Request");

		CUInt128 uTargetID = LoadReq["TID"];
		//if((uTargetID ^ GetParent<CKademlia>()->Root()->GetID()) > GetParent<CKademlia>()->Root()->GetMaxDistance())
		//	LoadRes["ERR"] = "ToFarAway";
		//else
			LoadRes = GetParent<CKademlia>()->Index()->DoLoad(uTargetID, LoadReq, Count);
	}

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Sending 'Load Response' to %s", pNode->GetID().ToHex().c_str());
	pChannel->QueuePacket(KAD_LOAD_RESPONSE, LoadRes);
}

void CKadHandler::HandleLoadRes(const CVariant& LoadRes, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
		LogLine(LOG_DEBUG, L"Recived 'Load Response' from %s", pNode->GetID().ToHex().c_str());

	SKadData* pData = pChannel->GetData<SKadData>();
	if(!pData->pLookup)
		return;
	CPointer<CKadOperation> pLookup = pData->pLookup->Cast<CKadOperation>();
	if(!pLookup)
		throw CException(LOG_ERROR, L"Recived Load Response for a lookup that is not a CKadOperation");

	if(LoadRes.Has("ERR"))
	{
		pLookup->HandleError(pNode);
		LogLine(LOG_DEBUG | LOG_ERROR, L"lookup load error: %s", LoadRes["ERR"].To<wstring>().c_str());
		if(CLookupProxy* pProxy = pLookup->Cast<CLookupProxy>())
			SendReportPkt(pProxy->GetReturnChannel(), LOG_ERROR, L"Load Error", LoadRes["ERR"], pProxy->IsTraced() ? pNode->GetID() : CUInt128());
		return;
	}

	CVariant LoadRel = pLookup->AddLoadRes(LoadRes, pNode);

	// Relay the results back to the requesting client
	if(CLookupProxy* pProxy = pLookup->Cast<CLookupProxy>())
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugLU"))
			LogLine(LOG_DEBUG, L"Relaying 'Load Response' from %s to %s", pNode->GetID().ToHex().c_str(), pProxy->GetReturnNode()->GetID().ToHex().c_str());
		pProxy->GetReturnChannel()->QueuePacket(KAD_LOAD_RESPONSE, LoadRel);
	}
}

///////////////////////
// Debugging

void CKadHandler::SendReportPkt(CComChannel* pChannel, UINT Flags, const wstring& Line, const string& Error, const CUInt128& NID)
{
	CVariant Packet;
	if(NID != 0)
		Packet["TRACE"].Append(NID);

	Packet["FLAG"] = (uint8)Flags;
	Packet["TEXT"] = Line;
	if(!Error.empty())
		Packet["ERR"] = Error;

	pChannel->QueuePacket(KAD_LOOKUP_REPORT, Packet);
}

void CKadHandler::HandleReportPkt(const CVariant& Packet, CKadNode* pNode, CComChannel* pChannel)
{
	SKadData* pData = pChannel->GetData<SKadData>();

	CVariant Trace = Packet.Get("TRACE").Clone();
	Trace.Append(pNode->GetID());

	CPointer<CKadOperation> pLookup = pData->pLookup->Cast<CKadOperation>();
	if(pLookup)
		pLookup->LogReport(Packet["FLAG"], Packet["TEXT"], Packet.Get("ERR"), Trace);

	// Relay the report back to the requesting client
	if(CLookupProxy* pProxy = pLookup->Cast<CLookupProxy>())
	{
		CVariant PacketRel = Packet.Clone();
		if(pProxy->IsTraced())
			PacketRel["TRACE"] = Trace;

		pProxy->GetReturnChannel()->QueuePacket(KAD_LOOKUP_REPORT, PacketRel);
	}
}
