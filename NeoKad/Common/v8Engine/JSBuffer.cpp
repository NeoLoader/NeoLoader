#include "GlobalHeader.h"
#include "JSBuffer.h"
#include "../../../Framework/Exception.h"
#include "../../../Framework/Strings.h"
#include "../Variant.h"

IMPLEMENT_OBJECT(CBufferObj, CObject)


///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSBuffer::m_Template;

v8::Local<v8::ObjectTemplate> CJSBuffer::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"read"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxRead),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"write"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxWrite),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"readValue"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxReadValue),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"writeValue"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxWriteValue),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"readString"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxReadString),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"writeString"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxWriteString),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"seek"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxSeek),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"resize"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxResize),v8::ReadOnly);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"length"), GetLength, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"position"), GetPosition, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	v8::Local<v8::ObjectTemplate> ConstructorTemplate = v8::ObjectTemplate::New();
	ConstructorTemplate->SetInternalFieldCount(2);

	ConstructorTemplate->SetCallAsFunctionHandler(CJSBuffer::JSBuffer);

	return HandleScope.Escape(ConstructorTemplate);
}

/** new Buffer:
* Creates a new Buffer object
* 
* @param string hex
*	Optional Hex Encoded payload to be put into the new buffer
*
* @return object
*	New Buffer object
*/
void CJSBuffer::JSBuffer(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSScript* jScript = GetJSObject<CJSScript>(args.Holder());
	CBufferObj* pBuffer = new CBufferObj();
	if(args.Length() > 0)
		*pBuffer = FromHex(CJSEngine::GetWStr(args[0]));
	CJSBuffer* jObject = new CJSBuffer(pBuffer, jScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

#define BUFFER_TRY		try {
#define BUFFER_CATCH	} catch(const CException& Exception) { args.GetIsolate()->ThrowException(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Exception.GetLine().c_str()))); return;}
#define BUFFER_CATCH_X	} catch(const CException& Exception) {}

/** read:
* read data from the current bufer to a new buffer object
* 
* @param int togo
*	bytes to be read
*
* @return object
*	New Buffer containing the read data
*/
void CJSBuffer::FxRead(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBuffer* jBuffer = GetJSObject<CJSBuffer>(args.Holder());
	CBufferObj* pBuffer = jBuffer->m_pBuffer;
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	BUFFER_TRY
	size_t ToGo = args[0]->Uint32Value();
	CBuffer Buffer(ToGo);
	Buffer.WriteData(pBuffer->ReadData(ToGo), ToGo);
	CJSObject* jObject = CJSBuffer::New(new CBufferObj(Buffer), jBuffer->m_pScript);
	args.GetReturnValue().Set(jObject->GetInstance());
	BUFFER_CATCH
}

/** write:
* write data from one buffer to an other
* 
* @param Buffer Object data
*	buffer to be writen into the current buffer
*/
void CJSBuffer::FxWrite(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBuffer* jBuffer = GetJSObject<CJSBuffer>(args.Holder());
	CBufferObj* pBuffer = jBuffer->m_pBuffer;
	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	CBufferObj* pData = GetCObject<CBufferObj>(args[0]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	BUFFER_TRY
	pBuffer->WriteData(pData->GetBuffer(), pData->GetSize());
	args.GetReturnValue().SetUndefined();
	BUFFER_CATCH
}

/** readValue:
* read a value of a given type from the buffer
*	Note: uint64 and int64 will be returned as doubles as JS does not support 64bit integers, the rest will be returned as 32bit integers
* 
* @param string type
*	value type to be read: uint64, int64/sint64, uint32, int32/sint32, uint16, int16/sint16, uint8/byte, int8/sint8/char
*
* @return object
*	read value
*/
void CJSBuffer::FxReadValue(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBuffer* jBuffer = GetJSObject<CJSBuffer>(args.Holder());
	CBufferObj* pBuffer = jBuffer->m_pBuffer;
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	BUFFER_TRY
	wstring Type = CJSEngine::GetWStr(args[0]);
	if(CompareStr(Type, L"uint64"))
		args.GetReturnValue().Set(v8::Number::New(v8::Isolate::GetCurrent(), pBuffer->ReadValue<uint64>()));
	else if(CompareStr(Type, L"int64") || CompareStr(Type, L"sint64"))
		args.GetReturnValue().Set(v8::Number::New(v8::Isolate::GetCurrent(), pBuffer->ReadValue<sint64>()));
	else if(CompareStr(Type, L"uint32"))
		args.GetReturnValue().Set(v8::Uint32::New(v8::Isolate::GetCurrent(), pBuffer->ReadValue<uint32>()));
	else if(CompareStr(Type, L"int32") || CompareStr(Type, L"sint32"))
		args.GetReturnValue().Set(v8::Int32::New(v8::Isolate::GetCurrent(), pBuffer->ReadValue<sint32>()));
	else if(CompareStr(Type, L"uint16"))
		args.GetReturnValue().Set(v8::Uint32::New(v8::Isolate::GetCurrent(), pBuffer->ReadValue<uint16>()));
	else if(CompareStr(Type, L"int16") || CompareStr(Type, L"sint16"))
		args.GetReturnValue().Set(v8::Int32::New(v8::Isolate::GetCurrent(), pBuffer->ReadValue<sint16>()));
	else if(CompareStr(Type, L"uint8") || CompareStr(Type, L"byte"))
		args.GetReturnValue().Set(v8::Uint32::New(v8::Isolate::GetCurrent(), pBuffer->ReadValue<uint8>()));
	else if(CompareStr(Type, L"int8") || CompareStr(Type, L"sint8") || CompareStr(Type, L"char"))
		args.GetReturnValue().Set(v8::Int32::New(v8::Isolate::GetCurrent(), pBuffer->ReadValue<sint8>()));
	else
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
	BUFFER_CATCH
}

/** writeValue:
* write a value of a given type to the buffer
* 
* @param string type
*	value type to be read: uint64, int64/sint64, uint32, int32/sint32, uint16, int16/sint16, uint8/byte, int8/sint8/char
*
* @param value to be writen
*/
void CJSBuffer::FxWriteValue(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBuffer* jBuffer = GetJSObject<CJSBuffer>(args.Holder());
	CBufferObj* pBuffer = jBuffer->m_pBuffer;
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	BUFFER_TRY
	wstring Type = CJSEngine::GetWStr(args[0]);
	if(CompareStr(Type, L"uint64"))
		pBuffer->WriteValue<uint64>(args[1]->NumberValue());
	else if(CompareStr(Type, L"int64") || CompareStr(Type, L"sint64"))
		pBuffer->WriteValue<sint64>(args[1]->NumberValue());
	else if(CompareStr(Type, L"uint32"))
		pBuffer->WriteValue<uint32>(args[1]->Uint32Value());
	else if(CompareStr(Type, L"int32") || CompareStr(Type, L"sint32"))
		pBuffer->WriteValue<sint32>(args[1]->Int32Value());
	else if(CompareStr(Type, L"uint16"))
		pBuffer->WriteValue<uint16>(args[1]->Uint32Value());
	else if(CompareStr(Type, L"int16") || CompareStr(Type, L"sint16"))
		pBuffer->WriteValue<sint16>(args[1]->Int32Value());
	else if(CompareStr(Type, L"uint8") || CompareStr(Type, L"byte"))
		pBuffer->WriteValue<uint8>(args[1]->Uint32Value());
	else if(CompareStr(Type, L"int8") || CompareStr(Type, L"sint8") || CompareStr(Type, L"char"))
		pBuffer->WriteValue<sint8>(args[1]->Int32Value());
	else if(CompareStr(Type, L"str"))
		pBuffer->WriteString(CJSEngine::GetWStr(args[1]));
	else
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
	BUFFER_CATCH
}

/** readString:
* read a UTF8 encoded string from the buffer
* 
* @return object
*	read string
*/
void CJSBuffer::FxReadString(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBuffer* jBuffer = GetJSObject<CJSBuffer>(args.Holder());
	CBufferObj* pBuffer = jBuffer->m_pBuffer;

	BUFFER_TRY
		args.GetReturnValue().Set(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(pBuffer->ReadString().c_str())));
	BUFFER_CATCH
}

/** writeString:
* write a UTF8 encoded string to the buffer
* 
* @param string to be writen
*/
void CJSBuffer::FxWriteString(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBuffer* jBuffer = GetJSObject<CJSBuffer>(args.Holder());
	CBufferObj* pBuffer = jBuffer->m_pBuffer;
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	BUFFER_TRY
	pBuffer->WriteString(CJSEngine::GetWStr(args[0]));
	BUFFER_CATCH
}

/** seek:
* seek a position in the buffer
* 
* @param int pos
*	position to be set
*/
void CJSBuffer::FxSeek(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CBufferObj* pBuffer = GetCObject<CBufferObj>(args.Holder());
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), pBuffer->SetPosition(args[0]->Uint32Value())));
}

/** resize:
* resize the buffer
* 
* @param int new size
*	new buffer size
*/
void CJSBuffer::FxResize(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CBufferObj* pBuffer = GetCObject<CBufferObj>(args.Holder());
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	pBuffer->SetSize(args[0]->Uint32Value(), true);
}

/** length:
* get buffer size
* 
* @return int
*	buffer size
*/
void CJSBuffer::GetLength(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	CBufferObj* pBuffer = GetCObject<CBufferObj>(info.Holder());
	size_t uLength = pBuffer->GetSize();
	ASSERT(uLength < 0xFFFFFFFF);
	info.GetReturnValue().Set(v8::Uint32::New(v8::Isolate::GetCurrent(), (uint32)uLength));
}

/** position:
* get curretn position
* 
* @return int
*	buffer position
*/
void CJSBuffer::GetPosition(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	CBufferObj* pBuffer = GetCObject<CBufferObj>(info.Holder());
	size_t uPosition = pBuffer->GetPosition();
	ASSERT(uPosition < 0xFFFFFFFF);
	info.GetReturnValue().Set(v8::Uint32::New(v8::Isolate::GetCurrent(), (uint32)uPosition));
}

/** toString:
* convert the buffer into a hex encoded string
* 
* @return string
*	Hex Encoded buffer content
*/
void CJSBuffer::FxToString(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CBufferObj* pBuffer = GetCObject<CBufferObj>(args.Holder());

	args.GetReturnValue().Set(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(ToHex(pBuffer->GetBuffer(), pBuffer->GetSize()).c_str())));
}
