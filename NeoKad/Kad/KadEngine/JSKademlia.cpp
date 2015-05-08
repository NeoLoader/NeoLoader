#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "JSKademlia.h"
#include "JSPayloadIndex.h"
#include "JSKadID.h"
#include "../Kademlia.h"
#include "../../Common/v8Engine/JSScript.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "../../Common/v8Engine/JSCryptoKey.h"
#include "KadScript.h"
#include "JSKadRoute.h"

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSKademlia::m_Template;

v8::Local<v8::ObjectTemplate> CJSKademlia::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"index"), GetIndex, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"setupRoute"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxSetupRoute), v8::ReadOnly);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"nodeID"), GetNodeID, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

void CJSKademlia::GetIndex(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKademlia* jKademlia = GetJSObject<CJSKademlia>(info.Holder());
	CPointer<CPayloadIndexObj> pIndex = new CPayloadIndexObj(jKademlia->m_pKademlia->Index());
	info.GetReturnValue().Set(jKademlia->m_pScript->GetObject(pIndex)); // get existing object or 
}

void CJSKademlia::FxSetupRoute(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKademlia* jKademlia = GetJSObject<CJSKademlia>(args.Holder());
	CKadScript* pScript = jKademlia->m_pScript->GetParent<CKadScript>();
	CCryptoKeyObj* pCryptoKey = (args.Length() < 1 || !args[0]->IsObject()) ? NULL : GetCObject<CCryptoKeyObj>(args[0]->ToObject());
	if(CKadRouteObj* pRoute = pScript->SetupRoute(pCryptoKey))
		args.GetReturnValue().Set(jKademlia->m_pScript->GetObject(pRoute));
	else
		args.GetReturnValue().SetNull();
}

void CJSKademlia::GetNodeID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKademlia* jKademlia = GetJSObject<CJSKademlia>(info.Holder());
	CJSObject* jObject = CJSKadID::New(new CKadIDObj(jKademlia->m_pKademlia->GetID()), jKademlia->m_pScript);
	info.GetReturnValue().Set(jObject->GetInstance());
}