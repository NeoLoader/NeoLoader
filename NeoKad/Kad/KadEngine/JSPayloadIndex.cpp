#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "../Kademlia.h"
#include "JSPayloadIndex.h"
#include "JSKadID.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "../../Common/v8Engine/JSCryptoKey.h"

IMPLEMENT_OBJECT(CPayloadIndexObj, CObject)

v8::Persistent<v8::ObjectTemplate> CJSPayloadIndex::m_Template;

v8::Local<v8::ObjectTemplate> CJSPayloadIndex::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"store"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxStore),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"list"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxList),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"load"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxLoad),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"refresh"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxRefresh),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"remove"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxRemove),v8::ReadOnly);
	
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

void CJSPayloadIndex::FxStore(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSPayloadIndex* jIndex = GetJSObject<CJSPayloadIndex>(args.Holder());
	CPayloadIndexObj* pIndexObj = jIndex->m_pIndex;
	if (args.Length() < 3 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	if (!pIndexObj->GetExclusiveCID().IsValid()) // No write access to public index
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Access Denided"));
		return;
	}

	CPayloadIndex* pIndex = pIndexObj->Index();
	if (!pIndex)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	CUInt128 ID = CJSKadID::FromValue(args[0]);
	if (ID == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	string Path = CJSEngine::GetStr(args[1]);
	CVariant Data = CJSVariant::FromValue(args[2]);
	time_t Expire = -1;
	if(args.Length() >= 4)
		Expire = args[3]->NumberValue();

	/*CCryptoKeyObj* pCryptoKey = NULL;
	if(args.Length() >= 5)
	{
		pCryptoKey = GetCObject<CCryptoKeyObj>(args[4]->ToObject());
		if(!pCryptoKey)
			return args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
	}*/

	CVariant Load = pIndex->Store(ID, Path, Data, Expire, pIndexObj->GetExclusiveCID());
	args.GetReturnValue().Set(CJSVariant::ToValue(Load, jIndex->m_pScript));
}

void CJSPayloadIndex::FxList(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CPayloadIndexObj* pIndexObj = GetCObject<CPayloadIndexObj>(args.Holder());
	if (args.Length() < 2 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CPayloadIndex* pIndex = pIndexObj->Index();
	if (!pIndex)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	CUInt128 ID = CJSKadID::FromValue(args[0]);
	if (ID == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	string Path = CJSEngine::GetStr(args[1]);

	vector<SKadEntryInfo> Entrys;
	pIndex->List(ID, Path, Entrys, pIndexObj->GetExclusiveCID());
	v8::Local<v8::Array> Array = v8::Array::New(v8::Isolate::GetCurrent(), (int)Entrys.size());
	for(uint32 i=0; i < Entrys.size(); i++)
	{
		v8::Local<v8::Object> Entry = v8::Object::New(v8::Isolate::GetCurrent());
		Entry->SetHiddenValue(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"index"),v8::Number::New(v8::Isolate::GetCurrent(), Entrys[i].Index));
		Entry->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"path"),v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)Entrys[i].Path.c_str()));
		Entry->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"date"),v8::Number::New(v8::Isolate::GetCurrent(), Entrys[i].Date));
		Entry->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"expire"),v8::Number::New(v8::Isolate::GetCurrent(), Entrys[i].Expire));
		Array->Set(i, Entry);
	}
	args.GetReturnValue().Set(Array);
}

void CJSPayloadIndex::FxLoad(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSPayloadIndex* jIndex = GetJSObject<CJSPayloadIndex>(args.Holder());
	CPayloadIndexObj* pIndexObj = jIndex->m_pIndex;
	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CPayloadIndex* pIndex = pIndexObj->Index();
	if (!pIndex)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	//
	int Index = 0;
	if(args.Length() >= 2)
	{
		CUInt128 ID = CJSKadID::FromValue(args[0]);
		if (ID == 0)
		{
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
			return;
		}

		string Path = CJSEngine::GetStr(args[1]);

		Index = pIndex->Find(ID, Path, pIndexObj->GetExclusiveCID());
	}
	else
		Index = args[0]->ToObject()->GetHiddenValue(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"index"))->NumberValue();
	//

	CVariant Result = pIndex->Load(Index, pIndexObj->GetExclusiveCID());
	if(Result.IsValid())
		args.GetReturnValue().Set(CJSVariant::ToValue(Result, jIndex->m_pScript));
	else
		args.GetReturnValue().SetNull();
}

void CJSPayloadIndex::FxRefresh(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CPayloadIndexObj* pIndexObj = GetCObject<CPayloadIndexObj>(args.Holder());
	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	if (!pIndexObj->GetExclusiveCID().IsValid()) // No write access to public index
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Access Denided"));
		return;
	}

	CPayloadIndex* pIndex = pIndexObj->Index();
	if (!pIndex)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	//
	int Index = 0;
	if(args.Length() >= 2)
	{
		CUInt128 ID = CJSKadID::FromValue(args[0]);
		if (ID == 0)
		{
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
			return;
		}

		string Path = CJSEngine::GetStr(args[1]);

		Index = pIndex->Find(ID, Path, pIndexObj->GetExclusiveCID());
	}
	else
		Index = args[0]->ToObject()->GetHiddenValue(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"index"))->NumberValue();
	//

	pIndex->Refresh(Index, pIndexObj->GetExclusiveCID());
	args.GetReturnValue().SetUndefined();
}

void CJSPayloadIndex::FxRemove(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CPayloadIndexObj* pIndexObj = GetCObject<CPayloadIndexObj>(args.Holder());
	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	if (!pIndexObj->GetExclusiveCID().IsValid()) // No write access to public index
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Access Denided"));
		return;
	}

	CPayloadIndex* pIndex = pIndexObj->Index();
	if (!pIndex)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Object"));
		return;
	}

	//
	int Index = 0;
	if(args.Length() >= 2)
	{
		CUInt128 ID = CJSKadID::FromValue(args[0]);
		if (ID == 0)
		{
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
			return;
		}

		string Path = CJSEngine::GetStr(args[1]);

		Index = pIndex->Find(ID, Path, pIndexObj->GetExclusiveCID());
	}
	else
		Index = args[0]->ToObject()->GetHiddenValue(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"index"))->NumberValue();
	//

	/*CCryptoKeyObj* pCryptoKey = NULL;
	if(args.Length() >= )
	{
		pCryptoKey = GetCObject<CCryptoKeyObj>(args[ ]->ToObject());
		if(!pCryptoKey)
			return args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
	}*/

	pIndex->Remove(Index, pIndexObj->GetExclusiveCID());
	args.GetReturnValue().SetUndefined();
}
