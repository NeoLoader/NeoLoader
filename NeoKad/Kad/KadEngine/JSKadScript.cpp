#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "../Kademlia.h"
#include "JSKadScript.h"
#include "KadEngine.h"
#include "JSPayloadIndex.h"
#include "../../Common/v8Engine/JSScript.h"
#include "../../Common/v8Engine/JSDataStore.h"
#include "../../Common/v8Engine/JSCryptoKey.h"
#include "KadDebugging.h"

v8::Persistent<v8::ObjectTemplate> CJSKadScript::m_Template;

v8::Local<v8::ObjectTemplate> CJSKadScript::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"storage"), GetData, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"index"), GetIndex, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"engineVersion"), GetEngineVersion, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"secretKey"), GetSecretKey, SetSecretKey); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"invoke"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxInvoke),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"setInterval"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxTimer,v8::Integer::New(v8::Isolate::GetCurrent(), 'setI')));
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"clearInterval"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxTimer,v8::Integer::New(v8::Isolate::GetCurrent(), 'clrI')));
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"setTimeout"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxTimer,v8::Integer::New(v8::Isolate::GetCurrent(), 'setT')));
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"clearTimeout"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxTimer,v8::Integer::New(v8::Isolate::GetCurrent(), 'clrT')));

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

void CJSKadScript::GetData(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadScript* jKadScript = GetJSObject<CJSKadScript>(info.Holder());
	CDataStoreObj* pData = jKadScript->m_pKadScript->GetData();
	info.GetReturnValue().Set(jKadScript->m_pScript->GetObject(pData)); // get existing object or 
}

void CJSKadScript::GetIndex(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadScript* jKadScript = GetJSObject<CJSKadScript>(info.Holder());
	CPointer<CPayloadIndexObj> pIndex = new CPayloadIndexObj(jKadScript->m_pScript->GetParent<CKademlia>()->Index(), jKadScript->m_pKadScript->GetCodeID());
	info.GetReturnValue().Set(jKadScript->m_pScript->GetObject(pIndex)); // get existing object or 
}

void CJSKadScript::GetEngineVersion(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	info.GetReturnValue().Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"0.1"));
}

void CJSKadScript::GetSecretKey(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadScript* jScript = GetJSObject<CJSKadScript>(info.Holder());
	CKadScript* pScript = jScript->m_pKadScript;

	if(CAbstractKey* pSecretKey = pScript->GetSecretKey())
	{
		CCryptoKeyObj* pCryptoKey = new CCryptoKeyObj();
		pCryptoKey->SetAlgorithm(pSecretKey->GetAlgorithm());
		pCryptoKey->SetKey(pSecretKey);
		CJSObject* jObject = CJSSymKey::New(pCryptoKey, jScript->m_pScript);
		info.GetReturnValue().Set(jObject->GetInstance());
	}
	else
		info.GetReturnValue().SetNull();
}

void CJSKadScript::SetSecretKey(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CKadScript* pScript = GetCObject<CKadScript>(info.Holder());
	CCryptoKeyObj* pSecretKey = value->IsObject() ? GetCObject<CCryptoKeyObj>(value->ToObject()) : NULL;
	if(!pSecretKey)
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	pScript->SetSecretKey(pSecretKey);
}

void CJSKadScript::FxInvoke(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CKadScript* pScript = GetCObject<CKadScript>(args.Holder());
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	
	CVariant CodeID = CJSVariant::FromValue(args[0]);
	if(CKadScript* pKadScript = pScript->GetParent<CKademlia>()->Engine()->GetScript(CodeID))
	{
		//CDebugScope Debug; //this event is to be associated with the callers debug scope
		ASSERT(!CDebugScope::IsEmpty());

		CVariant Arguments;
		if(args.Length() >= 3)
			Arguments = CJSVariant::FromValue(args[2]);
		vector<CPointer<CObject> > Parameters;
		Parameters.push_back(new CVariantPrx(Arguments));
		Parameters.push_back(new CVariantPrx(pScript->GetCodeID()));

		try
		{
			CJSScript* pScript = pKadScript->GetJSScript(true);

			if (!pScript->Has("localAPI", CJSEngine::GetStr(args[1])))
			{
				args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"localAPI NoFunction"));
				return;
			}

			CPointer<CObject> Return;
			pScript->Call(string("localAPI"), CJSEngine::GetStr(args[1]), Parameters, Return);
			
			CJSKadScript* jKadScript = GetJSObject<CJSKadScript>(args.Holder());
			args.GetReturnValue().Set(jKadScript->m_pScript->GetObject(Return));
			return;
		}
		catch(const CJSException& Exception)
		{
			pKadScript->LogLine(Exception.GetFlag(), L"localAPI call returned an error: %s", Exception.GetLine().c_str());
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)Exception.GetError().c_str()));
		}
	}
	args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"localAPI NoScript"));
}

void CJSKadScript::FxTimer(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadScript* jKadScript = GetJSObject<CJSKadScript>(args.Holder());
	uint32_t Type = args.Data()->Int32Value();
	switch(Type)
	{
		case 'setI':
		case 'setT':
			if(args.Length() >= 2)
				args.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), jKadScript->SetTimer(args[0], args[1]->Int32Value(), Type == 'setI')));
			break;
		case 'clrI':
		case 'clrT':
			if(args.Length() >= 1)
				jKadScript->ClearTimer(args[0]->Int32Value());
			break;
		default:
			ASSERT(0);
			args.GetReturnValue().SetUndefined();
	}
}

v8::Local<v8::Object> CJSKadScript::ThisObject()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSScript* pScript = m_pKadScript->GetJSScript();
	if(!pScript) // if a script gets terminated all routes get closed to, this should not happen
	{
		ASSERT(0); // this should not happen
		return HandleScope.Escape(v8::Object::New(v8::Isolate::GetCurrent()));
	}
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), pScript->GetContext());
	return HandleScope.Escape(Context->Global());
}