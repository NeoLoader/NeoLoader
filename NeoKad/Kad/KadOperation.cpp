#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadOperation.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "RoutingRoot.h"
#include "KadEngine/KadEngine.h"
#include "KadEngine/KadScript.h"
#include "KadEngine/KadOperator.h"
#include "KadNode.h"
#include "KadHandler.h"

IMPLEMENT_OBJECT(CKadOperation, CKadLookup)

CKadOperation::CKadOperation(const CUInt128& ID, CObject* pParent)
 : CKadLookup(ID, pParent) 
{
	m_BrancheCount = 0;
	m_SpreadCount = GetParent<CKademlia>()->Cfg()->GetInt("SpreadCount");
	m_SpreadShare = 0;

	m_ActiveCount = 0;
	m_FailedCount = 0;

	m_pOperator = NULL;

	m_InRange = (ID ^ GetParent<CKademlia>()->Root()->GetID()) <= GetParent<CKademlia>()->Root()->GetMaxDistance();
	m_OutOfNodes = false;
	m_ManualMode = false;

	m_StoreTTL = 0;
	m_LoadCount = 0;

	m_TotalDoneJobs = 0;
	m_TotalReplys = 0;
}

void CKadOperation::SetSpreadCount(int SpreadCount)
{
	m_SpreadCount = Min(SpreadCount, GetParent<CKademlia>()->Cfg()->GetInt64("MaxSpreadCount"));
}

int CKadOperation::GetNeededCount()
{
	return Min(CKadLookup::GetNeededCount(), m_BrancheCount);
}

void CKadOperation::InitOperation()
{
	if(!m_SpreadShare) // that should only be the case for CKadTasks
		m_SpreadShare = m_SpreadCount;

	if(m_JumpCount > 0)
		m_BrancheCount = ((rand() % 4) == 0) ? 1 : 2;
	else if(m_HopLimit > 1)
	{
		switch(rand() % 10)
		{
		case 0: 
		case 1:		m_BrancheCount = 1; break;
		case 2:	
		case 3:	
		case 4:		m_BrancheCount = 3; break;
		default:	m_BrancheCount = 2;
		}
	}
	else if(m_HopLimit == 1)
		m_BrancheCount = GetSpreadShare();

	if(!m_ManualMode)
	{
		// allow to start node lookup only if we have actual calls to perform, as the last hop is always stateless
		if(m_HopLimit == 1 && (CountCalls() || CountStores() || CountLoads())) 
		{
			ASSERT(m_JumpCount == 0);
			m_LookupState = eLookupActive;
		}
	}
}

void CKadOperation::Start()
{
	LogReport(LOG_INFO, L"Starting Operation");

	if(m_pOperator && m_pOperator->IsValid())
	{
		try
		{
			m_InitParam = m_pOperator->Init(m_InitParam); // this could effectivly update spreading/branching options
		}
		catch(const CJSException& Exception)
		{
			LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
		}
	}

	CKadLookup::Start();

	if(m_pOperator && m_pOperator->IsValid())
		m_pOperator->OnStart();
}

void CKadOperation::LogReport(UINT Flags, const wstring& ErrorMessage, const string& Error, const CVariant& Trace)
{
	wstring Operation = L"Kad Operation";
	if(m_pOperator)
	{
		CKadScript* pKadScript = m_pOperator->GetScript();
		ASSERT(pKadScript);

		Operation += L" (" + pKadScript->GetName() + L" v" + CKadScript::GetVersion(pKadScript->GetVersion()) + L")";
	}
	
	if(Trace.Count() > 0) // remote result
	{
		wstring Sender = CUInt128(Trace.At((uint32)0)).ToHex();
		if(!Error.empty())
			LogLine(LOG_DEBUG | Flags, L"%s recived an Error: %s from %s", Operation.c_str(), ErrorMessage.c_str(), Sender.c_str());
		else
			LogLine(LOG_DEBUG | Flags, L"%s recived a Report: %s from %s", Operation.c_str(), ErrorMessage.c_str(), Sender.c_str());
	}
	else
	{
		if(!Error.empty())
			LogLine(LOG_DEBUG | Flags, L"%s caused an Error: %s", Operation.c_str(), ErrorMessage.c_str());
		else
			LogLine(LOG_DEBUG | Flags, L"%s Reports: %s", Operation.c_str(), ErrorMessage.c_str());
	}
}

void CKadOperation::PrepareStop()
{
	ASSERT(m_StopTime == -1);

	if(m_pOperator && m_pOperator->IsValid())
	{
		try
		{
			m_pOperator->Finish();
		}
		catch(const CJSException& Exception)
		{
			LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
		}
	}

	CKadLookup::PrepareStop();
}

template<class T>
void CountDone(const T &Map, int &Done, int &Count)
{
    for(typename T::const_iterator J = Map.begin(); J != Map.end(); J++){
		if(J->second.Status.Done)
			Done++;
		Count += J->second.Status.Results;
	}
}

void CKadOperation::Process(UINT Tick)
{
	bool bLookupActive = m_LookupState == eLookupActive;

	CKadLookup::Process(Tick); // this looks for more nodes

	if(m_pOperator && m_pOperator->IsValid())
	{
		m_pOperator->RunTimers();

		if(bLookupActive && m_LookupState != eLookupActive)
			m_pOperator->OnClosestNodes();
	}

	if((Tick & E10PerSec) == 0)
		return;

	if(m_LookupState == eLookupActive)
		return; // we are looking for closer nodes right now - just wait a second
	

	// Note: the kad operation mode for this kind of lookups is complicated, there are 3 modes of operation
	//
	//		m_JumpCount > 0
	//	1. Jumping, where we open channels to random nodes and request proxying
	//
	//		m_HopLimit > 1
	//	2. Routing, where we open channels to propabalistically choosen closest nodes
	//				The propabalistically choice is based on the total desired spread area
	//				If we hit a node that is already handling this lookup we need to choose a new one
	//
	//		m_HopLimit == 1
	//	3. Itterative Mode, here we do a normal lookup for closest nodes and than talk to them, not using proxying
	//		
	//
	//		m_HopLimit == 0
	//	4. Target for a operations and messaging, not doing any own spreading
	//

	if(m_HopLimit == 0 || (m_HopLimit == 1 && !(CountCalls() || CountStores() || CountLoads())))
		return; // we are not alowed to do anything - hoop 0 or hoop 1 and we have no hopable jobs just messages

	int FailedCount = 0;
	// Note: if we are in range we will act as one successfull share
	int UsedShares = m_InRange ? 1 : 0;
	int ShareHolders = 0;
	for(TNodeStatusMap::iterator I = m_Nodes.begin(); I != m_Nodes.end(); I++)
	{
		SOpProgress* pProgress = (SOpProgress*)I->second;
		if(pProgress->OpState == eOpFailed)
		{
			FailedCount++;
			continue;
		}
		if(pProgress->Shares == 0)
			continue;
		
		if(!I->first.pChannel)
		{
			ASSERT(0);
			continue;
		}

		UsedShares += pProgress->Shares;
		ShareHolders++;

		bool bStateless;
		if(!((bStateless = (pProgress->OpState == eOpStateless)) || pProgress->OpState == eOpProxy))
			continue;

		// send newly added requests if there are any
		if (pProgress->Calls.size() != CountCalls()
		 || pProgress->Stores.size() != CountStores()
		 || pProgress->Loads.size() != CountLoads())
		{
			SendRequests(I->first.pNode, I->first.pChannel, pProgress, bStateless);
		}
	}
	int FreeShares = GetSpreadShare() - UsedShares;
	//ASSERT(FreeShares >= 0);
	m_ActiveCount = ShareHolders;
	m_FailedCount = FailedCount;


	int TotalDoneJobs = 0;
	int TotalReplys = 0;
	if(m_pOperator)
		CountDone(m_pOperator->GetRequests(), TotalDoneJobs, TotalReplys);
	else
		CountDone(m_CallMap, TotalDoneJobs, TotalReplys);
	CountDone(m_StoreMap, TotalDoneJobs, TotalReplys);
	CountDone(m_LoadMap, TotalDoneJobs, TotalReplys);
	m_TotalDoneJobs = TotalDoneJobs;
	m_TotalReplys = TotalReplys;


	if(m_ManualMode)
		return;

	// Note: if we are in Jump Mode we prep the iterator to give uss randome nodes
	//		If we are in Iterative Mode we prep the iterator to give us the closest node
	//		If we are in Routong Mode it complicated, we want a random node from the m_SpreadCount closest nodes
	CRoutingZone::SIterator Iter(m_JumpCount > 0 ? 0 : (m_HopLimit > 1 ? m_SpreadCount : 1));
	for(CKadNode* pNode = NULL; FreeShares > 0 && (pNode = GetParent<CKademlia>()->Root()->GetClosestNode(Iter, m_ID)) != NULL;)
	{
		if(m_JumpCount == 0 && ((pNode->GetID() ^ m_ID) > GetParent<CKademlia>()->Root()->GetMaxDistance()))
			break; // there are no nodes anymore in list that would be elegable here
		
		CComChannel* pChannel = GetChannel(pNode);
		if(!pChannel)
			continue;

		SOpProgress* pProgress = GetProgress(pNode);
		ASSERT(pProgress); // right after get channel this must work
		if(pProgress->OpState != eNoOp)
			continue; // this node has already been tryed

		if(m_HopLimit > 1)
			RequestProxying(pNode, pChannel, pProgress, FreeShares, ShareHolders, m_InitParam);
		else
			RequestStateless(pNode, pChannel, pProgress);

		if(pProgress->Shares)
		{
			FreeShares -= pProgress->Shares;
			ShareHolders ++;
		}
	}

	// if there are shares left we are out of nodes
	m_OutOfNodes = (FreeShares > 0);
}

void CKadOperation::NodeStalling(CKadNode* pNode)
{
	if(SOpProgress* pProgress = GetProgress(pNode))
		pProgress->Shares = 0;
}

bool CKadOperation::OnCloserNodes(CKadNode* pNode, CComChannel* pChannel, int CloserCount)
{
	if(!CKadLookup::OnCloserNodes(pNode, pChannel, CloserCount))
		return false;

	if(m_ManualMode)
		return true;

	// Note: if we are in here it means we are operating in itterative mode that means m_HopLimit is 1 and we always send the requests out in stateless mode
	ASSERT(m_HopLimit == 1);

	SOpProgress* pProgress = GetProgress(pNode);
	if(!pProgress || pProgress->OpState != eNoOp) // error or already used
		return true;
	ASSERT(pProgress->Shares == 0);

	if(CountCalls() || CountStores() || CountLoads())
		RequestStateless(pNode, pChannel, pProgress);
	return true;
}

bool CKadOperation::AddNode(CKadNode* pNode, bool bStateless, const CVariant &InitParam, int Shares)
{
	CComChannel* pChannel = GetChannel(pNode);
	if(!pChannel)
		return false;

	SOpProgress* pProgress = GetProgress(pNode);
	if(!pProgress || pProgress->OpState != eNoOp) // error or already used
		return false;
	ASSERT(pProgress->Shares == 0);

	if(bStateless)
	{
		if(!(CountCalls() || CountStores() || CountLoads()))
		{
			LogReport(LOG_WARNING, L"Can not add a node in stateles mode if no requests are queued");
			return false;
		}
		RequestStateless(pNode, pChannel, pProgress);
		return true;
	}
	
	if(Shares != 0)
		return RequestProxying(pNode, pChannel, pProgress, Shares, InitParam);

	// Note: if we are in range we will act as one successfull share
	int UsedShares = m_InRange ? 1 : 0;
	int ShareHolders = 0;
	for(TNodeStatusMap::iterator I = m_Nodes.begin(); I != m_Nodes.end(); I++)
	{
		SOpProgress* pProgress = (SOpProgress*)I->second;
		if(pProgress->Shares == 0)
			continue;
			
		UsedShares += pProgress->Shares;
		ShareHolders++;
	}
	int FreeShares = GetSpreadShare() - UsedShares;
	ASSERT(FreeShares >= 0);

	if(FreeShares < 1)
	{
		LogReport(LOG_WARNING, L"Can not add a proxy node, no free shares available");
		return false;
	}

	return RequestProxying(pNode, pChannel, pProgress, FreeShares, ShareHolders, InitParam);
}

void CKadOperation::ChannelClosed(CKadNode* pNode)
{
	if(SOpProgress* pProgress = GetProgress(pNode))
	{
		pProgress->Shares = 0;
		pProgress->OpState = eOpFailed;
	}
}

void CKadOperation::RequestStateless(CKadNode* pNode, CComChannel* pChannel, SOpProgress* pProgress)
{
	pProgress->OpState = eOpStateless;
	pProgress->Shares = 1;

	if(m_pOperator)
		m_pOperator->AddNode(pNode);

	SendRequests(pNode, pChannel, pProgress, true);
}

bool CKadOperation::RequestProxying(CKadNode* pNode, CComChannel* pChannel, SOpProgress* pProgress, int FreeShares, int ShareHolders, const CVariant &InitParam)
{
	if(pProgress->uTimeOut != -1) // error or node already in use
		return false;

	int Shares;
	if(m_BrancheCount > ShareHolders + 1)
	{
		int Pieces = m_BrancheCount - ShareHolders; // >= 2
		// tha maximum amount of shares that can be done to one client such that there will ne somethign left for others
		int MaxShares = FreeShares - (Pieces - 1); 
		Shares = MaxShares < 2 ? 1 : 1 + (rand() % (MaxShares - 1));
	}
	else // we need to spend all shares to avoind to much branching
	{
		ASSERT(m_BrancheCount == ShareHolders + 1);
		Shares = FreeShares;
	}

	return RequestProxying(pNode, pChannel, pProgress, Shares, InitParam);
}

bool CKadOperation::RequestProxying(CKadNode* pNode, CComChannel* pChannel, SOpProgress* pProgress, int Shares, const CVariant &InitParam)
{
	if(GetParent<CKademlia>()->Handler()->SendProxyReq(pNode, pChannel, this, Shares, InitParam) == 0)
		return false;

	pProgress->OpState = eOpPending;
	pProgress->Shares = Shares;
	pProgress->uTimeOut = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("RequestTimeOut"));

	if(m_pOperator)
		m_pOperator->AddNode(pNode);

	if(m_LookupHistory)
		m_LookupHistory->ExpectResponse(pNode->GetID());
	return true;
}

template<class T>
bool TestDone(const T &Map)
{
    for(typename T::const_iterator J = Map.begin(); J != Map.end(); J++){
		if(!J->second.Done)
			return false;
	}
	return true;
}

/*template<class T>
bool CheckDone(const T &Map)
{
	for(T::const_iterator J = Map.begin(); J != Map.end(); J++){
		if(!J->second.second.Done)
			return false;
	}
	return true;
}
*/

bool CKadOperation::HasFoundEnough()
{
	if(m_ManualMode || (m_CallMap.empty() && m_StoreMap.empty() && m_LoadMap.empty()))
		return false; // we re not looking for automatic results, this is a custom message based lokup operation


	if(m_LookupState == eLookupActive)
		return false; // we are still looking for nodes


	// Check if we are waiting for any incomming results:
	for(TNodeStatusMap::iterator I = m_Nodes.begin(); I != m_Nodes.end(); I++)
	{
		SOpProgress* pProgress = (SOpProgress*)I->second;
		if(pProgress->Shares == 0)
			continue;

		// check if all tasks have ben delt
		if(pProgress->Calls.size() != CountCalls()
		 || pProgress->Stores.size() != CountStores()
		 || pProgress->Loads.size() != CountLoads())
			return false; // yes we are - we dont even send out all yet

		// check if all delt tasks have been completed
		if(!TestDone(pProgress->Calls))
			return false;
		if(!TestDone(pProgress->Stores))
			return false;
		if(!TestDone(pProgress->Loads))
			return false;
	}


	// if there are nor more nodes to ask it must be enough
	if(m_OutOfNodes || m_HopLimit == 0) 
		return true; // there isn't more period
	

	/*// we check if we have got a sufficient amount of results
	if(m_pOperator)
	{
		// Note: we dont check if all tasks we ware asked to do are done, we only check if al tasked we wanted to do are done
		//			OnFinish we are supposed to send definitive answers to all task we ware askedto do
		if(!CheckDone(m_pOperator->GetRequests()))
			return false;
	}
	else
	{
		if(!CheckDone(m_CallMap))
			return false;
	}
	if(!CheckDone(m_StoreMap))
		return false;
	if(!CheckDone(m_LoadMap))
		return false;
	return true;*/
	return m_TotalDoneJobs >= CountCalls() + CountStores() + CountLoads();
}

bool CKadOperation::ReadyToStop()
{
	if(m_pOperator)
	{
		// we check if the script got terminated in the mean time
		if(!m_pOperator->IsValid())
		{
			LogReport(LOG_ERROR, L"Script has been unexpectedly terminated", "Terminated");
			return true;
		}
	}

	bool bFoundEnough = HasFoundEnough();
	bool bTimedOut = HasTimedOut();
	if(!bTimedOut && !bFoundEnough)
		return false;

	// If we have an operator we ask him if its ok to finish now
	if(m_pOperator && m_pOperator->IsValid())
	{
		bool bForceFinish = GetDuration() > SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt64("MaxLookupTimeout")); // if true than timeout can not be canceled
		if(!m_pOperator->OnFinish(bForceFinish, bTimedOut, bFoundEnough) && !bForceFinish)
			return false;
	}
	return true;
}

void CKadOperation::Stop()
{
	CKadLookup::Stop();

	LogReport(LOG_INFO, L"Stopped Operation");
}

void CKadOperation::ProcessMessage(const string& Name, const CVariant& Data, CKadNode* pNode, CComChannel* pChannel)
{
	if(m_pOperator && m_pOperator->IsValid())
		m_pOperator->ProcessMessage(Name, Data, pNode);
}

bool CKadOperation::SendMessage(const string& Name, const CVariant& Data, CKadNode* pNode)
{
	if(SOpProgress* pProgress = GetProgress(pNode))
	{
		if(pProgress && pProgress->OpState == eOpProxy) // we can not send messages to stateless nodes
		{
			CComChannel* pChannel = GetChannel(pNode);
			return GetParent<CKademlia>()->Handler()->SendMessagePkt(pNode, pChannel, Name, Data) != 0;
		}
	}
	return false;
}

int CKadOperation::CountCalls()
{
	if(m_pOperator)
		return m_pOperator->CountRequests();
	else
		return m_CallMap.size();
}

string CKadOperation::SetupScript(CKadScript* pKadScript)
{
	m_CodeID = pKadScript->GetCodeID();
	try
	{
		CJSScript* pJSScript = pKadScript->GetJSScript(true); // this may throw a CJSException if instantiation fails
		m_pOperator = new CKadOperator(pKadScript, pJSScript, this);
	}
	catch(const CJSException& Exception)
	{
		LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
		return Exception.GetError();
	}
	return "";
}

void CKadOperation::HandleError(CKadNode* pNode)
{
	// Note: this function is called when we get Results, StoreRes or LoadRes with an fatal error
	//			the node is than excluded from handling for all later opearation in this state.
	if(SOpProgress* pProgress = GetProgress(pNode))
	{
		pProgress->Shares = 0;
		pProgress->OpState = eOpFailed;
	}
}

void CKadOperation::SendRequests(CKadNode* pNode, CComChannel* pChannel, SOpProgress* pProgress, bool bStateless)
{
	CKadHandler* pHandler = GetParent<CKademlia>()->Handler();

	CVariant Calls;
	if(m_pOperator)
	{
		const CKadOperator::TRequestMap& ResuestMap = m_pOperator->GetRequests();
		for(CKadOperator::TRequestMap::const_iterator I = ResuestMap.begin(); I != ResuestMap.end(); I++)
		{
			CKadRequest* pRequest = I->second.pRequest;
			if(pProgress->Calls.insert(TStatusMap::value_type(pRequest->GetXID(), SOpStatus())).second) // check if we already dealed this task
			{
				CVariant Call;
				Call["FX"] = pRequest->GetName();
				Call["ARG"] = pRequest->GetArguments();
				Call["XID"] = pRequest->GetXID();
				Calls.Append(Call);
			}
		}
	}
	else
	{
		for(TCallOpMap::const_iterator I = m_CallMap.begin(); I != m_CallMap.end(); I++)
		{
			CVariant Call = I->second.Call;
			if(pProgress->Calls.insert(TStatusMap::value_type(Call["XID"], SOpStatus())).second) // check if we already dealed this task
				Calls.Append(Call);
		}
	}
	if(Calls.Count() > 0)
		pHandler->SendExecuteReq(pNode, pChannel, this, Calls, bStateless);

	CVariant Stores;
	for(TStoreOpMap::const_iterator I = m_StoreMap.begin(); I != m_StoreMap.end(); I++)
	{
		if(pProgress->Stores.insert(TStatusMap::value_type(I->first, SOpStatus())).second) // check if we already dealed this task
		{
			CVariant Store;
			Store["XID"] = I->first;
			Store["PLD"] = I->second.Payload;
			Stores.Append(Store);
		}
	}
	if(Stores.Count() > 0)
		pHandler->SendStoreReq(pNode, pChannel, this, Stores, bStateless);
			
	CVariant Loads;
	for(TLoadOpMap::const_iterator I = m_LoadMap.begin(); I != m_LoadMap.end(); I++)
	{
		if(pProgress->Loads.insert(TStatusMap::value_type(I->first, SOpStatus())).second) // check if we already dealed this task
		{
			CVariant Load;
			Load["XID"] = I->first;
			Load["PATH"] = I->second.Path;
			Loads.Append(Load);
		}
	}
	if(Loads.Count() > 0)
		pHandler->SendLoadReq(pNode, pChannel, this, Loads, bStateless);
}

bool CKadOperation::IsDone(TStatusMap& (Get)(SOpProgress*), const CVariant& XID)
{
	int UsedShares = m_InRange ? 1 : 0;
	for(TNodeStatusMap::iterator I = m_Nodes.begin(); I != m_Nodes.end(); I++)
	{
		SOpProgress* pProgress = (SOpProgress*)I->second;
		if(pProgress->Shares == 0)
			continue;

		TStatusMap& Map = Get(pProgress);
		TStatusMap::iterator J = Map.find(XID);
		if(J == Map.end() || !J->second.Done)
			return false;

		UsedShares += pProgress->Shares;
	}
	return UsedShares >= GetSpreadShare();
}

CVariant CKadOperation::AddCallRes(const CVariant& CallRes, CKadNode* pNode)
{
	SOpProgress* pProgress = GetProgress(pNode);

	CVariant FilteredRes = CallRes.Clone(false); // Make a Shellow Copy
	const CVariant& Results = CallRes["RET"];

	CVariant Filtered;
	for(uint32 i=0; i < Results.Count(); i++)
	{
        CVariant Result = Results.At(i).Clone(false); // Make a Shellow Copy
		const CVariant& XID = Result["XID"];

		// this checks if this particular response is the last and and if this node is done
		if(pProgress) // might be NULL if we filter our own index response right now
		{
			SOpStatus &Status = pProgress->Calls[XID];
			Status.Results++;
			if(!Result.Get("MORE"))
				Status.Done = true; // this marks that no more results are to be expected form this node
		}

		SOpStatus* pStatus = NULL;
		TCallOpMap::iterator I = m_CallMap.find(XID);
		if(I != m_CallMap.end())
		{
			pStatus = &I->second.Status;
			pStatus->Results++; // count the response even if it gets filtered lateron
			if(!pStatus->Done)
				pStatus->Done = IsDone(SOpProgress::GetCalls, XID);
		}

		if(m_pOperator)
		{
			CKadOperator::TRequestMap& ResuestMap = m_pOperator->GetRequests();
			CKadOperator::TRequestMap::iterator I = ResuestMap.find(XID);
			if(I != ResuestMap.end()) // this should not fail
			{
				SOpStatus* pAuxStatus = &I->second.Status;
				pAuxStatus->Results++; // count the response even if it gets filtered lateron
				if(!pAuxStatus->Done)
					pAuxStatus->Done = IsDone(SOpProgress::GetCalls, XID);
			}
		}

		if(!Result.Has("ERR"))
		{
			if(m_pOperator && m_pOperator->IsValid())
			{
				try
				{
					if(m_pOperator->AddCallRes(Result["RET"], XID))
						continue; // intercepted response - Note: if we add a response to this request now it wil be marked as no more results if thats so
				}
				catch(const CJSException& Exception)
				{
					LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
				}
			}
		}
		//else 
		//	LogLine(LOG_ERROR | LOG_DEBUG, L"Got Execution Error %s", Result["ERR"].To<wstring>().c_str());

		// check if this is a call we issued, that is one not present in the call map, in that case its never to be relayed
		if(pStatus)
		{
			// UpdateMore sets the "MORE" flag on the packet we relay further down to the source, and checks if we considder this request done
			if(!pStatus->Done)
				Result.Insert("MORE", true);
			else
				Result.Remove("MORE");

			Filtered.Append(Result);
		}
	}

	if(m_pOperator) // add new result to the filtered list
		Filtered.Merge(m_pOperator->GetResponses());
	FilteredRes.Insert("RET", Filtered);
	return FilteredRes;
}

CVariant CKadOperation::AddStoreRes(const CVariant& StoreRes, CKadNode* pNode)
{
	SOpProgress* pProgress = GetProgress(pNode);

	CVariant FilteredRes = StoreRes.Clone(false); // Make a Shellow Copy
	const CVariant& StoredList = StoreRes["RES"];

	CVariant FilteredList;
	for(uint32 i=0; i < StoredList.Count(); i++)
	{
		CVariant Stored = StoredList.At(i).Clone(false); // Make a Shellow Copy
		const CVariant& XID = Stored["XID"];

		// Counting
		if(pProgress) // might be NULL if we filter our own index response right now
		{
			SOpStatus &Status = pProgress->Stores[XID];
			Status.Results++;
			if(!Stored.Get("MORE"))
				Status.Done = true; // this marks that no more results are to be expected form this node
		}

		SOpStatus* pStatus = &m_StoreMap[XID].Status;
		pStatus->Results++;
		if(!pStatus->Done)
			pStatus->Done = IsDone(SOpProgress::GetStores, XID);

		if(!pStatus->Done)
			Stored.Insert("MORE", true);
		else
			Stored.Remove("MORE");
		//

		//m_StoredCounter[Stored["XID"]].push_back(Stored.Get("EXP", 0)); // on error there is no EXP

		FilteredList.Append(Stored);
	}
	
	FilteredRes.Insert("RES", FilteredList);
	return FilteredRes;
}

CVariant CKadOperation::AddLoadRes(const CVariant& LoadRes, CKadNode* pNode)
{
	SOpProgress* pProgress = GetProgress(pNode);

	CVariant FilteredRes = LoadRes.Clone(false); // Make a Shellow Copy
	const CVariant& LoadedList = LoadRes["RES"];

	CVariant FilteredList;
	for(uint32 i=0; i < LoadedList.Count(); i++)
	{
		CVariant Loaded = LoadedList.At(i).Clone(false); // Make a Shellow Copy
		const CVariant& XID = Loaded["XID"];

		// Counting
		if(pProgress) // might be NULL if we filter our own index response right now
		{
			SOpStatus &Status = pProgress->Loads[XID];
			Status.Results++;
			if(!Loaded.Get("MORE"))
				Status.Done = true; // this marks that no more results are to be expected form this node
		}

		SOpStatus* pStatus = &m_LoadMap[XID].Status;
		pStatus->Results++;
		if(!pStatus->Done)
			pStatus->Done = IsDone(SOpProgress::GetLoads, XID);
		
		if(!pStatus->Done)
			Loaded.Insert("MORE", true);
		else
			Loaded.Remove("MORE");
		//

		if(Loaded.Has("ERR"))
		{
			FilteredList.Append(Loaded);
			continue;
		}

		// Filtering
		CVariant UniquePayloads;
		const CVariant& Payloads = Loaded["PLD"];
		for(uint32 j=0; j < Payloads.Count(); j++)
		{
			const CVariant& Payload = Payloads.At(j);
			if(m_LoadFilter[XID].insert(Payload["DATA"].GetFP()).second)
				UniquePayloads.Append(Payload);
		}

		// Note: we must add this even if UniquePayloads is empty or else we will misscount replys
		CVariant NewLoaded;
		NewLoaded["XID"] = XID;
		NewLoaded["PLD"] = UniquePayloads;
		FilteredList.Append(NewLoaded);
		//
	}

	FilteredRes.Insert("RES", FilteredList);
	return FilteredRes;
}

void CKadOperation::ProxyingResponse(CKadNode* pNode, CComChannel* pChannel, const string &Error)
{
	if(m_LookupHistory)
		m_LookupHistory->RegisterResponse(pNode->GetID());

	SOpProgress* pProgress = GetProgress(pNode);
	if(!pProgress)
		return;

	pProgress->uTimeOut = -1;

	if(!Error.empty())
	{
		pProgress->Shares = 0;
		pProgress->OpState = eOpFailed;

		LogLine(LOG_DEBUG, L"Recived error: '%S' on proxy lookup", Error.c_str());
	}
	else
	{
		pProgress->OpState = eOpProxy;

		if(m_pOperator && m_pOperator->IsValid())
			m_pOperator->OnProxyNode(pNode);

		SendRequests(pNode, pChannel, pProgress, false);
	}
}
///////////////////////////////////////////////////////////////////////////
//

IMPLEMENT_OBJECT(CLookupProxy, CKadOperation)

CLookupProxy::CLookupProxy(const CUInt128& ID, CObject* pParent)
: CKadOperation(ID, pParent) 
{
}

void CLookupProxy::Process(UINT Tick)
{
	CKadOperation::Process(Tick);

	if((Tick & E10PerSec) == 0)
		return;

	FlushCaches();
}

void CLookupProxy::FlushCaches()
{
	if(m_pOperator)
	{
		CVariant Results = m_pOperator->GetResponses();
		if(Results.Count() > 0)
			GetParent<CKademlia>()->Handler()->SendExecuteRes(this, Results);
	}
}

bool CLookupProxy::ReadyToStop()
{
	// If we loose the connection to the initiating node we finish instantly
	if(!m_Return.pChannel->IsConnected())
		return true;
	return CKadOperation::ReadyToStop();
}

void CLookupProxy::InitProxy(CKadNode* pNode, CComChannel* pChannel) 
{
	m_Return = SKadNode(pNode, pChannel);
}

bool CLookupProxy::SendMessage(const string& Name, const CVariant& Data, CKadNode* pNode)
{
	if(pNode == m_Return.pNode)
		return GetParent<CKademlia>()->Handler()->SendMessagePkt(pNode, m_Return.pChannel, Name, Data) != 0;
	return CKadOperation::SendMessage(Name, Data, pNode);
}

CVariant CLookupProxy::AddCallReq(const CVariant& Requests)
{
	CVariant Results(CVariant::EList);

	for(uint32 i=0; i<Requests.Count(); i++)
	{
		const CVariant& Request = Requests[i];
		if(!m_CallMap.insert(TCallOpMap::value_type(Request["XID"], SCallOp(Request))).second)
			continue; // we already got this request, ignore it

		if(m_pOperator && m_pOperator->IsValid())
		{
			try
			{
				m_pOperator->AddCallReq(Request["FX"], Request["ARG"], Request["XID"]);
			}
			catch(const CJSException& Exception)
			{
				LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());

				CKadOperation::SOpStatus &Progress = m_CallMap[Request["XID"]].Status;
				Progress.Done = true; // this one is finished

				CVariant Result;
				Result["XID"] = Request["XID"];
				Result["ERR"] = Exception.GetError();
				Results.Append(Result);
			}
		}
	}

	if(m_pOperator)
		Results.Merge(m_pOperator->GetResponses());
	return Results;
}

void CLookupProxy::AddStoreReq(const CVariant& StoreReq) 
{
	for(int i=0; i < StoreReq.Count(); i++)
	{
		const CVariant& Store = StoreReq.At(i);
		if(!m_StoreMap.insert(TStoreOpMap::value_type(Store["XID"], SStoreOp(Store["PLD"]))).second)
			continue;
	}
}

void CLookupProxy::AddLoadReq(const CVariant& LoadReq)
{
	for(int i=0; i < LoadReq.Count(); i++)
	{
		const CVariant& Load = LoadReq.At(i);
		if(!m_LoadMap.insert(TLoadOpMap::value_type(Load["XID"], SLoadOp(Load["PATH"]))).second)
			continue;
	}
}

void CLookupProxy::LogReport(UINT Flags, const wstring& ErrorMessage, const string& Error, const CVariant& Trace)
{
	CKadOperation::LogReport(Flags, ErrorMessage, Error, Trace);

	if(!Trace.IsValid()) // if the trace is valid its a remote report that means it wil be relayed automatically, send only local reports
		GetParent<CKademlia>()->Handler()->SendReportPkt(GetReturnChannel(), Flags, ErrorMessage, Error);
}
