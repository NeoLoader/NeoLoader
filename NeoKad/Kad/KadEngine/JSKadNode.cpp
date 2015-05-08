#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "../Kademlia.h"
#include "JSKadNode.h"
#include "JSKadID.h"
#include "KadScript.h"
#include "../../Common/v8Engine/JSScript.h"
#include "../../Common/v8Engine/JSVariant.h"

v8::Persistent<v8::ObjectTemplate> CJSKadNode::m_Template;

v8::Local<v8::ObjectTemplate> CJSKadNode::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"nodeID"), GetNodeID, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"sendMessage"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxSendMessage),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"isProxy"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'prxy')),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"hasFailed"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxValue, v8::Integer::New(v8::Isolate::GetCurrent(), 'fail')),v8::ReadOnly);
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"shares"), GetValue, 0, v8::Integer::New(v8::Isolate::GetCurrent(), 'shre'), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"lookup"), GetLookup, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

void CJSKadNode::GetNodeID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadNode* jNode = GetJSObject<CJSKadNode>(info.Holder());
	CKadNode* pNode = jNode->m_pNode; // this is a week poitner - the node may be gone
	if (!pNode)
	{
		info.GetReturnValue().SetUndefined();
		return;
	}
	CJSObject* jObject = CJSKadID::New(new CKadIDObj(pNode->GetID()), jNode->m_pScript);
	info.GetReturnValue().Set(jObject->GetInstance());
}

void CJSKadNode::GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadNode* jNode = GetJSObject<CJSKadNode>(info.Holder());
	CKadNode* pNode = jNode->m_pNode; // this is a week poitner - the node may be gone
	CKadOperation* pLookup = jNode->m_pLookup;
	if (!pNode || !pLookup)
	{
		info.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	CKadOperation::SOpProgress* pProgress = pLookup->GetProgress(pNode);
	if (!pProgress)
	{
		info.GetReturnValue().SetUndefined();
		return;
	}

	switch(info.Data()->Int32Value())
	{
		case 'shre': info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pProgress->Shares)); break;
		default: info.GetReturnValue().SetUndefined();
	}
}

/*void CJSKadNode::SetValue(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadNode* jNode = GetJSObject<CJSKadNode>(info.Holder());
	CKadNode* pNode = jNode->m_pNode; // this is a week poitner - the node may be gone
	CKadOperation* pLookup = jNode->m_pLookup;
	if(!pNode || !pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	switch(info.Data()->Int32Value())
	{

	}
}*/

void CJSKadNode::FxSendMessage(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadNode* jNode = GetJSObject<CJSKadNode>(args.Holder());
	CKadNode* pNode = jNode->m_pNode; // this is a week poitner - the node may be gone
	CKadOperation* pLookup = jNode->m_pLookup;
	if (!pNode || !pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Arguments"));
		return;
	}

	string Name = CJSEngine::GetStr(args[0]);
	CVariantPrx* pData = GetCObject<CVariantPrx>(args[1]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), pLookup->SendMessage(Name, pData->GetCopy(), pNode)));
}

void CJSKadNode::FxValue(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadNode* jNode = GetJSObject<CJSKadNode>(args.Holder());
	CKadNode* pNode = jNode->m_pNode; // this is a week poitner - the node may be gone
	CKadOperation* pLookup = jNode->m_pLookup;
	if (!pNode || !pLookup)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}
	CKadOperation::SOpProgress* pProgress = pLookup->GetProgress(pNode);
	if (!pProgress)
	{
		args.GetReturnValue().SetUndefined();
		return;
	}

	uint32_t Type = args.Data()->Int32Value();
	switch(Type)
	{
		case 'prxy': args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), pProgress->OpState != CKadOperation::eOpProxy)); break;
		case 'fail': args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), pProgress->OpState != CKadOperation::eOpFailed)); break;
		default: args.GetReturnValue().SetUndefined();
	}
}

void CJSKadNode::GetLookup(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSKadNode* jNode = GetJSObject<CJSKadNode>(info.Holder());
	CKadNode* pNode = jNode->m_pNode; // this is a week poitner - the node may be gone
	CKadOperation* pLookup = jNode->m_pLookup;
	if (!pNode || !pLookup)
	{
		info.GetReturnValue().SetUndefined();
		return;
	}

	info.GetReturnValue().Set(jNode->m_pScript->GetObject(pLookup));
}
