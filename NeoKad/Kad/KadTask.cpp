#include "GlobalHeader.h"
#include "KadHeader.h"
#include "Kademlia.h"
#include "KadTask.h"
#include "KadConfig.h"
#include "KadNode.h"
#include "KadHandler.h"
#include "../../Framework/Maths.h"
#include "KadEngine/KadOperator.h"
#include "KadEngine/KadEngine.h"

IMPLEMENT_OBJECT(CKadTask, CKadOperation)

CKadTask::CKadTask(const CUInt128& ID, CObject* pParent)
 : CKadOperation(ID, pParent)
{
	// Note: we never got through our own index if we are the initiatio so we can not count as as 1 or else we will have 
	//			less free shares than counted results
	m_InRange = false; 
}

CKadTask::~CKadTask()
{
}

bool CKadTask::SetupScript(const CVariant& CID)
{
	// Note: this function is used only for internal instantiation that means if we are the initiator of the lookup
	if(CKadScript* pKadScript = GetParent<CKademlia>()->Engine()->GetScript(CID))
	{
		string Error = CKadOperation::SetupScript(pKadScript);
		if(Error.empty())
			return true;
		LogLine(LOG_ERROR, L"Failed to instantiate JSScript for local kad Operation: %S", Error.c_str());
	}
	return false;
}

void CKadTask::Process(UINT Tick)
{
	CKadOperation::Process(Tick);

	FlushCaches();
}

void CKadTask::FlushCaches()
{
	if(m_pOperator)
	{
		CVariant Responses = m_pOperator->GetResponses();
		for(int i=0; i < Responses.Count(); i++)
		{
			const CVariant& Response = Responses.At(i);
			SKadRet KadResult;
			KadResult.Function = m_Calls[Response.Get("XID")].Function;
			KadResult.Return = Response.Get("RET");
			m_Results.insert(TRetMap::value_type(Response.Get("XID"), KadResult));
		}
	}
}

void CKadTask::AddCall(const string& Name, const CVariant& Arguments, const CVariant& XID)
{
	SKadCall Call;
	Call.Function = Name;
	Call.Parameters = Arguments;
	m_Calls[XID] = Call; 
}

void CKadTask::InitOperation()
{
	// Note: init was already called by CKadOperation::Start()

	for(TCallMap::const_iterator I = m_Calls.begin(); I != m_Calls.end(); I++)
	{
		CVariant Call;
		Call["FX"] = I->second.Function;
		Call["ARG"] = I->second.Parameters;
		Call["XID"] = I->first;
		if(!m_CallMap.insert(TCallOpMap::value_type(I->first, SCallOp(Call))).second)
		{
			ASSERT(0);
			continue; // we already got this request, ignore it
		}

		if(m_pOperator && m_pOperator->IsValid())
		{
			try
			{
				m_pOperator->AddCallReq(I->second.Function, I->second.Parameters, I->first);
			}
			catch(const CJSException& Exception)
			{
				LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());

				CKadOperation::SOpStatus &Progress = m_CallMap[I->first].Status;
				Progress.Done = true; // this one is finished

				SKadRet KadResult;
				KadResult.Function = I->second.Function;
				KadResult.Return = CVariant();
				m_Results.insert(TRetMap::value_type(I->first, KadResult));

				LogLine(LOG_ERROR, L"Failed to execute local JSScript call: %S", Exception.GetError().c_str());
			}
		}
	}

	CKadOperation::InitOperation(); // sets the lookup parameters

	// Note: after this the lookup will be set to active mode
}

CVariant CKadTask::Store(const string& Path, const CVariant& Data)
{	
	uint32 XID = 0; // Generate a new store ID not used befoure
	do XID = GetRand64();  //we use 32 bit integers to save on overhead but we could us anything
	while (m_StoreMap.count(XID) > 0);

	Store(XID, Path, Data);
	return XID;
}

bool CKadTask::Store(const CVariant& XID, const string& Path, const CVariant& Data)
{
	CVariant Payload;
	Payload["PATH"] = Path;
	if(Data.IsValid()) // if we have data (Data != CVariant())
		Payload["DATA"] = Data;
	//else no "DATA" means refresh, if we dont add any data we want to refresh existing entries
	// Note "DATA" == CVariant() would mean delete, we dont want that
	Payload["RELD"] = GetTime();
	if(m_pStoreKey)
		Payload.Sign(m_pStoreKey);

	return m_StoreMap.insert(TStoreOpMap::value_type(XID, SStoreOp(Payload))).second;
}

CVariant CKadTask::Load(const string& Path)
{
	uint32 XID = 0; // Generate a new load ID not used befoure
	do XID = GetRand64();  //we use 32 bit integers to save on overhead but we could us anything
	while (m_LoadMap.count(XID) > 0);

	Load(XID, Path);

	return XID;
}

bool CKadTask::Load(const CVariant& XID,const string& Path)
{
	return m_LoadMap.insert(TLoadOpMap::value_type(XID, SLoadOp(Path))).second;
}

CVariant CKadTask::GetAccessKey()
{
	if(!m_pStoreKey)
		return CVariant();

	CScoped<CPublicKey> pPubKey = m_pStoreKey->PublicKey();
	return CVariant(pPubKey->GetKey(), pPubKey->GetSize());
}

CVariant CKadTask::AddCallRes(const CVariant& CallRes, CKadNode* pNode)
{
	CVariant FilteredRet = CKadOperation::AddCallRes(CallRes, pNode);

	FlushCaches();

	const CVariant& Results = FilteredRet["RET"];
	for(uint32 i=0; i < Results.Count(); i++)
	{
		const CVariant& Result = Results.At(i);
		if(Result.Has("ERR"))
		{
			LogLine(LOG_DEBUG | LOG_ERROR, L"lookup execution request error: %s", Result["ERR"].To<wstring>().c_str());
			continue;
		}

		SKadRet KadResult;
		KadResult.Return = Result["RET"];
		KadResult.Function = m_Calls[Result["XID"]].Function;
		m_Results.insert(TRetMap::value_type(Result["XID"], KadResult));
	}

	return CVariant(); // this is not evaluate further
}

CVariant CKadTask::AddStoreRes(const CVariant& StoreRes, CKadNode* pNode)
{
	CVariant FilteredRes = CKadOperation::AddStoreRes(StoreRes, pNode);

	const CVariant& StoredList = FilteredRes["RES"];
	for(uint32 i=0; i < StoredList.Count(); i++)
	{
		const CVariant& Stored = StoredList.At(i);
		if(Stored.Has("ERR"))
		{
			LogLine(LOG_DEBUG | LOG_ERROR, L"lookup store error: %s", Stored["ERR"].To<wstring>().c_str());
			continue;
		}

		try
		{
			if(!m_pOperator || !m_pOperator->CallStoreClBk(Stored["XID"], Stored["EXP"])) 
				m_StoredMap[Stored["XID"]].push_back(Stored["EXP"]);
		}
		catch(const CJSException& Exception)
		{
			LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
		}
	}
	return CVariant(); // this is not evaluate further
}

CVariant CKadTask::AddLoadRes(const CVariant& RetrieveRes, CKadNode* pNode)
{
	CVariant FilteredRes = CKadOperation::AddLoadRes(RetrieveRes, pNode);

	const CVariant& LoadedList = FilteredRes["RES"];
	for(uint32 i=0; i < LoadedList.Count(); i++)
	{
		const CVariant& Loaded = LoadedList.At(i);
		if(Loaded.Has("ERR"))
		{
			LogLine(LOG_DEBUG | LOG_ERROR, L"lookup Retrieve error: %s", Loaded["ERR"].To<wstring>().c_str());
			continue;
		}

		try
		{
			if(!m_pOperator || !m_pOperator->CallLoadClBk(Loaded["XID"], Loaded["PLD"]))
			{
				const CVariant& Payloads = Loaded["PLD"];
				for(uint32 j=0; j < Payloads.Count(); j++)
				{
					const CVariant& Payload = Payloads.At(j);
					SKadLoaded Entry;
					Entry.Path = Payload["PATH"].To<string>();
					Entry.Data = Payload["DATA"];
								//Payload["RELD"]
					m_LoadedMap.insert(TLoadedMap::value_type(Loaded["XID"], Entry));
				}	
			}
		}
		catch(const CJSException& Exception)
		{
			LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
		}
	}
	return CVariant(); // this is not evaluate further
}

void CKadTask::QueryStored(TStoredMap& Stored)
{
	for(map<CVariant, vector<time_t> >::iterator I = m_StoredMap.begin(); I != m_StoredMap.end(); I++)
	{
		SKadStored Entry;
		Entry.Count = (uint32)I->second.size();
		Entry.Expire = Median(I->second);
		Stored[I->first] = Entry; // XID
	}
}

void CKadTask::QueryLoaded(TLoadedMap& Loaded)
{
	Loaded.swap(m_LoadedMap);
}

void CKadTask::QueryResults(TRetMap& Results)
{
	Results.swap(m_Results);
}
