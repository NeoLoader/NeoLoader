#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "KadOperator.h"
#include "KadScript.h"
#include "JSKadLookup.h"
#include "JSKadNode.h"
#include "JSKadRequest.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "../../Common/v8Engine/JSDataStore.h"
#include "KadDebugging.h"

IMPLEMENT_OBJECT(CKadRequest, CObject)

IMPLEMENT_OBJECT(CKadOperator, CObject)

CKadOperator::CKadOperator(CKadScript* pKadScript, CJSScript* pJSScript, CObject* pParent)
 : CObject(pParent), m_Responses(CVariant::EList)
{
	m_pJSKadLookup = NULL;
	ASSERT(pKadScript);
	m_pKadScript = pKadScript;
	
	m_pKadScript->KeepAlive();

	m_pJSScript = CPointer<CJSScript>(pJSScript, true); // this is a week pointer the script may be killed by a clean up function or a resource watchdog

	if(CJSObject* pObject = pJSScript->SetObject(GetParent<CKadOperation>())) // Note: the instance handler for this object is persistent!
		m_pJSKadLookup = (CJSKadLookup*)pObject;

	//m_Data = new CDataStoreObj();
}

CKadOperator::~CKadOperator()
{
	if(m_pJSKadLookup)
		m_pJSKadLookup->MakeWeak();

	for(map<CPointer<CKadNode>, CJSKadNode*>::iterator I = m_KadNodes.begin(); I != m_KadNodes.end(); I++)
	{
		ASSERT(I->second);
		I->second->MakeWeak();
	}

	for(TRequestMap::iterator I = m_RequestMap.begin(); I != m_RequestMap.end(); I++)
	{
		if(I->second.pJSRequest)
			I->second.pJSRequest->MakeWeak();
	}
}

CVariant CKadOperator::Init(const CVariant& InitParam)
{
	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return CVariant();
	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	if(CLookupProxy* pProxy = pOperation->Cast<CLookupProxy>())
	{
		CJSKadNode* &JSKadNode = m_KadNodes[NULL]; // NULL means its the return node
		if(JSKadNode = (CJSKadNode*)m_pJSScript->SetObject(pProxy->GetReturnNode())) // Note: the instance handler for this object is persistent!
			JSKadNode->SetOperation(pOperation);
	}

	if(!m_pJSScript->Has("lookupAPI", "init"))
		return CVariant(); // nothing to do here

	CDebugScope Debug(m_pKadScript, pOperation);

	vector<CPointer<CObject> > Parameters;
	Parameters.push_back(pOperation);
	if(InitParam.IsValid())
		Parameters.push_back(new CVariantPrx(InitParam));
	CPointer<CObject> Return;
	m_pJSScript->Call(string("lookupAPI"), "init", Parameters, Return);
	if(CVariantPrx* pVariant = Return->Cast<CVariantPrx>())
		return pVariant->GetCopy();
	return CVariant();
}

void CKadOperator::Finish()
{
	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return;
	if(!m_pJSScript->Has("lookupAPI", "finish"))
		return; // nothing to do here

	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	CDebugScope Debug(m_pKadScript, pOperation);

	vector<CPointer<CObject> > Parameters;
	Parameters.push_back(pOperation);
	CPointer<CObject> Return;
	m_pJSScript->Call(string("lookupAPI"), "finish", Parameters, Return);
}

void CKadOperator::RunTimers()
{
	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return;
	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	try
	{
		CDebugScope Debug(m_pKadScript, pOperation);

		ASSERT(m_pJSKadLookup);
		m_pJSKadLookup->RunTimers(m_pJSScript);
	}
	catch(const CJSException& Exception)
	{
		pOperation->LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
	}
}

void CKadOperator::ProcessMessage(const string& Name, const CVariant& Data, CKadNode* pNode)
{
	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return;
	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	try
	{
		CDebugScope Debug(m_pKadScript, pOperation);

		vector<CPointer<CObject> > Parameters;
		Parameters.push_back(new CVariantPrx(Name));
		Parameters.push_back(new CVariantPrx(Data));
		CPointer<CObject> Return;
		m_pJSScript->Call(pNode, "onMessage", Parameters, Return);
	}
	catch(const CJSException& Exception)
	{
		pOperation->LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
	}
}

void CKadOperator::AddNode(CKadNode* pNode)
{
	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return;
	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	CJSKadNode* &JSKadNode = m_KadNodes[pNode];
	if(JSKadNode == NULL)
	{
		if(JSKadNode = (CJSKadNode*)m_pJSScript->SetObject(pNode)) // Note: the instance handler for this object is persistent!
			JSKadNode->SetOperation(pOperation);
	}
}

CPointer<CObject> CKadOperator::OnLookupEvent(const string& Event, vector<CPointer<CObject> >& Parameters)
{
	CPointer<CObject> Return;
	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return Return;
	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	try
	{
		if(m_pJSScript->Has(pOperation, Event))
		{
			CDebugScope Debug(m_pKadScript, pOperation);

			m_pJSScript->Call(pOperation, Event, Parameters, Return);
		}
	}
	catch(const CJSException& Exception)
	{
		pOperation->LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
	}
	return Return;	
}

void CKadOperator::OnStart()
{
	vector<CPointer<CObject> > Parameters;
	OnLookupEvent("onStart", Parameters);
}

void CKadOperator::OnClosestNodes()
{
	vector<CPointer<CObject> > Parameters;
	OnLookupEvent("onNodes", Parameters);
}

void CKadOperator::OnProxyNode(CKadNode* pNode)
{
	vector<CPointer<CObject> > Parameters;
	Parameters.push_back(pNode);
	OnLookupEvent("onNode", Parameters);
}

bool CKadOperator::OnFinish(bool bForceFinish, bool bTimedOut, bool bFoundEnough)
{
	vector<CPointer<CObject> > Parameters;
	Parameters.push_back(new CVariantPrx(bForceFinish));
	Parameters.push_back(new CVariantPrx(bTimedOut));
	Parameters.push_back(new CVariantPrx(bFoundEnough));
	if(CVariantPrx* pVariant = OnLookupEvent("onFinish", Parameters)->Cast<CVariantPrx>())
		return pVariant->GetCopy().AsNum<int>() != 0;
	return true; // null return means the event is not present
}

CKadRequest* CKadOperator::AddRequest(const string& Name, const CVariant& Arguments)
{
	uint32 XID = 0; // Generate a new request ID not used befoure
	do XID = GetRand64();  //we use 32 bit integers to save on overhead but we could us anything
	while (m_RequestMap.count(XID) > 0);

	// Note: we dont need to mark the request for filtering as we know after chekcing out the m_CallMap if its to be filtered or not
	CPointer<CKadRequest> pRequest = new CKadRequest(this, XID, Name, Arguments);

	if(!m_RequestMap.insert(TRequestMap::value_type(XID, SKadRequest(pRequest))).second)
		return NULL;

	return pRequest;
}

void CKadOperator::AddCallReq(const string& Name, const CVariant& Arguments, const CVariant& XID)
{
	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return;

	CPointer<CKadRequest> pRequest = new CKadRequest(this, XID, Name, Arguments);

	SKadRequest Request(pRequest);

	if(m_pJSScript->Has("lookupAPI", Name))
	{
		Request.pJSRequest = (CJSKadRequest*)m_pJSScript->SetObject(pRequest); // this is a strong pointer so it must be managed by hand

		CKadOperation* pOperation = GetParent<CKadOperation>();
		ASSERT(pOperation);

		CDebugScope Debug(m_pKadScript, pOperation);

		vector<CPointer<CObject> > Parameters;
		Parameters.push_back(pOperation); // lookup
		Parameters.push_back(new CVariantPrx(Arguments)); // call arguments
		Parameters.push_back(&*pRequest); // request object
		CPointer<CObject> Return;
		m_pJSScript->Call(string("lookupAPI"), Name, Parameters, Return);
		if(CVariantPrx* pVariant = Return->Cast<CVariantPrx>())
		{
			if(pVariant->GetCopy().AsNum<int>() != 0) // return true means we handle the request fully and it should not be automatically relayed to the next node
			{
				Request.pJSRequest->MakeWeak();
				return; 
			}
		}
	}

	// we did not intercept this request so put it on the list of to be done requests
	m_RequestMap.insert(TRequestMap::value_type(XID, Request));
}

bool CKadOperator::AddCallRes(const CVariant& Arguments, const CVariant& XID)
{
	TRequestMap::iterator I = m_RequestMap.find(XID); 
	if(I == m_RequestMap.end())
		return false;

	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return false;

	CKadRequest* pRequest = I->second.pRequest;
	if(!m_pJSScript->Has(pRequest, "onResponse"))
		return false; // pRequest->IsFiltered(); // nothing to do
	
	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	CDebugScope Debug(m_pKadScript, pOperation);

	vector<CPointer<CObject> > Parameters;
	Parameters.push_back(pOperation); // lookup
	Parameters.push_back(new CVariantPrx(Arguments)); // return value
	CPointer<CObject> Return;
	m_pJSScript->Call(pRequest, "onResponse", Parameters, Return);
	if(CVariantPrx* pVariant = Return->Cast<CVariantPrx>())
	{
		if(pVariant->GetCopy().AsNum<int>() != 0)
			return true; // we choose to filter this response
	}
	return false; //pRequest->IsFiltered();
}

bool CKadOperator::AddResponse(const CVariant& Return, const CVariant& XID, bool bMore)
{
	CVariant Result;
	Result["XID"] = XID;
	Result["RET"] = Return;

	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	CKadOperation::TCallOpMap &CallMap = pOperation->GetCalls();
	CKadOperation::TCallOpMap::iterator I = CallMap.find(XID);
	if(I == CallMap.end())
		return false; // script is fucked up it sends a response to its own request

	CKadOperation::SOpStatus* pStatus = &I->second.Status;
	// Note: If we relayed this request we lost the controll over the more flag
	if(m_RequestMap.count(XID) == 0)
	{
		pStatus->Results++;
		if(!bMore) 
			pStatus->Done = true;
	}

	// Note: this may be aleady set by the response filter
	if(!pStatus->Done)
		Result["MORE"] = true;

	m_Responses.Append(Result);
	return true;
}

CVariant CKadOperator::GetResponses()
{
	CVariant Responses = m_Responses; 
	m_Responses = CVariant(CVariant::EList); 
	return Responses;
}

bool CKadOperator::HasMore(const CVariant& XID)
{
	TRequestMap::iterator I = m_RequestMap.find(XID); 
	if(I == m_RequestMap.end())
		return false;
	return !I->second.Status.Done;
}

bool CKadOperator::CallStoreClBk(const CVariant& XID, time_t Expire)
{
	map<CVariant, uint32>::iterator I = m_StoreClBks.find(XID); 
	if(I == m_StoreClBks.end())
		return false;

	if(I->second == 0)
		return true;

	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return false;
	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	CDebugScope Debug(m_pKadScript, pOperation);

	vector<CPointer<CObject> > Parameters;
	Parameters.push_back(pOperation); // lookup
	Parameters.push_back(new CVariantPrx(Expire));
	CPointer<CObject> Return;
	m_pJSScript->Callback(I->second, Parameters, Return, false, pOperation);

	return true;
}

bool CKadOperator::CallLoadClBk(const CVariant& XID, const CVariant& Payloads)
{
	map<CVariant, uint32>::iterator I = m_LoadClBks.find(XID); 
	if(I == m_LoadClBks.end())
		return false;

	if(I->second == 0)
		return true;

	ASSERT(m_pJSScript); // WARNING: this is a week pointer
	if(!m_pJSScript)
		return false;
	CKadOperation* pOperation = GetParent<CKadOperation>();
	ASSERT(pOperation);

	CDebugScope Debug(m_pKadScript, pOperation);

	for(uint32 j=0; j < Payloads.Count(); j++)
	{
		const CVariant& Payload = Payloads[j];

		vector<CPointer<CObject> > Parameters;
		Parameters.push_back(pOperation); // lookup
		Parameters.push_back(new CVariantPrx(Payload["DATA"]));
		Parameters.push_back(new CVariantPrx(Payload["PATH"]));
		Parameters.push_back(new CVariantPrx(Payload["RELD"]));
		CPointer<CObject> Return;
		m_pJSScript->Callback(I->second, Parameters, Return, false, pOperation);
	}

	return true;
}
