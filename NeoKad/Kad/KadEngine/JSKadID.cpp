#include "GlobalHeader.h"
#include "JSKadID.h"
#include "../../Common/v8Engine/JSVariant.h"

IMPLEMENT_OBJECT(CKadIDObj, CObject)

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSKadID::m_Template;

v8::Local<v8::ObjectTemplate> CJSKadID::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxToString), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toVariant"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxToVariant), v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	v8::Local<v8::ObjectTemplate> ConstructorTemplate = v8::ObjectTemplate::New();
	ConstructorTemplate->SetInternalFieldCount(2);

	ConstructorTemplate->SetCallAsFunctionHandler(CJSKadID::JSKadID);

	return HandleScope.Escape(ConstructorTemplate);
}

void CJSKadID::JSKadID(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CUInt128 Value;
	if(args.Length() >= 1)
		Value = FromValue(args[0]);
	CJSScript* jScript = GetJSObject<CJSScript>(args.Holder());
	CJSObject* jObject = CJSVariant::New(new CKadIDObj(Value), jScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

CUInt128 CJSKadID::FromValue(v8::Local<v8::Value> value)
{
    v8::HandleScope HandleScope(v8::Isolate::GetCurrent());

	CUInt128 Value;
	if(value->IsString())
		Value.FromHex(CJSEngine::GetWStr(value));
	else if(value->IsObject())
	{
		if(CKadIDObj* pVariant = GetCObject<CKadIDObj>(value->ToObject()))
			Value = pVariant->m_Value;
		else if(CVariantPrx* pVariant = GetCObject<CVariantPrx>(value->ToObject()))
			Value = pVariant->GetCopy();
	}
	else	
	{
		sint64 iValue = value->IntegerValue();
		if(iValue < 0)
			Value = CUInt128(true);
		else
			Value = CUInt128((sint32)iValue);
	}
	return Value;
}

void CJSKadID::FxToString(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadID* jKadID = GetJSObject<CJSKadID>(args.Holder());

	args.GetReturnValue().Set(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(jKadID->m_pKadID->m_Value.ToHex().c_str())));
}

void CJSKadID::FxToVariant(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadID* jKadID = GetJSObject<CJSKadID>(args.Holder());

	CJSObject* jObject = CJSVariant::New(new CVariantPrx(jKadID->m_pKadID->m_Value), jKadID->m_pScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}
