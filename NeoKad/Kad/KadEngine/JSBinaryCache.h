#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../../../v8/include/v8-platform.h"
#include "../../../Framework/Buffer.h"
#include "../../Common/MT/Thread.h"
#include "../../Common/MT/Mutex.h"

class CBinaryBlock;

class CCacheOperation: public CObject
{
public:
	DECLARE_OBJECT(CCacheOperation);

	enum EOperation { eNone, eDelete, eRead, eWrite, eRename };

	CCacheOperation(const wstring& FileName, CObject* pParent = NULL);
	CCacheOperation(const wstring& FileName, uint64 Offset, uint64 Length, CObject* pParent = NULL);
	CCacheOperation(const wstring& FileName, uint64 Offset, const CBuffer& Data, CObject* pParent = NULL);
	CCacheOperation(const wstring& FileName, const wstring& NewName, CObject* pParent = NULL);

	void				Execute();
	void				Callback();

	void				SetCallback(CJSScript* pScript, uint32 Callback);
	bool				HasCallback()												{return m_pScript;}

protected:
	wstring				m_FileName;
	wstring				m_NewName;
	EOperation			m_Operation;
	uint64				m_Offset;
	CBuffer				m_Data;
	
	bool				m_Result;
	
	CPointer<CJSScript>	m_pScript;
	uint32				m_Callback;
	pair<void*, void*>	m_DebugScope;
};

class CBinaryCache: public CObject
{
public:
	DECLARE_OBJECT(CBinaryCache);

	CBinaryCache(CObject* pParent = NULL);
	virtual ~CBinaryCache();

	void				Process(UINT Tick);

	static void RunProc(const void* param) 
	{
		((CBinaryCache*)param)->Run();
	}

	void				Run();

	void				RemoveFile(const wstring& Path);
	void				ReadFile(const wstring& Path, uint64 Offset, uint64 Length, CJSScript* pScript = NULL, uint32 Callback = 0);
	void				WriteFile(const wstring& Path, uint64 Offset, const CBuffer& Data, CJSScript* pScript = NULL, uint32 Callback = 0);
	uint64				GetFileSize(const wstring& Path);

	bool				AddBlock(CBinaryBlock* pBlock);
	CBinaryBlock*		GetBlock(const wstring& Name);

protected:
	void				Cleanup();

	CThread				m_Thread;
	CMutex				m_Mutex;
	volatile bool		m_Active;

	uint64				m_uLastCleanup;
	wstring				m_CachePath;

	list<CPointer<CCacheOperation> >	m_OperationQueue;
	list<CPointer<CCacheOperation> >	m_CallbackQueue;

	typedef map<wstring, CPointer<CBinaryBlock> > BlockMap;
	BlockMap			m_Blocks;
};

class CJSBinaryCache: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSBinaryCache(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate() {return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pCache;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CBinaryCache	Type;

protected:
	CJSBinaryCache(CBinaryCache* pCache, CJSScript* pScript) : m_pCache(pCache, true) {Instantiate(pScript);}

	static void	FxCreate(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxOpen(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxSave(const v8::FunctionCallbackInfo<v8::Value> &args);
	// K-ToDo-Now: add some additional function for cache index metrics (load, etc)

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>		m_pCache;
};
