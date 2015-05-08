#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "JSKadLookup.h"
#include "JSKadRequest.h"
#include "JSKadNode.h"
#include "JSKadID.h"
#include "../Kademlia.h"
#include "../KadHandler.h"
#include "../KadTask.h"
#include "../RoutingRoot.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "../../Common/v8Engine/JSCryptoKey.h"
#include "../../Common/v8Engine/JSDataStore.h"

v8::Persistent<v8::ObjectTemplate> CJSKadLookup::m_Template;

v8::Local<v8::ObjectTemplate> CJSKadLookup::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	//InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"data"), GetData, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"addRequest"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxAddRequest),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"store"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxStore),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"load"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxLoad),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"finish"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxFinish),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"setManual"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxSetManual),v8::ReadOnly);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"node"), GetNode, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"nodes"), GetNodes, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"selectNode"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxSelectNode),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"findNodes"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxFindNodes),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"addNode"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxAddNode),v8::ReadOnly);
	
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"targetID"), GetTargetID, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"isProxy"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'prxy')),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"inRange"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'rang')),v8::ReadOnly);
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"duration"), GetValue, 0, v8::Integer::New(v8::Isolate::GetCurrent(), 'tlen'), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"timeOut"), GetValue, SetValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'timo')); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"jumpCount"), GetValue, SetValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'jmps')); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"hopLimit"), GetValue, SetValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'hops')); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"spreadCount"), GetValue, SetValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'sprd')); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"spreadShare"), GetValue, SetValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'shre')); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"storeTTL"), GetValue, SetValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'ttl')); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"loadCount"), GetValue, SetValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'cnt')); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"getAccessKey"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxGetAccessKey),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"setInterval"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxTimer,v8::Integer::New(v8::Isolate::GetCurrent(), 'setI')));
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"clearInterval"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxTimer,v8::Integer::New(v8::Isolate::GetCurrent(), 'clrI')));
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"setTimeout"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxTimer,v8::Integer::New(v8::Isolate::GetCurrent(), 'setT')));
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"clearTimeout"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxTimer,v8::Integer::New(v8::Isolate::GetCurrent(), 'clrT')));

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

//v8::Local<v8::Value> CJSKadLookup::GetData(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
//{
//	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
//	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(info.Holder());
//
//	CDataStoreObj* pData = jLookup->m_pLookup->GetOperator()->GetData();
//	return HandleScope.Close(jLookup->m_pScript->GetObject(pData)); // get existing object or 
//}

void CJSKadLookup::FxAddRequest(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(args.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CKadOperator* pOperator = pLookup->GetOperator();
	string Name = CJSEngine::GetStr(args[0]);
	CVariant Arguments = CJSVariant::FromValue(args[1]);
	CKadRequest* pRequest = pOperator->AddRequest(Name, Arguments);
	if (!pRequest)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument (XID)"));
		return;
	}
	
	CJSObject* jObject = jLookup->m_pScript->SetObject(pRequest); // Note: the instance handler for this object is persistent!

	v8::Local<v8::Object> Instance = v8::Local<v8::Object>::New(v8::Isolate::GetCurrent(), jObject->GetInstance());
	if(args.Length() >= 3)
        Instance->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"onResponse"), args[2]);

	return args.GetReturnValue().Set(Instance);
}

void CJSKadLookup::FxStore(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(args.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CKadTask* pTask = jLookup->m_pLookup->Cast<CKadTask>();
	if (!pTask)
	{
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), false));
		return;
	}

	string Path = CJSEngine::GetStr(args[0]);
	CVariant Data = CJSVariant::FromValue(args[1]);
	
	uint32 XID = pTask->Store(Path, Data);

	uint32 CallbackID = 0;
	if(args.Length() >= 3)
		CallbackID = jLookup->m_pScript->SetCallback(args[2], pLookup);
	pLookup->GetOperator()->SetStoreClBk(XID, CallbackID);
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), true));
}

void CJSKadLookup::FxLoad(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(args.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CKadTask* pTask = jLookup->m_pLookup->Cast<CKadTask>();
	if (!pTask)
	{
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), false));
		return;
	}

	string Path = CJSEngine::GetStr(args[0]);

	uint32 XID = pTask->Load(Path);

	uint32 CallbackID = 0;
	if(args.Length() >= 2)
		CallbackID = jLookup->m_pScript->SetCallback(args[1], pLookup);
	pLookup->GetOperator()->SetLoadClBk(XID, CallbackID);
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), true));
}

void CJSKadLookup::FxFinish(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CKadOperation* pLookup = GetCObject<CKadOperation>(args.Holder());
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	pLookup->PrepareStop();
	args.GetReturnValue().SetUndefined();
}

void CJSKadLookup::FxSetManual(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CKadOperation* pLookup = GetCObject<CKadOperation>(args.Holder());
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	if (pLookup->IsStarted() || pLookup->IsStopped())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Operation Mode cant be changed after task initialisation"));
		return;
	}

	pLookup->SetManualMode();
	args.GetReturnValue().SetUndefined();
}

void CJSKadLookup::GetNode(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(info.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	map<CPointer<CKadNode>, CJSKadNode*>::iterator I = pLookup->GetOperator()->m_KadNodes.find(NULL);
	if(I == pLookup->GetOperator()->m_KadNodes.end() || !I->second) // we dont have a return node we are the initiator
		info.GetReturnValue().SetNull();
	else
		info.GetReturnValue().Set(I->second->GetInstance());
}

void CJSKadLookup::GetNodes(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(info.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	map<CUInt128, CJSKadNode*> Map; // sort by distance
	for(map<CPointer<CKadNode>, CJSKadNode*>::iterator I = pLookup->GetOperator()->m_KadNodes.begin(); I != pLookup->GetOperator()->m_KadNodes.end(); I++)
	{
		if(I->first == NULL)
			continue;
		Map[I->first->GetID() ^ pLookup->GetID()] = I->second;
	}

	int Counter = 0;
	v8::Local<v8::Array> Array = v8::Array::New(v8::Isolate::GetCurrent(), Map.size());
	for (map<CUInt128, CJSKadNode*>::iterator I = Map.begin(); I != Map.end(); I++)
	{
		v8::Local<v8::Object> Instance = v8::Local<v8::Object>::New(v8::Isolate::GetCurrent(), I->second->GetInstance());
		Array->Set(Counter++, Instance);
	}
	info.GetReturnValue().Set(Array);
}

void CJSKadLookup::FxSelectNode(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(args.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	int Select;
	if(args.Length() >= 1)
		Select = args[0]->Int32Value();
	else
		Select = pLookup->GetJumpCount() > 0 ? 0 : (pLookup->GetHopLimit() > 1 ? pLookup->GetSpreadCount() : 1);

	CRoutingZone::SIterator Iter(Select);
	for(CKadNode* pNode = NULL; (pNode = pLookup->GetParent<CKademlia>()->Root()->GetClosestNode(Iter, pLookup->GetID())) != NULL;)
	{
		if(pLookup->GetJumpCount() == 0 && ((pNode->GetID() ^ pLookup->GetID()) > pLookup->GetParent<CKademlia>()->Root()->GetMaxDistance()))
			break; // there are no nodes anymore in list that would be elegable here

		CKadOperation::SOpProgress* pProgress = pLookup->GetProgress(pNode);
		if(pProgress && pProgress->OpState != CKadOperation::eNoOp)
			continue; // this node has already been tryed

		CJSObject* jObject = CJSKadID::New(new CKadIDObj(pNode->GetID()), jLookup->m_pScript);
		args.GetReturnValue().Set(jObject->GetInstance());
		return;
	}
	args.GetReturnValue().SetNull();
}

void CJSKadLookup::FxFindNodes(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(args.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	if (!pLookup->FindCloser())
	{
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), false));
		return;
	}

	v8::Local<v8::Object> Instance = v8::Local<v8::Object>::New(v8::Isolate::GetCurrent(), jLookup->GetInstance());
	if(args.Length() >= 1)
        Instance->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"onNodes"), args[0]);

	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), true));
}

void CJSKadLookup::FxAddNode(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(args.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	// NodeID, stateless, shares, init_param
	CUInt128 ID = CJSKadID::FromValue(args[0]);
	if (ID == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	bool bStateless;
	int Shares = 0;
	if(args.Length() >= 2)
	{
		bStateless = args[1]->BooleanValue();
		if(!bStateless && args.Length() >= 3)
			Shares = args[2]->Int32Value();
	}
	else
		bStateless = pLookup->GetHopLimit() <= 1;

	CVariant InitParam;
	if(args.Length() >= 4)
		InitParam = CJSVariant::FromValue(args[3]);
	else
		InitParam = pLookup->GetInitParam();

	CKadNode* pNode = pLookup->GetParent<CKademlia>()->Root()->GetNode(ID);
	if (!pNode || !pLookup->AddNode(pNode, bStateless, InitParam, Shares))
	{
		args.GetReturnValue().SetNull(); // something failed
		return;
	}

	args.GetReturnValue().Set(jLookup->m_pScript->GetObject(pNode));
}

void CJSKadLookup::GetTargetID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(info.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	CJSObject* jObject = CJSKadID::New(new CKadIDObj(pLookup->GetID()), jLookup->m_pScript);
	info.GetReturnValue().Set(jObject->GetInstance());
}

void CJSKadLookup::FxValue(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CKadOperation* pLookup = GetCObject<CKadOperation>(args.Holder());
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	uint32_t Type = args.Data()->Int32Value();
	switch(Type)
	{
		case 'prxy': args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), pLookup->Inherits("CLookupProxy"))); break;
		case 'rang': args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), pLookup->IsInRange())); break;
		default: args.GetReturnValue().SetUndefined();
	}
}

void CJSKadLookup::GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CKadOperation* pLookup = GetCObject<CKadOperation>(info.Holder());
	if (!pLookup)
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	switch(info.Data()->Int32Value())
	{
		case 'tlen': info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pLookup->GetDuration())); break;
		
		case 'timo': info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pLookup->GetTimeOut())); break;
		case 'jmps': info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pLookup->GetJumpCount())); break;
		case 'hops': info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pLookup->GetHopLimit())); break;
		case 'sprd': info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pLookup->GetSpreadCount())); break;
		case 'shre': info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pLookup->GetSpreadShare())); break;

		case 'ttl': info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pLookup->GetStoreTTL())); break;
		case 'cnt': info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pLookup->GetLoadCount())); break;
		default: info.GetReturnValue().SetUndefined();
	}
}

void CJSKadLookup::SetValue(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CKadOperation* pLookup = GetCObject<CKadOperation>(info.Holder());
	if(!pLookup)
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	if(pLookup->Inherits("CLookupProxy") || pLookup->IsStarted() || pLookup->IsStopped())
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Parameter cant be changed after task initialisation"));
		return;
	}

	switch(info.Data()->Int32Value())
	{
		case 'time': pLookup->SetTimeOut(value->Uint32Value()); break;
		case 'jmps': pLookup->SetJumpCount(value->Uint32Value()); break;
		case 'hops': pLookup->SetHopLimit(value->Uint32Value()); break;
		case 'sprd': pLookup->SetSpreadCount(value->Uint32Value()); break;
		case 'shre': pLookup->SetSpreadShare(value->Uint32Value()); break;

		case 'ttl': pLookup->SetStoreTTL(value->Uint32Value()); break;
		case 'cnt': pLookup->SetLoadCount(value->Uint32Value()); break;
	}
}

void CJSKadLookup::FxGetAccessKey(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(args.Holder());
	CKadOperation* pLookup = jLookup->m_pLookup;
	if (!pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	bool bPrivate = false;
	if(args.Length() >= 1)
		bPrivate = args[0]->BooleanValue();

	if(bPrivate)
	{
		CKadTask* pTask = pLookup->Cast<CKadTask>();
		if (!pTask)
		{
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Access Denided")); // Only the initial lookup is in posession of the private key
			return;
		}

		if(CPrivateKey* pPrivKey = pTask->GetStoreKey())
		{
			CCryptoKeyObj* pCryptoKey = new CCryptoKeyObj();
			pCryptoKey->SetAlgorithm(pPrivKey->GetAlgorithm());
			pCryptoKey->SetKey(pPrivKey);
			CJSObject* jObject = CJSPrivKey::New(pCryptoKey, jLookup->m_pScript);
			args.GetReturnValue().Set(jObject->GetInstance());
			return;
		}
	}
	else // Public
	{
		CVariant AccessKey = pLookup->GetAccessKey();
		if(AccessKey.IsValid())
		{
			CScoped<CPublicKey> pPubKey = new CPublicKey();	
			if (!pPubKey->SetKey(AccessKey.GetData(), AccessKey.GetSize()))
			{
				args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid (or not supported) key data"));
				return;
			}

			CCryptoKeyObj* pCryptoKey = new CCryptoKeyObj();
			pCryptoKey->SetAlgorithm(pPubKey->GetAlgorithm());
			pCryptoKey->SetKey(pPubKey);
			CJSObject* jObject = CJSPrivKey::New(pCryptoKey, jLookup->m_pScript);
			args.GetReturnValue().Set(jObject->GetInstance());
			return;
		}
	}
	args.GetReturnValue().SetNull();
}

void CJSKadLookup::FxTimer(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadLookup* jLookup = GetJSObject<CJSKadLookup>(args.Holder());
	uint32_t Type = args.Data()->Int32Value();
	switch(Type)
	{
		case 'setI':
		case 'setT':
			if (args.Length() >= 2)
				args.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), jLookup->SetTimer(args[0], args[1]->Int32Value(), Type == 'setI')));
			break;
		case 'clrI':
		case 'clrT':
			if(args.Length() >= 1)
				jLookup->ClearTimer(args[0]->Int32Value());
			break;
		default:		
			ASSERT(0);
			args.GetReturnValue().SetUndefined();
	}
}

v8::Local<v8::Object> CJSKadLookup::ThisObject()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Object> Instance = v8::Local<v8::Object>::New(v8::Isolate::GetCurrent(), m_Instance);
	return HandleScope.Escape(Instance);
}
