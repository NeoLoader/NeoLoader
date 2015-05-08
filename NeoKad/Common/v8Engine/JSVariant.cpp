#include "GlobalHeader.h"
#include "JSVariant.h"
#include "JSBuffer.h"
#include "JSCryptoKey.h"
#include "../../Kad/UIntX.h"
#include "../../../Framework/Strings.h"

IMPLEMENT_OBJECT(CVariantRef, CObject)
IMPLEMENT_OBJECT(CVariantPrx, CObject)

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSVariant::m_Template;

v8::Local<v8::ObjectTemplate> CJSVariant::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->SetNamedPropertyHandler(GetByName, SetByName, QueryByName, DelByName, EnumNames);
	InstanceTemplate->SetIndexedPropertyHandler(GetAtIndex, SetAtIndex, QueryAtIndex, DelAtIndex, EnumIndexes);

	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"length"), GetLength, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"append"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxAppend), v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"sign"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxSign), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"verify"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxVerify), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"encrypt"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxEncrypt), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"decrypt"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxDecrypt), v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"convert"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxConvert), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"clear"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxClear), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"freeze"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxFreeze), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"unfreeze"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxUnfreeze), v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"isValid"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxIsValid), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"isSigned"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxIsSigned), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"isEncrypted"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxIsEncrypted), v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"fromPacket"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxFromPacket), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toPacket"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxToPacket), v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"fromCode"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxFromCode), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toCode"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxToCode), v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxToString), v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	v8::Local<v8::ObjectTemplate> ConstructorTemplate = v8::ObjectTemplate::New();
	ConstructorTemplate->SetInternalFieldCount(2);

	ConstructorTemplate->SetCallAsFunctionHandler(CJSVariant::JSVariant);

	return HandleScope.Escape(ConstructorTemplate);
}

/** new Variant:
* Creates a new Variant object
* 
* @param value
*	optional value to create the variant from
*
* @return object
*	New Variant object
*/
void CJSVariant::JSVariant(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CVariant Variant;
	if(args.Length() >= 1)
		Variant = FromValue(args[0]);
	CJSScript* jScript = GetJSObject<CJSScript>(args.Holder());
	CJSObject* jObject = CJSVariant::New(new CVariantPrx(Variant), jScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

v8::Local<v8::Value> CJSVariant::ToValue(const CVariant& Variant, CJSScript* pScript)
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	switch(Variant.GetType())
	{
		case CVariant::EUtf8:
			return HandleScope.Escape(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Variant.To<wstring>().c_str())));
		case CVariant::EAscii:
			return HandleScope.Escape(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)Variant.To<string>().c_str()));

		case CVariant::ESInt:
			if(Variant.GetSize() <= sizeof(sint32))
				return HandleScope.Escape(v8::Integer::New(v8::Isolate::GetCurrent(), Variant.To<sint32>()));
			else if(Variant.GetSize() <= sizeof(sint64))
				return HandleScope.Escape(v8::Number::New(v8::Isolate::GetCurrent(), (double)Variant.To<sint64>()));
			break;

		case CVariant::EUInt:
			if(Variant.GetSize() <= sizeof(sint32))
				return HandleScope.Escape(v8::Integer::New(v8::Isolate::GetCurrent(), Variant.To<uint32>()));
			else if(Variant.GetSize() <= sizeof(sint64))
				return HandleScope.Escape(v8::Number::New(v8::Isolate::GetCurrent(), (double)Variant.To<uint64>()));
			break;

		case CVariant::EDouble:
			return HandleScope.Escape(v8::Number::New(v8::Isolate::GetCurrent(), Variant.To<double>()));
	}

	if(pScript)
	{
		switch(Variant.GetType())
		{
			case CVariant::EBytes:
			{
				CJSObject* jObject = CJSBuffer::New(new CBufferObj(Variant), pScript);
				v8::Local<v8::Object> Value = v8::Local<v8::Object>::New(v8::Isolate::GetCurrent(), jObject->GetInstance());
				return HandleScope.Escape(Value);
			}
		}

		CJSObject* jObject = CJSVariant::New(new CVariantPrx(Variant), pScript);
		v8::Local<v8::Object> Value = v8::Local<v8::Object>::New(v8::Isolate::GetCurrent(), jObject->GetInstance());
		return HandleScope.Escape(Value);
	}

	// for toString return payload as hex
	return HandleScope.Escape(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(ToHex(Variant.GetData(), Variant.GetSize()).c_str())));
}

CVariant CJSVariant::FromValue(v8::Local<v8::Value> value_obj)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());

	if(value_obj->IsBoolean())
		return value_obj->BooleanValue();
	if(value_obj->IsString())
		return CJSEngine::GetWStr(value_obj);
	if(value_obj->IsNumber())
		return value_obj->NumberValue();
	if(value_obj->IsInt32())
		return value_obj->Uint32Value();
	if(value_obj->IsUint32())
		return value_obj->Int32Value();

	if(value_obj->IsObject())
	{
		if(CJSVariant* jSetVariant = GetJSObject<CJSVariant>(value_obj->ToObject()))
		{
			const CVariant& SetVariant = jSetVariant->m_pVariant->GetVariant();
			return SetVariant;
		}
		else if(CBufferObj* pSetBuffer = GetCObject<CBufferObj>(value_obj->ToObject()))
			return *pSetBuffer;
		else
			return CJSEngine::GetWStr(value_obj);
	}
	
	return CVariant();
}

#define VARIANT_TRY		try {
#define VARIANT_CATCH	} catch(const CException& Exception) { args.GetIsolate()->ThrowException(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Exception.GetLine().c_str()))); return;}
#define VARIANT_CATCH_	} catch(const CException& Exception) { info.GetIsolate()->ThrowException(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Exception.GetLine().c_str()))); return;}
#define VARIANT_CATCH_X	} catch(const CException&) {}

/** convert:
* convert the variant to a given type
* 
* @param string type
*	type to convert to string/integer/double/binary
*
* @return bool
*	conversion result success/failed
*/
void CJSVariant::FxConvert(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	if (args.Length() != 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	
	wstring To = CJSEngine::GetWStr(args[0]);
	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if(CompareStr(To, L"string"))
		Variant = Variant.To<wstring>();
	else if(CompareStr(To, L"integer"))
	{
		if(Variant.GetSize() == sizeof(uint8))
			Variant = Variant.To<uint8>();
		else if(Variant.GetSize() == sizeof(uint16))
			Variant = Variant.To<uint16>();
		else if(Variant.GetSize() == sizeof(uint32))
			Variant = Variant.To<uint32>();
		else if(Variant.GetSize() == sizeof(uint64))
			Variant = Variant.To<uint64>();
	}
	else if(CompareStr(To, L"double"))
		Variant = Variant.To<uint32>();
	else if(CompareStr(To, L"binary"))
		Variant = CVariant(Variant.GetData(), Variant.GetSize());
	else
	{
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), false));
		return;
	}
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), true));
	VARIANT_CATCH
}

/** clear:
* clear the variant content
*/
void CJSVariant::FxClear(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	
	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	Variant.Clear();
	VARIANT_CATCH
	args.GetReturnValue().SetUndefined();
}

/** freeze:
* fixates a variant in its binary representation and sets it read only
*/
void CJSVariant::FxFreeze(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	
	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	Variant.Freeze();
	VARIANT_CATCH
	args.GetReturnValue().SetUndefined();
}

/** unfreeze:
* makes a prevously fixed variant writable again
*/
void CJSVariant::FxUnfreeze(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	
	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	Variant.Unfreeze();
	VARIANT_CATCH
	args.GetReturnValue().SetUndefined();
}

/** isValid:
* check if the variant has a valid value
*
* @return bool
*	is the variant value valid
*/
void CJSVariant::FxIsValid(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());

	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	return args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), Variant.IsValid()));
	VARIANT_CATCH
}

/** isSigned:
* checks if the variant has a signature
*
* @return bool
*	is the variant value signed
*/
void CJSVariant::FxIsSigned(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());

	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), Variant.IsSigned()));
	VARIANT_CATCH
}

/** isEncrypted:
* check if the variant is encrypted
*
* @return int
*	is the variant value encrypted (0 - None, 1 - Asymmetric, 2- Symmetric)
*/
void CJSVariant::FxIsEncrypted(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());

	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	args.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), Variant.IsEncrypted()));
	VARIANT_CATCH
}

/** fromPacket:
* read a variant form a byte buffer object
*
* @param buffer object
*	buffer object to read the variant from
*
* @return bool
*	is the variant valid
*/
void CJSVariant::FxFromPacket(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	if (args.Length() != 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	CBufferObj* pBuffer = GetCObject<CBufferObj>(args[0]->ToObject());
	if (!pBuffer)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	Variant.FromPacket(pBuffer);
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), Variant.IsValid()));
	VARIANT_CATCH
}

/** toPacket:
* write a variant to a byte buffer object
*
* @param buffer object
*	buffer object to write the variant to
*/
void CJSVariant::FxToPacket(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	if (args.Length() != 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	CBufferObj* pBuffer = GetCObject<CBufferObj>(args[0]->ToObject());
	if (!pBuffer)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	Variant.ToPacket(pBuffer);
	args.GetReturnValue().SetUndefined();
	VARIANT_CATCH
}

/** fromCode:
* read a variant form a hex or other encoded string
*
* @param string code
*
* @param string encoding
*
* @return bool
*	is the variant valid
*/
void CJSVariant::FxFromCode(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	wstring Code = CJSEngine::GetWStr(args[0]);
	wstring Coding = L"hex";
	if(args.Length() >= 2)
		Coding = CJSEngine::GetWStr(args[1]);

	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if(CompareStr(Coding, L"hex"))
		Variant = FromHex(Code);
	else if(CompareStr(Coding, L"kad"))
	{
		CUInt128 ID;
		ID.FromHex(Code);
		Variant = ID;
	}
	else
	{
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), false));
		return;
	}
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), true));
}

/** toCode:
* write a variant to a hex or other encoded string
*
* @param string encoding
*
* @param string code
*/
void CJSVariant::FxToCode(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	wstring Coding = L"hex";
	if(args.Length() >= 1)
		Coding = CJSEngine::GetWStr(args[0]);

	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	wstring Code;
	if(CompareStr(Coding, L"hex"))
		Code = ToHex(Variant.GetData(), Variant.GetSize());
	else if(CompareStr(Coding, L"kad"))
	{
		CUInt128 ID(Variant);
		Code = ID.ToHex();
	}
	else
	{
		args.GetReturnValue().SetUndefined();
		return;
	}
	args.GetReturnValue().Set(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Code.c_str())));
}

/** toString:
* convert the buffer to a string if possible
*
* @return string
*	string representation of the variant
*/
void CJSVariant::FxToString(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());

	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if (Variant.IsMap())
	{
		args.GetReturnValue().Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Map"));
		return;
	}
	if (Variant.IsList())
	{
		args.GetReturnValue().Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"List"));
		return;
	}
	args.GetReturnValue().Set(ToValue(Variant, NULL));
	VARIANT_CATCH
}

void CJSVariant::GetByName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	if (!jVariant || info.Holder()->HasRealNamedProperty(name))
	{
		info.GetReturnValue().Set(v8::Local<v8::Value>());
		return;
	}

	string Name = CJSEngine::GetStr(name);
	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if(!Variant.IsValid() || Variant.IsMap() || Variant.IsList())
	{
		if (!Variant.Has(Name.c_str()))
		{
			info.GetReturnValue().SetUndefined();
			return;
		}
		vector<string> Path = jVariant->m_pVariant->GetPath();
		Path.push_back(Name);
		CJSVariant* jObject = new CJSVariant(new CVariantPrx(jVariant->m_pVariant, Path), jVariant->m_pScript);
		info.GetReturnValue().Set(jObject->GetInstance());
	}
	else
		info.GetReturnValue().SetUndefined();
	VARIANT_CATCH_
}

void CJSVariant::SetByName(v8::Local<v8::String> name, v8::Local<v8::Value> value_obj, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	if (!jVariant || info.Holder()->HasRealNamedProperty(name))
	{
		info.GetReturnValue().Set(v8::Local<v8::Value>());
		return;
	}

	string Name = CJSEngine::GetStr(name);
	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	CVariant& Value = Variant.At(Name.c_str());
	Value = FromValue(value_obj);
	info.GetReturnValue().SetUndefined();
	VARIANT_CATCH_
}

void CJSVariant::QueryByName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	ASSERT(jVariant);

	string Name = CJSEngine::GetStr(name);
	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if (Variant.Has(Name.c_str()))
	{
		info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), v8::None));
		return;
	}
	VARIANT_CATCH_X
	info.GetReturnValue().Set(v8::Local<v8::Integer>());
}

void CJSVariant::DelByName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	if (!jVariant || info.Holder()->HasRealNamedProperty(name))
	{
		info.GetReturnValue().Set(v8::Local<v8::Boolean>());
		return;
	}

	string Name = CJSEngine::GetStr(name);
	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if(Variant.Has(Name.c_str()))
	{
		Variant.Remove(Name.c_str());
		info.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), true));
		return;
	}
	VARIANT_CATCH_X
	info.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), false));
}

void CJSVariant::EnumNames(const v8::PropertyCallbackInfo<v8::Array>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	ASSERT(jVariant);

	v8::Local<v8::Array> Members;
	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if(Variant.IsMap())
	{
		Members = v8::Array::New(v8::Isolate::GetCurrent(), Variant.Count());
		for(uint32 i = 0; i < Variant.Count(); i++)
			Members->Set(v8::Integer::New(v8::Isolate::GetCurrent(), i), v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)Variant.Key(i).c_str()));
	}
	VARIANT_CATCH_X
	info.GetReturnValue().Set(Members);
}

void CJSVariant::GetAtIndex(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	if (!jVariant || info.Holder()->HasRealIndexedProperty(index))
	{
		info.GetReturnValue().Set(v8::Local<v8::Value>());
		return;
	}

	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if(Variant.Count() > index)
	{
		const CVariant& Value = Variant.At(index);

		if(Value.IsMap() || Value.IsList())
		{
			vector<string> Path = jVariant->m_pVariant->GetPath();
			string Name(5,'\0');
			memcpy((char*)Name.data() + 1, &index, 4);
			Path.push_back(Name);
			CJSVariant* jObject = new CJSVariant(new CVariantPrx(jVariant->m_pVariant, Path), jVariant->m_pScript);
			info.GetReturnValue().Set(jObject->GetInstance());
		}
		else
			info.GetReturnValue().Set(ToValue(Value, jVariant->m_pScript));
		return;
	}
	info.GetReturnValue().SetUndefined();
	VARIANT_CATCH_
}

void CJSVariant::SetAtIndex(uint32_t index, v8::Local<v8::Value> value_obj, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	if (!jVariant || info.Holder()->HasRealIndexedProperty(index))
	{
		info.GetReturnValue().Set(v8::Local<v8::Value>());
		return;
	}

	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	CVariant& Value = Variant.At(index);
	Value = FromValue(value_obj);
	info.GetReturnValue().SetUndefined();
	VARIANT_CATCH_
}

void CJSVariant::QueryAtIndex(uint32_t index, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	ASSERT(jVariant);

	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if (Variant.Count() > index)
	{
		info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), v8::None));
		return;
	}
	VARIANT_CATCH_X
	info.GetReturnValue().Set(v8::Local<v8::Integer>());
}

void CJSVariant::DelAtIndex(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	if (!jVariant || info.Holder()->HasRealIndexedProperty(index))
	{
		info.GetReturnValue().Set(v8::Local<v8::Boolean>());
		return;
	}

	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if(Variant.Count() > index)
	{
		Variant.Remove(index);
		info.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), true));
		return;
	}
	VARIANT_CATCH_X
	info.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), false));
}

void CJSVariant::EnumIndexes(const v8::PropertyCallbackInfo<v8::Array>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	ASSERT(jVariant);

	v8::Local<v8::Array> Members;
	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	if(Variant.IsList())
	{
		Members = v8::Array::New(v8::Isolate::GetCurrent(), Variant.Count());
		for(uint32 i = 0; i < Variant.Count(); i++)
			Members->Set(v8::Integer::New(v8::Isolate::GetCurrent(), i), v8::Integer::New(v8::Isolate::GetCurrent(), i)->ToString());
	}
	VARIANT_CATCH_X
	info.GetReturnValue().Set(Members);
}

/** length:
* return intem count of List/Map variants
*
* @return int
*	item count
*/
void CJSVariant::GetLength(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(info.Holder());
	ASSERT(jVariant);

	VARIANT_TRY
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	info.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), Variant.Count()));
	VARIANT_CATCH_
}

/** append:
* append a variant to a variant list
*
* @param variant
*	variant to ba appended
*/
void CJSVariant::FxAppend(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	if (args.Length() != 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	VARIANT_TRY
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	CVariant Value = FromValue(args[0]);
	Variant.Append(Value);
	args.GetReturnValue().SetUndefined();
	VARIANT_CATCH
}

bool CJSVariant::GetCredentials(const v8::FunctionCallbackInfo<v8::Value> &args, CCryptoKeyObj* &pCryptoKey, UINT* eAlgorithm, CCryptoKeyObj** pIV)
{
	if(args.Length() < 1 || !args[0]->IsObject())
		return false;
	
	pCryptoKey = GetCObject<CCryptoKeyObj>(args[0]->ToObject());
	if(!pCryptoKey)
		return false;

	if(args.Length() >= 2)
	{
		if(args[1]->IsString())
		{
			if(eAlgorithm)
				*eAlgorithm = CAbstractKey::Str2Algorithm(CJSEngine::GetStr(args[1]));
		}
		else if(args[1]->IsObject())
		{
			if(pIV)
				*pIV = GetCObject<CCryptoKeyObj>(args[1]->ToObject());
		}
	}

	return true;
}

/** sign:
* sign a variant using a private key
*
* @param key object
*	private key to be used for the signature
*
* @param string 
*	optional algorythm to be used instead of the default one for the given key
*/
void CJSVariant::FxSign(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	VARIANT_TRY
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	CCryptoKeyObj* pCryptoKey;
	UINT eAlgorithm = 0;
	if (!GetCredentials(args, pCryptoKey, &eAlgorithm, NULL))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CPrivateKey PrivKey(pCryptoKey->GetAlgorithm());
	PrivKey.SetKey(pCryptoKey);
	Variant.Sign(&PrivKey, eAlgorithm);

	args.GetReturnValue().SetUndefined();
	VARIANT_CATCH
}

/** verify:
* verify a variant using a public key
*
* @param key object
*	public key to be used for the verification
*
* @return bool
*	signature verification result
*/
void CJSVariant::FxVerify(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	VARIANT_TRY
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	const CVariant& Variant = jVariant->m_pVariant->GetVariant();
	CCryptoKeyObj* pCryptoKey;
	if (!GetCredentials(args, pCryptoKey, NULL, NULL))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	CPublicKey PubKey(pCryptoKey->GetAlgorithm());
	PubKey.SetKey(pCryptoKey);
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), Variant.Verify(&PubKey)));
	VARIANT_CATCH
}

/** encrypt:
* encrypt using a public key or a symetric key
*
* @param key object
*	private key to be used for the encryption or symetric key
*
* @param string / IV
*	optional IV to be used with a symetric key
*	or algorythm to be used instead of the default one for the given public
*/
void CJSVariant::FxEncrypt(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	VARIANT_TRY
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	CCryptoKeyObj* pCryptoKey;
	UINT eAlgorithm = 0;
	CCryptoKeyObj* pIV = NULL;
	if (!GetCredentials(args, pCryptoKey, &eAlgorithm, &pIV))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	if((pCryptoKey->GetAlgorithm() & CAbstractKey::eAsymCipher) != 0)
	{
		CPublicKey PubKey(pCryptoKey->GetAlgorithm());
		PubKey.SetKey(pCryptoKey);
		Variant.Encrypt(&PubKey, eAlgorithm);
	}
	else
		Variant.Encrypt(pCryptoKey, pIV);

	args.GetReturnValue().SetUndefined();
	VARIANT_CATCH
}

/** decrypt:
* decrypt a variant using a private key a symetric key
*
* @param key object
*	private key to be used for the decryption or symetric key
*
* @param IV
*	optional IV to be used with a symetric key
*
* @return bool
*	decryption result
*/
void CJSVariant::FxDecrypt(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	VARIANT_TRY
	CJSVariant* jVariant = GetJSObject<CJSVariant>(args.Holder());
	CVariant& Variant = jVariant->m_pVariant->GetVariant();
	CCryptoKeyObj* pCryptoKey;
	CCryptoKeyObj* pIV = NULL;
	if (!GetCredentials(args, pCryptoKey, NULL, &pIV))
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	if((pCryptoKey->GetAlgorithm() & CAbstractKey::eAsymCipher) != 0)
	{
		CPrivateKey PrivKey(pCryptoKey->GetAlgorithm());
		PrivKey.SetKey(pCryptoKey);
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), Variant.Decrypt(&PrivKey)));
	}
	else
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), Variant.Decrypt(pCryptoKey, pIV)));

	VARIANT_CATCH
}
