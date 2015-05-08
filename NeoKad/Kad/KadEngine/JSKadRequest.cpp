#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "JSKadRequest.h"
#include "../Kademlia.h"
#include "../KadConfig.h"
#include "../../../Framework/Strings.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "../../Common/v8Engine/JSBuffer.h"

v8::Persistent<v8::ObjectTemplate> CJSKadRequest::m_Template;

v8::Local<v8::ObjectTemplate> CJSKadRequest::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"addResponse"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxAddResponse),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"hasMore"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'more')),v8::ReadOnly);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"name"), GetValue, 0, v8::Integer::New(v8::Isolate::GetCurrent(), 'name')); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	/*v8::Local<v8::ObjectTemplate> ConstructorTemplate = v8::ObjectTemplate::New();
	ConstructorTemplate->SetInternalFieldCount(2);

	ConstructorTemplate->SetCallAsFunctionHandler(CJSBinaryBlock::JSBinaryBlock);

	return HandleScope.Escape(ConstructorTemplate);*/
	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

/*v8::Local<v8::Value> CJSKadRequest::JSKadRequest(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBinaryBlock* jObject = new CJSBinaryBlock(new CBinaryBlock());
	return HandleScope.Close(jObject->GetInstance());
}*/

void CJSKadRequest::FxAddResponse(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadRequest* jRequest = GetJSObject<CJSKadRequest>(args.Holder());
	CKadRequest* pRequest = jRequest->m_pRequest;
	if (!pRequest)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	bool bMore = false;
	if(args.Length() >= 2)
		bMore = args[1]->BooleanValue();

	CVariant Arguments = CJSVariant::FromValue(args[0]);
	if (!pRequest->AddResponse(Arguments, bMore))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Operation")); // this request was issued by this script it can not answert to it self
		return;
	}
	args.GetReturnValue().SetUndefined();
}

void CJSKadRequest::FxValue(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadRequest* jRequest = GetJSObject<CJSKadRequest>(args.Holder());
	CKadRequest* pRequest = jRequest->m_pRequest;
	if (!pRequest)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	uint32_t Type = args.Data()->Int32Value();
	switch(Type)
	{
		case 'more': args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), pRequest->HasMore())); break;
		default: args.GetReturnValue().SetUndefined();
	}
}

void CJSKadRequest::GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadRequest* jRequest = GetJSObject<CJSKadRequest>(info.Holder());
	CKadRequest* pRequest = jRequest->m_pRequest;
	if (!pRequest)
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	switch(info.Data()->Int32Value())
	{
		case 'name': info.GetReturnValue().Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)pRequest->GetName().c_str())); break;
		default: info.GetReturnValue().SetUndefined();
	}
}

void CJSKadRequest::SetValue(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	/*v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadRequest* jRequest = GetJSObject<CJSKadRequest>(info.Holder());
	CKadRequest* pRequest = jRequest->m_pRequest;
	if(!pRequest)
		return args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));

	switch(info.Data()->Int32Value())
	{
		
	}*/
}