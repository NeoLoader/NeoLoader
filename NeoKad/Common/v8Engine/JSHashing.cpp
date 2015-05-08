#include "GlobalHeader.h"
#include "JSHashing.h"
#include "JSBuffer.h"
#include "../Variant.h"

IMPLEMENT_OBJECT(CHashingObj, CObject)

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSHashing::m_Template;

v8::Local<v8::ObjectTemplate> CJSHashing::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"reset"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxReset),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"update"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxUpdate),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"finish"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxFinish),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"hash"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxHash),v8::ReadOnly);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"value"), GetValue, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"algorithm"), GetAlgorithm, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	v8::Local<v8::ObjectTemplate> ConstructorTemplate = v8::ObjectTemplate::New();
	ConstructorTemplate->SetInternalFieldCount(2);

	ConstructorTemplate->SetCallAsFunctionHandler(CJSHashing::JSHashing);

	return HandleScope.Escape(ConstructorTemplate);
}

/** new HashFkt:
* Creates a new Hashing object
* 
* @param string alghorytm
*	Hashing algorythm to be used
*
* @return object
*	New Hashing object
*/
void CJSHashing::JSHashing(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSScript* jScript = GetJSObject<CJSScript>(args.Holder());
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	UINT eAlgorithm = CAbstractKey::Str2Algorithm(CJSEngine::GetStr(args[0]));
	if ((eAlgorithm & CAbstractKey::eHashFunkt) == 0)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CHashingObj* pHashing = new CHashingObj(eAlgorithm);
	CJSHashing* jObject = new CJSHashing(pHashing, jScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

/** reset:
* reset hash function
*/
void CJSHashing::FxReset(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CHashingObj* pHashing = GetCObject<CHashingObj>(args.Holder());
	pHashing->Reset();
	args.GetReturnValue().SetUndefined();
}

/** update:
* add new data to the hash function
* 
* @param string or buffer object
*	data to be hashed
*/
void CJSHashing::FxUpdate(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CHashingObj* pHashing = GetCObject<CHashingObj>(args.Holder());

	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Missing"));
		return;
	}
	if(args[0]->IsString())
	{
		wstring wstr = CJSEngine::GetWStr(args[0]);
		string str;
		WStrToUtf8(str, wstr);
		pHashing->Add((byte*)str.c_str(), str.length());
	}
	else if(args[0]->IsObject())
	{
		CBufferObj* pData = GetCObject<CBufferObj>(args[0]->ToObject());
		if (!pData)
		{
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
			return;
		}
		pHashing->Add(pData);
	}
	else
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
		return;
	}
	args.GetReturnValue().SetUndefined();
}

/** finish;
* finish hash transformation
*/
void CJSHashing::FxFinish(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSHashing* jHashing = GetJSObject<CJSHashing>(args.Holder());

	jHashing->m_pHashing->Finish();
	args.GetReturnValue().SetUndefined();
}

/** finish;
* return calculated hash as buffer
*
* @return buffer object
*	buffer object containing the calculated hash
*/
void CJSHashing::GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSHashing* jHashing = GetJSObject<CJSHashing>(info.Holder());

	CBuffer Buffer(jHashing->m_pHashing->GetKey(), jHashing->m_pHashing->GetSize(), true);
	CJSObject* jObject = CJSBuffer::New(new CBufferObj(Buffer), jHashing->m_pScript);
	info.GetReturnValue().Set(jObject->GetInstance());
}

/** algorithm;
* return set Algorithm
*
* @return string
*	Algorithm as astring
*/
void CJSHashing::GetAlgorithm(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CHashingObj* pHashing = GetCObject<CHashingObj>(info.Holder());
	info.GetReturnValue().Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)CAbstractKey::Algorithm2Str(pHashing->GetAlgorithm()).c_str()));
}

/** toString;
* return calculated hash as hex string
*
* @return string
*	hash string
*/
void CJSHashing::FxToString(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CHashingObj* pHashing = GetCObject<CHashingObj>(args.Holder());

	wstring Key;
	AsciiToWStr(Key, CAbstractKey::Algorithm2Str(pHashing->GetAlgorithm()));
	Key.append(L":" + ToHex(pHashing->GetKey(), pHashing->GetSize()));

	args.GetReturnValue().Set(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Key.c_str())));
}

/** hash;
* calculate a hash from a given input and return the result buffer, 
*	without changinh the shate of the hash function assotiated with the object
*
* @param string or buffer object
*	data to be hashed
*
* @return string
*	hash string
*/
void CJSHashing::FxHash(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSHashing* jHashing = GetJSObject<CJSHashing>(args.Holder());

	CHashFunction Hash(jHashing->m_pHashing->GetAlgorithm());
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Missing"));
		return;
	}
	if(args[0]->IsString())
	{
		wstring wstr = CJSEngine::GetWStr(args[0]);
		string str;
		WStrToUtf8(str, wstr);
		Hash.Add((byte*)str.c_str(), str.length());
	}
	else if(args[0]->IsObject())
	{
		CBufferObj* pData = GetCObject<CBufferObj>(args[0]->ToObject());
		if (!pData)
		{
			args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
			return;
		}
		Hash.Add(pData);
	}
	else
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Payload Incompatible"));
		return;
	}
	Hash.Finish();

	CBuffer Buffer(Hash.GetKey(), Hash.GetSize(), true);
	CJSObject* jObject = CJSBuffer::New(new CBufferObj(Buffer), jHashing->m_pScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}