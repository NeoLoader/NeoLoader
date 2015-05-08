#include "GlobalHeader.h"
#include "JSEngine.h"
#include "JSTerminator.h"
#include "../v8/include/libplatform/libplatform.h"


//map<string, set<void*> > g_CountMap;

CJSObject::~CJSObject()
{
	m_Instance.Reset();
}

void CJSObject::Instantiate(CJSScript* pScript, bool bWeak)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::Object> Instance = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), GetTemplate())->NewInstance();
	if(Instance.IsEmpty())
		return; // execution terminated
	Instance->SetInternalField(0, v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)GetObjectName()));
	Instance->SetInternalField(1, v8::External::New(v8::Isolate::GetCurrent(), this));

	m_Instance.Reset(v8::Isolate::GetCurrent(), Instance);

	if (bWeak)
		m_Instance.SetWeak<CJSObject>(this, CleanUpCallback);

	m_pScript = CPointer<CJSScript>(pScript, true);

	//qDebug() << "make" << GetObjectName() << " (" << (uint64)GetObjectPtr() << ")";
	//g_CountMap[GetObjectName()].insert(GetObjectPtr());
}

void CJSObject::MakeWeak()
{
	if(!m_Instance.IsWeak())
		m_Instance.SetWeak<CJSObject>(this, CleanUpCallback);
}

void CJSObject::CleanUpCallback(const v8::WeakCallbackData<v8::Object, CJSObject>& data)
{
	CJSObject* jObject = data.GetParameter();
	CObject* pObject = NULL;
	bool bFound = false;
	if(jObject->m_pScript)
	{
		// Note: jObject->m_pScript->m_Objects.find(jObject->GetObjectPtr() fails if the object was hold in a weak pointer, so we have to always inspect the entire map
		for(map<CObject*, CJSObject*>::iterator I = jObject->m_pScript->m_Objects.begin(); I != jObject->m_pScript->m_Objects.end();)
		{
			if(I->second == jObject)
			{
				ASSERT(!bFound);
				bFound = true;
				pObject = I->first;
				I = jObject->m_pScript->m_Objects.erase(I);
			}
			else
				I++;
		}
	}
	if(!pObject)
		pObject = jObject->GetObjectPtr();
	//qDebug() << "break" << jObject->GetObjectName() << " (" << (uint64)pObject << ")";
	//g_CountMap[jObject->GetObjectName()].erase(pObject);
	delete jObject;
}

void CJSObject::FxToString(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	if (CObject* pObject = GetCObject<CObject>(args.Holder()))
	{
		args.GetReturnValue().Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)pObject->ClassName()));
		return;
	}
	args.GetReturnValue().SetNull();
}

void MemoryAllocationCallback(v8::ObjectSpace space, v8::AllocationAction action, int size)
{
	switch(action)
	{
	case v8::kAllocationActionAllocate:
		CJSEngine::m_TotalMemory += (size_t)size;
		break;
	case v8::kAllocationActionFree:
		if(CJSEngine::m_TotalMemory >= (size_t)size)
			CJSEngine::m_TotalMemory -= (size_t)size;
		else
			CJSEngine::m_TotalMemory = 0;
		break;
	}

	if(CJSEngine::m_TotalMemory > MB2B(896)) // Note: V8 is by default limited to 1GB of memory
	{
		// Issue execution Termination
		ASSERT(0);
		v8::V8::TerminateExecution(v8::Isolate::GetCurrent());//(m_Isolate);
	}
}

IMPLEMENT_OBJECT(CJSEngine, CObject)

map<string, CJSEngine::TNew> CJSEngine::m_NewMap;
CJSEngine::SConstrMap CJSEngine::m_ConstrMap;
size_t CJSEngine::m_TotalMemory = 0;

v8::Platform* g_Platform = NULL;
v8::Isolate* g_Isolate = NULL;
v8::Isolate::Scope* g_IsolateScope = NULL;

CJSTerminatorThread* g_JSTerminator = NULL;

void CJSEngine::Init()
{
	// Initialize V8.
	v8::V8::InitializeICU();
	g_Platform = v8::platform::CreateDefaultPlatform();
	v8::V8::InitializePlatform(g_Platform);
	v8::V8::Initialize();

	//v8::Locker::StartPreemption(10); // preemption is needed for the terminator as well as for the debugger

	g_Isolate = v8::Isolate::New();

	g_IsolateScope = new v8::Isolate::Scope(g_Isolate);

	g_JSTerminator = new CJSTerminatorThread(g_Isolate);

	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::V8::AddMemoryAllocationCallback(MemoryAllocationCallback, v8::kObjectSpaceAll, v8::kAllocationActionAll);
}

void CJSEngine::Dispose()
{
	delete g_IsolateScope;

	g_Isolate->Dispose();

	v8::V8::Dispose();
	v8::V8::ShutdownPlatform();
	delete g_Platform;
}

void FxAlert(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	wstring Prompt = args.Length() >= 1 ? CJSEngine::GetWStr(args[0]) : L"";
	qDebug() << QString::fromStdWString(Prompt);
	args.GetReturnValue().SetUndefined();
};

CJSObject* CJSEngine::NewObject(CObject* pObject, CJSScript* pScript)
{
	list<string> ClassNames = pObject->ClassNames();
	for(list<string>::iterator I = ClassNames.begin(); I != ClassNames.end(); I++)
	{
		map<string, TNew>::iterator J = m_NewMap.find(*I);
		if(J != m_NewMap.end())
			return J->second(pObject, pScript);
	}
	return NULL;
}

void CJSEngine::SetupNew(v8::Local<v8::Object> Global, CJSScript* pScript)
{
	for (map<string, SConstrHolder*>::iterator I = m_ConstrMap.Map.begin(); I != m_ConstrMap.Map.end(); I++)
	{
		v8::Local<v8::Object> Instance = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), I->second->Template)->NewInstance();
		Instance->SetInternalField(0, v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)pScript->ClassName()));
		Instance->SetInternalField(1, v8::External::New(v8::Isolate::GetCurrent(), pScript));
		Global->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)I->first.c_str()), Instance);
	}

	/*v8::Local<v8::Object> Instance = v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxAlert)->toin;
	Global->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"alert"), Instance);*/
}
