#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "JSBinaryBlock.h"
#include "JSBinaryCache.h"
#include "../Kademlia.h"
#include "../KadConfig.h"
#include "../../../Framework/Strings.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "../../Common/v8Engine/JSBuffer.h"
#include "../../Common/FileIO.h"

IMPLEMENT_OBJECT(CBinaryBlock, CObject)

CBinaryBlock::CBinaryBlock(bool bTemporary, CObject* pParent)
: CObject(pParent)
{
	CAbstractKey ID(KEY_128BIT, true);
	m_FileName = ToHex(ID.GetKey(), ID.GetSize());
	if(bTemporary)
	{
		m_FileName += L".tmp";
		m_LastUsed = 0;
	}
	else
	{
		m_FileName += L".bin";
		m_LastUsed = GetTime();
	}
}

CBinaryBlock::CBinaryBlock(const wstring& FileName, CObject* pParent)
: CObject(pParent)
{
	m_FileName = FileName;

	// Filter filename
	for(;;)
	{
		wstring::size_type Pos = m_FileName.find_first_of(L"\\/*?:<>|\"");
		if(Pos == wstring::npos)
			break;
		m_FileName.replace(Pos,1,L"_");
	}

	m_LastUsed = GetTime();
}

CBinaryBlock::~CBinaryBlock()
{
	if(m_LastUsed == 0)
		GetParent<CBinaryCache>()->RemoveFile(m_FileName);
}

void CBinaryBlock::SaveData(const wstring& Path)
{
	CVariant Data;

	Data["LUSA"] = m_LastUsed;

	WriteFile(Path, Data);
}

bool CBinaryBlock::LoadData(const wstring& Path)
{
	CVariant Data;
	if(!ReadFile(Path, Data))
		return false;

	m_LastUsed = Data["LUSA"].To<uint64>();

	return true;
}

void CBinaryBlock::Remove()
{
	if(m_FileName.empty())
		return;
	GetParent<CBinaryCache>()->RemoveFile(m_FileName);
}

bool CBinaryBlock::Read(uint64 Offset, uint64 Length, CJSScript* pScript, uint32 Callback)
{
	if(m_FileName.empty())
		return false;

	if(m_LastUsed)
		m_LastUsed = GetTime();
	GetParent<CBinaryCache>()->ReadFile(m_FileName, Offset, Length, pScript, Callback);
	return true;
}

bool CBinaryBlock::Write(uint64 Offset, const CBuffer& Data, CJSScript* pScript, uint32 Callback)
{
	if(m_FileName.empty())
		return false;

	if(m_LastUsed)
		m_LastUsed = GetTime();
	GetParent<CBinaryCache>()->WriteFile(m_FileName, Offset, Data, pScript, Callback);
	return true;
}

uint64 CBinaryBlock::GetSize()
{
	if(m_FileName.empty())
		return 0;

	if(m_LastUsed)
		m_LastUsed = GetTime();
	return GetParent<CBinaryCache>()->GetFileSize(m_FileName);
}

void CBinaryBlock::SetPermanent()
{
	m_LastUsed = GetTime();
	pair<wstring, wstring> NameExt = Split2(m_FileName, L".", true);
	m_FileName = NameExt.first + L".bin";
}

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSBinaryBlock::m_Template;

v8::Local<v8::ObjectTemplate> CJSBinaryBlock::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"remove"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxRemove),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"read"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxRead),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"write"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxWrite),v8::ReadOnly);
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"size"), GetSize, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"name"), GetName, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	/*v8::Local<v8::ObjectTemplate> ConstructorTemplate = v8::ObjectTemplate::New();
	ConstructorTemplate->SetInternalFieldCount(2);

	ConstructorTemplate->SetCallAsFunctionHandler(CJSBinaryBlock::JSBinaryBlock);

	return HandleScope.Escape(ConstructorTemplate);*/
	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

/*v8::Local<v8::Value> CJSBinaryBlock::JSBinaryBlock(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBinaryBlock* jObject = new CJSBinaryBlock(new CBinaryBlock());
	return HandleScope.Close(jObject->GetInstance());
}*/

void CJSBinaryBlock::FxRemove(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CBinaryBlock* pBlock = GetCObject<CBinaryBlock>(args.Holder());
	pBlock->Remove();
	args.GetReturnValue().SetUndefined();
}

void CJSBinaryBlock::FxRead(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBinaryBlock* jBlock = GetJSObject<CJSBinaryBlock>(args.Holder());
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	if(args.Length() >= 3)
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), jBlock->m_pBlock->Read(args[0]->NumberValue(), args[1]->NumberValue(), jBlock->m_pScript, jBlock->m_pScript->SetCallback(args[2]))));
	else
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), jBlock->m_pBlock->Read(args[0]->NumberValue(), args[1]->NumberValue())));
}

void CJSBinaryBlock::FxWrite(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBinaryBlock* jBlock = GetJSObject<CJSBinaryBlock>(args.Holder());
	if (args.Length() < 2 || !args[1]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	
	CBufferObj* pData = GetCObject<CBufferObj>(args[1]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}

	if(args.Length() >= 3)
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), jBlock->m_pBlock->Write(args[0]->NumberValue(), *pData, jBlock->m_pScript, jBlock->m_pScript->SetCallback(args[2]))));
	else
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), jBlock->m_pBlock->Write(args[0]->NumberValue(), *pData)));
}

void CJSBinaryBlock::GetSize(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CBinaryBlock* pBlock = GetCObject<CBinaryBlock>(info.Holder());
	info.GetReturnValue().Set(v8::Number::New(v8::Isolate::GetCurrent(), pBlock->GetSize()));
}

void CJSBinaryBlock::GetName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CBinaryBlock* pBlock = GetCObject<CBinaryBlock>(info.Holder());
	info.GetReturnValue().Set(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(pBlock->GetName().c_str())));
}
