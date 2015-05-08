#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "JSBinaryCache.h"
#include "JSBinaryBlock.h"
#include "../Kademlia.h"
#include "../KadConfig.h"
#include "../../Common/FileIO.h"
#include "../../../Framework/Strings.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "../../Common/v8Engine/JSBuffer.h"
#include "KadDebugging.h"

#include "JSKadScript.h"
#include "KadOperator.h"

IMPLEMENT_OBJECT(CCacheOperation, CObject)

CCacheOperation::CCacheOperation(const wstring& FileName, CObject* pParent)
: CObject(pParent)
{
	m_FileName = FileName;
	m_Operation = eDelete;
	m_Offset = 0;
	m_Result = false;
	m_Callback = 0;
}

CCacheOperation::CCacheOperation(const wstring& FileName, uint64 Offset, uint64 Length, CObject* pParent)
: CObject(pParent)
{
	m_FileName = FileName;
	m_Operation = eRead;
	m_Offset = Offset;
	m_Result = false;
	m_Callback = 0;

	m_Data.AllocBuffer(Length, true);
}

CCacheOperation::CCacheOperation(const wstring& FileName, uint64 Offset, const CBuffer& Data, CObject* pParent)
: CObject(pParent)
{
	m_FileName = FileName;
	m_Operation = eWrite;
	m_Offset = Offset;
	m_Result = false;
	m_Callback = 0;
	m_Data = Data;
}

CCacheOperation::CCacheOperation(const wstring& FileName, const wstring& NewName, CObject* pParent)
{
	m_FileName = FileName;
	m_NewName = NewName;
	m_Operation = eRename;
	m_Offset = 0;
	m_Result = false;
	m_Callback = 0;
}

void CCacheOperation::Execute()
{
	switch(m_Operation)
	{
		case eDelete:
			m_Result = RemoveFile(m_FileName);
			break;
		case eRead:
			m_Result = ReadFile(m_FileName, m_Offset, m_Data);
			break;
		case eWrite:
			m_Result = WriteFile(m_FileName, m_Offset, m_Data);
			break;
		case eRename:
			m_Result = RenameFile(m_FileName, m_NewName);
			break;
		default:
			ASSERT(0);
	}
}

void CCacheOperation::Callback()
{
	ASSERT(m_pScript);

	vector<CPointer<CObject> > Arguments;
	switch(m_Operation)
	{
		case eDelete:
			break;
		case eWrite:
			Arguments.push_back(new CVariantPrx(m_Result));
		case eRead:
			Arguments.push_back(new CBufferObj(m_Data));
			break;
		case eRename:
			break;
		default:
			ASSERT(0);
	}

	CDebugScope Debug(m_DebugScope);

	try
	{
		CPointer<CObject> Return;
		m_pScript->Callback(m_Callback, Arguments, Return);
	}
	catch(const CJSException& Exception)
	{
		CKadScript* pScript = (CKadScript*)CDebugScope::Scope().first;
		CObject* pObject = (CObject*)CDebugScope::Scope().second;
		if(CKadOperation* pOperation = pObject->Cast<CKadOperation>())
			pOperation->LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
		else
			pScript->LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
	}
}

void CCacheOperation::SetCallback(CJSScript* pScript, uint32 Callback)
{
	m_pScript = pScript; 
	m_Callback = Callback;
	m_DebugScope = CDebugScope::Scope();
}

///////////////////////////////////////////////////


IMPLEMENT_OBJECT(CBinaryCache, CObject)

CBinaryCache::CBinaryCache(CObject* pParent)
: CObject(pParent), m_Thread(RunProc, this)
{
	m_uLastCleanup = GetCurTick();
	m_CachePath = GetParent<CKademlia>()->Cfg()->GetString("BinaryCachePath");
		
	vector<wstring> Blocks;
	ListDir(m_CachePath, Blocks);
	for(size_t i=0; i < Blocks.size(); i++)
	{
		pair<wstring, wstring> PathName = Split2(Blocks[i], L"/", true);
		pair<wstring, wstring> NameExt = Split2(PathName.second, L".", true);
		if(CompareStr(NameExt.second, L"tmp"))
			RemoveFile(Blocks[i]);
		else if (CompareStr(NameExt.second, L"bin"))
		{
			CBinaryBlock* pBlock = new CBinaryBlock(PathName.second, this);
			m_Blocks[PathName.second] = pBlock;

			pBlock->LoadData(PathName.first + L"/" + NameExt.first + L".dat");
		}
	}

	m_Active = true;
	m_Thread.Start();
}

CBinaryCache::~CBinaryCache()
{
	m_Active = false;
	m_Thread.Stop();

	Cleanup();
}

void CBinaryCache::Process(UINT Tick)
{
	m_Mutex.Lock();
	while(!m_CallbackQueue.empty())
	{
		CPointer<CCacheOperation> pOperation = m_CallbackQueue.front();
		m_CallbackQueue.pop_front();
		m_Mutex.Unlock();

		pOperation->Callback();

		m_Mutex.Lock();
	}
	m_Mutex.Unlock();
	
	if((Tick & EPer100Sec) != 0 && GetCurTick() - m_uLastCleanup > SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("CacheCleanupInterval")))
	{
		Cleanup();
		m_uLastCleanup = GetCurTick();
	}
}

void CBinaryCache::Cleanup()
{
	for(BlockMap::iterator I = m_Blocks.begin(); I != m_Blocks.end(); )
	{
		CBinaryBlock* pBlock = I->second;
		pair<wstring, wstring> NameExt = Split2(pBlock->GetName(), L".", true);
		if(GetTime() - pBlock->GetLastUsed() > GetParent<CKademlia>()->Cfg()->GetInt("CacheRetentionTime"))
		{
			RemoveFile(m_CachePath + pBlock->GetName());
			
			::RemoveFile(m_CachePath + NameExt.first + L".dat");

			pBlock->Dispose();
			I = m_Blocks.erase(I);
			continue;
		}

		pBlock->SaveData(m_CachePath + NameExt.first + L".dat");
		I++;
	}
}

void CBinaryCache::Run()
{
	while(m_Active)
	{
		m_Mutex.Lock();
		if(m_OperationQueue.empty())
		{
			m_Mutex.Unlock();
			CThread::Sleep(100);
			continue;
		}

		CPointer<CCacheOperation> pOperation = m_OperationQueue.front();
		m_OperationQueue.pop_front();
		m_Mutex.Unlock();

		pOperation->Execute();

		if(pOperation->HasCallback())
		{
			m_Mutex.Lock();
			m_CallbackQueue.push_back(pOperation);
			m_Mutex.Unlock();
		}
	}
}

void CBinaryCache::RemoveFile(const wstring& Path)
{
	m_Mutex.Lock();
	CCacheOperation* pOperation = new CCacheOperation(m_CachePath + Path, this);
	m_OperationQueue.push_back(pOperation);
	m_Mutex.Unlock();
}

void CBinaryCache::ReadFile(const wstring& Path, uint64 Offset, uint64 Length, CJSScript* pScript, uint32 Callback)
{
	m_Mutex.Lock();
	if(Length == -1)
	{
		uint64 uSize = GetFileSize(Path);
		if(uSize > Offset)
			Length = uSize - Offset;
		else
			Length = 0;
	}
	CCacheOperation* pOperation = new CCacheOperation(m_CachePath + Path, Offset, Length, this);
	if(pScript)
		pOperation->SetCallback(pScript, Callback);
	m_OperationQueue.push_back(pOperation);
	m_Mutex.Unlock();
}

void CBinaryCache::WriteFile(const wstring& Path, uint64 Offset, const CBuffer& Data, CJSScript* pScript, uint32 Callback)
{
	m_Mutex.Lock();
	CCacheOperation* pOperation = new CCacheOperation(m_CachePath + Path, Offset, Data, this);
	if(pScript)
		pOperation->SetCallback(pScript, Callback);
	m_OperationQueue.push_back(pOperation);
	m_Mutex.Unlock();
}

uint64 CBinaryCache::GetFileSize(const wstring& Path)
{
	return ::GetFileSize(m_CachePath + Path);
}

bool CBinaryCache::AddBlock(CBinaryBlock* pBlock)
{
	if(pBlock->IsTemporary())
	{
		wstring OldName = pBlock->GetName();
		pBlock->SetPermanent();
		wstring NewName = pBlock->GetName();
		m_Blocks[NewName] = pBlock;

		// Queue Rename
		m_Mutex.Lock();
		CCacheOperation* pOperation = new CCacheOperation(m_CachePath + OldName, m_CachePath + NewName, this);
		m_OperationQueue.push_back(pOperation);
		m_Mutex.Unlock();
	}
	return false;
}

CBinaryBlock* CBinaryCache::GetBlock(const wstring& Name)
{
	BlockMap::iterator I = m_Blocks.find(Name);
	if(I == m_Blocks.end())
		return NULL;
	return I->second;
}

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSBinaryCache::m_Template;

v8::Local<v8::ObjectTemplate> CJSBinaryCache::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"create"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxCreate),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"open"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxOpen),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"save"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxSave),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

void CJSBinaryCache::FxCreate(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSBinaryCache* jCache = GetJSObject<CJSBinaryCache>(args.Holder());
	CBinaryBlock* pBlock = new CBinaryBlock(true, jCache->m_pCache);
	CJSBinaryBlock* jObject = new CJSBinaryBlock(pBlock , jCache->m_pScript);
	args.GetReturnValue().Set(jObject->GetInstance());
}

void CJSBinaryCache::FxOpen(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	CJSBinaryCache* jCache = GetJSObject<CJSBinaryCache>(args.Holder());
	CBinaryBlock* pBlock = jCache->m_pCache->GetBlock(CJSEngine::GetWStr(args[0]));
	if (pBlock)
	{
		CJSBinaryBlock* jObject = new CJSBinaryBlock(pBlock, jCache->m_pScript);
		args.GetReturnValue().Set(jObject->GetInstance());
	}
	else
		args.GetReturnValue().SetNull();
}

void CJSBinaryCache::FxSave(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	if (args.Length() < 1 || !args[0]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	CJSBinaryCache* jCache = GetJSObject<CJSBinaryCache>(args.Holder());
	CBinaryBlock* pBlock = GetCObject<CBinaryBlock>(args[0]->ToObject());
	if (!pBlock)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), jCache->m_pCache->AddBlock(pBlock)));
}

