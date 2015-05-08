#pragma once

class CKadScript;
class CDataStoreObj;
class CJSKadLookup;
class CJSKadNode;
class CKadRequest;
class CJSKadRequest;
class CJSScript;
#include "../KadOperation.h"
#include "../../Common/v8Engine/JSScript.h"

class CKadOperator: public CObject
{
public:
	DECLARE_OBJECT(CKadOperator)

	CKadOperator(CKadScript* pKadScript, CJSScript* pJSScript, CObject* pParent = NULL);
	virtual ~CKadOperator();

	bool					IsValid()									{return m_pJSScript != NULL;}
	//CDataStoreObj*			GetData()								{return m_Data;}

	CVariant				Init(const CVariant& InitParam);

	void					RunTimers();

	void					OnStart();
	bool					OnFinish(bool bForceFinish, bool bTimedOut = true, bool bFoundEnough = false);
	void					Finish();

	CKadScript*				GetScript()								{return m_pKadScript;}

	void					ProcessMessage(const string& Name, const CVariant& Data, CKadNode* pNode);

	void					AddNode(CKadNode* pNode);
	void					OnProxyNode(CKadNode* pNode);
	void					OnClosestNodes();

	struct SKadRequest
	{
		SKadRequest(CPointer<CKadRequest>& pRequest) : pJSRequest(NULL) {this->pRequest = pRequest;}
		CPointer<CKadRequest>		pRequest;
		CKadOperation::SOpStatus	Status;
		CJSKadRequest*				pJSRequest;
	};
	typedef map<CVariant, SKadRequest> TRequestMap;
	TRequestMap&			GetRequests()							{return m_RequestMap;}
	int						CountRequests()							{return m_RequestMap.size();}

	CVariant				GetResponses();

	void					AddCallReq(const string& Name, const CVariant& Arguments, const CVariant& XID);
	bool					AddCallRes(const CVariant& Arguments, const CVariant& XID);

	bool					HasStoreClBks()											{return !m_StoreClBks.empty();}
	void					SetStoreClBk(const CVariant& XID, uint32 CallbackID)	{m_StoreClBks[XID] = CallbackID;}
	bool					CallStoreClBk(const CVariant& XID, time_t Expire);

	bool					HasLoadClBks()											{return !m_LoadClBks.empty();}
	void					SetLoadClBk(const CVariant& XID, uint32 CallbackID)		{m_LoadClBks[XID] = CallbackID;}
	bool					CallLoadClBk(const CVariant& XID, const CVariant& Payloads);

protected:
	friend class CJSKadLookup;
	friend class CKadRequest;

	CPointer<CObject>		OnLookupEvent(const string& Event, vector<CPointer<CObject> >& Parameters);

	CKadRequest*			AddRequest(const string& Name, const CVariant& Arguments);

	bool					AddResponse(const CVariant& Return, const CVariant& XID, bool bMore);
	bool					HasMore(const CVariant& XID);

	CPointer<CKadScript>	m_pKadScript;
	CPointer<CJSScript>		m_pJSScript;
	//CPointer<CDataStoreObj> m_Data;
	CJSKadLookup*			m_pJSKadLookup;

	map<CPointer<CKadNode>, CJSKadNode*> m_KadNodes;

	TRequestMap				m_RequestMap;	// List of queued requests we want to make
	CVariant				m_Responses;	// Response packets to be send down

	map<CVariant, uint32>	m_StoreClBks;
	map<CVariant, uint32>	m_LoadClBks;
};

class CKadRequest: public CObject
{
public:
	DECLARE_OBJECT(CKadRequest)

	CKadRequest(CKadOperator* pOperator, const CVariant& XID, const string& function, const CVariant& arguments)
		: CObject(pOperator), m_Function(function), m_Arguments(arguments), m_XID(XID) {}
	virtual ~CKadRequest() {}

	const string&			GetName()		{return m_Function;}
	const CVariant&			GetArguments()	{return m_Arguments;}
	const CVariant&			GetXID()		{return m_XID;}

protected:
	friend class CJSKadRequest;

	bool					AddResponse(const CVariant& Return, bool bMore) {
		return GetParent()->Cast<CKadOperator>()->AddResponse(Return, m_XID, bMore);
	}
	bool					HasMore() {
		return GetParent()->Cast<CKadOperator>()->HasMore(m_XID);
	}

	string		m_Function;
	CVariant	m_Arguments;
	CVariant	m_XID;
};