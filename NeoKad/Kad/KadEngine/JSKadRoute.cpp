#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "../Kademlia.h"
#include "JSKadRoute.h"
#include "JSRouteSession.h"
#include "KadScript.h"
#include "JSKadID.h"
#include "KadEngine.h"
#include "../../Common/v8Engine/JSScript.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "KadDebugging.h"

IMPLEMENT_OBJECT(CKadRouteObj, CKadRoute)

CKadRouteObj::CKadRouteObj(const CUInt128& ID, CPrivateKey* pEntityKey, CObject* pParent)
 : CKadRoute(ID, pEntityKey, pParent)
{
}

void CKadRouteObj::IncomingSession(CRouteSession* pSession)
{
	CKadScript* pKadScript = GetParent<CKadScript>();
	ASSERT(pKadScript);
	CJSScript* pJSScript = pKadScript->GetJSScript();
	if(!pJSScript) // if a script gets terminated all routes get closed to, this should not happen
		return; 

	((CRouteSessionObj*)pSession)->SetupInstance(pJSScript);

	CDebugScope Debug(pKadScript, this);

	try
	{
		vector<CPointer<CObject> > Arguments;
		Arguments.push_back(CPointer<CObject>(pSession, true));
		CPointer<CObject> Return;
		pJSScript->Call(this, "onSession", Arguments, Return);
	}
	catch(const CJSException& Exception)
	{
		pKadScript->LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
	}
}

CRouteSession* CKadRouteObj::MkRouteSession(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent)
{
	return new CRouteSessionObj(EntityID, TargetID, pParent);
}

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSKadRoute::m_Template;

v8::Local<v8::ObjectTemplate> CJSKadRoute::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"openSession"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxOpenSession),v8::ReadOnly);
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"targetID"), GetTargetID, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"entityID"), GetEntityID, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

void CJSKadRoute::FxOpenSession(const v8::FunctionCallbackInfo<v8::Value> &args)
{
#ifdef _DEBUG
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadRoute* jRoute = GetJSObject<CJSKadRoute>(args.Holder());
	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	CVariantPrx* pEntityID = GetCObject<CVariantPrx>(args[0]->ToObject());
	if (!pEntityID)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CUInt128 ID;
	if(args.Length() >= 2)
	{
		ID = CJSKadID::FromValue(args[1]);
		if (ID == 0)
		{
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
			return;
		}
	}

	CRouteSession* pSession = jRoute->m_pRoute->OpenSession(pEntityID->GetCopy(), (ID != 0) ? ID : jRoute->m_pRoute->GetID());

	CJSObject* jObject = jRoute->m_pScript->SetObject(pSession); // Note: the instance handler for this object is persistent!
	args.GetReturnValue().Set(jObject->GetInstance());
#else
	args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Access Denided"));
#endif
}

void CJSKadRoute::GetTargetID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadRoute* jRoute = GetJSObject<CJSKadRoute>(info.Holder());
	CJSObject* jObject = CJSKadID::New(new CKadIDObj(jRoute->m_pRoute->GetID()), jRoute->m_pScript);
	info.GetReturnValue().Set(jObject->GetInstance());
}

void CJSKadRoute::GetEntityID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadRoute* jRoute = GetJSObject<CJSKadRoute>(info.Holder());
	CJSObject* jObject = CJSKadID::New(new CKadIDObj(jRoute->m_pRoute->GetEntityID()), jRoute->m_pScript);
	info.GetReturnValue().Set(jObject->GetInstance());
}

