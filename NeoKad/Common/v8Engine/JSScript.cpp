#include "GlobalHeader.h"
#include "JSScript.h"
#include "JSEngine.h"
#include "JSVariant.h"
#include "JSTerminator.h"

IMPLEMENT_OBJECT(CJSScript, CObject)

CJSScript::CJSScript(CObject* pParent)
 : CObject(pParent)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	
	v8::Local<v8::Context> Context = v8::Context::New(v8::Isolate::GetCurrent());
	m_Context.Reset(v8::Isolate::GetCurrent(), Context);

	v8::Context::Scope ContextScope(Context);

	CJSEngine::SetupNew(Context->Global(), this);
}

CJSScript::~CJSScript()
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	// J-ToDo-Now: fix this why doesnt GC works with m_Context.Dispose(); only ???
	// we have to remove what we have added with Global()->Set(), in order for the GC to kick in
	v8::Local<v8::Array> Properties = Context->Global()->GetPropertyNames();
	for(uint32_t i=0; i < Properties->Length(); i++)
		Context->Global()->Delete(Properties->Get(i)->ToString());

	m_Context.Reset();
}

void CJSScript::Initialize(const wstring& Source, const wstring& Name, const TObjMap& Objects)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	for(TObjMap::const_iterator I = Objects.begin(); I != Objects.end(); I++)
		Context->Global()->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)I->first.c_str()), GetObject(I->second));

	CJSTerminator JSTerminator(g_JSTerminator);

	v8::Local<v8::String> ScriptSource = v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Source.c_str()));
	v8::ScriptOrigin Origin = v8::ScriptOrigin(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Name.c_str())));

	v8::Local<v8::Script> Script = v8::Script::Compile(ScriptSource, &Origin);
	if (Script.IsEmpty()) 
		throw CJSException("CompileFailed", L"JS Exception: %s", JSTerminator.GetException().c_str());

    v8::Local<v8::Value> Result = Script->Run();

    if(JSTerminator.HasException())
        throw CJSException("UncaughtException", L"JS Exception: %s", JSTerminator.GetException().c_str());
	if(Result.IsEmpty()) // Note: of the debugger caucht the exception we still want to return false
		throw CJSException("UncaughtException", L"JS Exception caught by debugger");
}

CJSObject* CJSScript::SetObject(CObject* pObject)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	if(CJSObject* jObject = CJSEngine::NewObject(pObject, this))
	{
		m_Objects.insert(map<CObject*, CJSObject*>::value_type(pObject, jObject));
		return jObject;
	}
	return NULL;
}

CJSObject* CJSScript::GetJSObject(CObject* pObject)
{
	map<CObject*, CJSObject*>::iterator I = m_Objects.find(pObject);
	if(I != m_Objects.end())
		return I->second;
	return NULL;
}

v8::Local<v8::Value> CJSScript::GetObject(CObject* pObject, bool bCanAdd)
{
	// Note: this function mut be only called when a lock already habeen issued
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	map<CObject*, CJSObject*>::iterator I = m_Objects.find(pObject);
	if(I != m_Objects.end())
	{
		if (I->second->GetObjectPtr() == pObject) // object may have been deleted and we have a new with the sam address :/
		{
			v8::Local<v8::Object> Value = v8::Local<v8::Object>::New(v8::Isolate::GetCurrent(), I->second->GetInstance());
			return HandleScope.Escape(Value);
		}
		ASSERT(I->second->GetObjectPtr() == NULL); // if it really was deleted the smart poitner must be NULL;
		m_Objects.erase(I); // delete will be done by CJSObject::CleanUpCallback
	}

	if(!bCanAdd)
		return v8::Null(v8::Isolate::GetCurrent());
	
	if(CJSObject* jObject = CJSEngine::NewObject(pObject, this))
	{
		m_Objects.insert(map<CObject*, CJSObject*>::value_type(pObject, jObject));
		v8::Local<v8::Object> Value = v8::Local<v8::Object>::New(v8::Isolate::GetCurrent(), jObject->GetInstance());
		return HandleScope.Escape(Value);
	}
	
	return v8::Null(v8::Isolate::GetCurrent());
}

bool CJSScript::Has(const string& jObject, const string& Name)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	v8::Local<v8::Object> Object;
	if(jObject.empty())
		Object = Context->Global();
	else
	{
		v8::Local<v8::Value> Value = Context->Global()->Get(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)jObject.c_str()));
		if(!Value->IsObject())
			return false;
		Object = Value->ToObject();
	}

	v8::Local<v8::Value> Value = Object->Get(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)Name.c_str()));
	return Value->IsFunction();
}

void CJSScript::Call(const string& jObject, const string& Name, const vector<CPointer<CObject> >& Arguments, CPointer<CObject>& Return)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	v8::Local<v8::Object> Object;
	if(jObject.empty())
		Object = Context->Global();
	else
	{
		v8::Local<v8::Value> Value = Context->Global()->Get(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)jObject.c_str()));
		if(!Value->IsObject())
			throw CJSException("InvalidCallObject", L"Object %S for execution is not valid", jObject.c_str());
		Object = Value->ToObject();
	}

	Call(Object, Name, Arguments, Return);
}

bool CJSScript::Has(CObject* pObject, const string& Name)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	v8::Local<v8::Value> Object = GetObject(pObject, false);
	if(!Object->IsObject())
		return false;
	
	v8::Local<v8::Value> Value = Object->ToObject()->Get(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)Name.c_str()));
	return Value->IsFunction();
}

void CJSScript::Call(CObject* pObject, const string& Name, const vector<CPointer<CObject> >& Arguments, CPointer<CObject>& Return)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	v8::Local<v8::Value> Object = GetObject(pObject, false);
	if(!Object->IsObject())
		throw CJSException("InvalidCallObject", L"Object for execution is not valid");

	Call(Object->ToObject(), Name, Arguments, Return);
}

void CJSScript::Call(v8::Local<v8::Object> Object, const string& Name, const vector<CPointer<CObject> >& Arguments,CPointer<CObject>& Return)
{
	v8::Local<v8::Value> Value = Object->Get(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)Name.c_str()));
	if(Value->IsNull() || Value->IsUndefined())
		throw CJSException("MissingFunction", L"Funktion %S for execution is not present", Name.c_str());
		
	if(!Value->IsFunction())
		throw CJSException("InvalidFunction", L"Funktion %S for execution is not valid", Name.c_str());

	v8::Local<v8::Function> Function = v8::Local<v8::Function>::Cast(Value);
	ASSERT(Function->IsFunction());

	CJSTerminator JSTerminator(g_JSTerminator);

	vector<v8::Local<v8::Value> > Args(Arguments.size());
	for(size_t i=0; i < Arguments.size(); i++)
	{
		CObject* Arg = Arguments[i];
		if(!Arg)
			Args[i] = v8::Null(v8::Isolate::GetCurrent());
		else if(CVariantPrx* pVariant = Arg->Cast<CVariantPrx>())
			Args[i] = CJSVariant::ToValue(pVariant->GetCopy(), this); // copy the variant
		else
			Args[i] = GetObject(Arguments[i]);
	}
	
	v8::Local<v8::Value> Result = Function->Call(Object, (int)Args.size(), Args.empty() ? NULL : &Args[0]);

	if(JSTerminator.HasException()) 
		throw CJSException("UncaughtException", L"JS Exception: %s", JSTerminator.GetException().c_str());
	if(Result.IsEmpty()) // Note: of the debugger caucht the exception we still want to return false
		throw CJSException("UncaughtException", L"JS Exception cought by attached debugger");

	if(Result->IsObject())
		Return = CJSObject::GetCObject<CObject>(Result->ToObject());
	else
		Return = new CVariantPrx(CJSVariant::FromValue(Result));
}

__inline wstring GetTempName(uint32 Int, const wstring& Base)
{
	wstringstream Val;
	Val << L"__" << Base << L"_";
	Val << Int;
	Val << L"__";
	return Val.str();
}

uint32 CJSScript::SetCallback(v8::Local<v8::Value> Action, CObject* pObject)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	v8::Local<v8::Object> Object;
	if(pObject)
	{
		Object = GetObject(pObject, false)->ToObject();
		if(Object.IsEmpty())
			return 0;
	}
	else
		Object = Context->Global();

	uint32 ID;
	do	ID = GetRand64() & MAX_FLOAT;
	while(!ID || Object->Has(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(GetTempName(ID, L"callback").c_str()))));

	Object->Set(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(GetTempName(ID, L"callback").c_str())), Action);

	return ID;
}

void CJSScript::ClearCallback(uint32 ID, CObject* pObject)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	v8::Local<v8::Object> Object;
	if(pObject)
	{
		Object = GetObject(pObject, false)->ToObject();
		if(Object.IsEmpty())
			return;
	}
	else
		Object = Context->Global();

	Object->Delete(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(GetTempName(ID, L"callback").c_str())));
}

void CJSScript::Callback(uint32 ID, const vector<CPointer<CObject> >& Arguments, CPointer<CObject>& Return, bool bClear, CObject* pObject)
{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_Context);
	v8::Context::Scope ContextScope(Context);

	v8::Local<v8::Object> Object;
	if(pObject)
	{
		Object = GetObject(pObject, false)->ToObject();
		if(Object.IsEmpty())
			throw CJSException("InvalidCallbackObject", L"Object for callback is not valid");
	}
	else
		Object = Context->Global();

	if(!Object->Has(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(GetTempName(ID, L"callback").c_str()))))
		throw CJSException("InvalidCallback", L"Object for callback is missing callback member");
	
	vector<v8::Local<v8::Value> > Args(Arguments.size());
	for(size_t i=0; i < Arguments.size(); i++)
	{
		CObject* Arg = Arguments[i];
		if(!Arg)
			Args[i] = v8::Null(v8::Isolate::GetCurrent());
		else if(CVariantPrx* pVariant = Arg->Cast<CVariantPrx>())
			Args[i] = CJSVariant::ToValue(pVariant->GetCopy(), this); // copy the variant 
		else
			Args[i] = GetObject(Arguments[i]);
	}

	v8::Local<v8::Value> Result;
	try
	{
		Call(GetTempName(ID, L"callback"), Args, Result, Object);

		if(bClear)
			Object->Delete(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(GetTempName(ID, L"callback").c_str())));
	}
	catch(const CJSException& Exception)
	{
		// if a callback caused an error clear it to avoind further errors
		Object->Delete(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(GetTempName(ID, L"callback").c_str())));
		throw;
	}

	if(!Result.IsEmpty())
	{
		if(Result->IsObject())
			Return = CJSObject::GetCObject<CObject>(Result->ToObject());
		else
			Return = new CVariantPrx(CJSVariant::FromValue(Result));
	}
}

void CJSScript::Call(const wstring& Name, const vector<v8::Local<v8::Value> >& Args, v8::Local<v8::Value>& Return, v8::Local<v8::Object> Object)
{	
	/* WARNING */ // this function must be called anly when context and lock are obtained

	v8::Local<v8::String> FxName = v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Name.c_str()));
	v8::Local<v8::Value> Action = Object->Get(FxName);
	if(Action->IsString()) // if the action isa string, run it as java script
	{
		v8::Local<v8::String> ScriptSource = Action->ToString();

		CJSTerminator JSTerminator(g_JSTerminator);

		v8::Local<v8::Script> Script = v8::Script::Compile(ScriptSource);
		if(Script.IsEmpty())
			throw CJSException("InvalidScript", L"JS Exception: %s", JSTerminator.GetException().c_str());

		v8::Local<v8::Value> Function = Script->Run();

		if(JSTerminator.HasException())
			throw CJSException("UncaughtException", L"JS Exception: %s", JSTerminator.GetException().c_str());
		if(Function.IsEmpty()) // Note: of the debugger caucht the exception we still want to return false
			throw CJSException("UncaughtException", L"JS Exception cought by attached debugger");

		// if it made a function replace it with that function
		if(Function->IsFunction())
		{
			Object->Set(FxName, Function);
			Action = Function;
		}
	}

	// run the function if it is one
	if(Action->IsFunction())
	{
		v8::Local<v8::Function> Function = v8::Local<v8::Function>::Cast(Action);
		ASSERT(!Function.IsEmpty());

		CJSTerminator JSTerminator(g_JSTerminator);

		v8::Local<v8::Value> Result = Function->Call(Object, (int)Args.size(), Args.empty() ? NULL : (v8::Local<v8::Value>*)&Args[0]);

		if(JSTerminator.HasException())
			throw CJSException("InvalidScript", L"JS Exception: %s", JSTerminator.GetException().c_str());
		if(Function.IsEmpty()) // Note: of the debugger caucht the exception we still want to return false
			throw CJSException("InvalidScript", L"JS Exception cought by attached debugger");

		Return = Result;
	}
	else
		Return = Action;
}

///////////////////////////////////////
//

CJSTimer::CJSTimer()
{
}

void CJSTimer::RunTimers(CJSScript* pScript)
{
	uint64 CurTick = GetCurTick();
	uint32 ID = 0;
	for(map<uint32, STimer>::iterator I = m_Timers.begin(); I != m_Timers.end(); I = m_Timers.upper_bound(ID))
	{
		ID = I->first;
		STimer& Timer = I->second;
		if(Timer.NextCall > CurTick)
			continue;

		v8::Locker Lock(v8::Isolate::GetCurrent());
		v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
		v8::Local<v8::Context> Context = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), pScript->m_Context);
		v8::Context::Scope ContextScope(Context);

		vector<v8::Local<v8::Value> >  Args;
		v8::Local<v8::Value> Return;
		try
		{
			pScript->Call(GetTempName(ID, L"timer"), Args, Return, ThisObject());
			if(Timer.Interval == 0) // timer is not periodic, clear it
				ClearTimer(ID);
			else
				Timer.NextCall += Timer.Interval; // reschedule the next run
		}
		catch(const CException& Exception)
		{
			// clear rimer to prevent further errors
			ClearTimer(ID);
			throw; // all remaining timers wil skip a round and be executed on the next tick
		}
	}
}

uint32 CJSTimer::SetTimer(v8::Local<v8::Value> Action, uint32 Interval, bool Periodic)
{
	// WARNING  // this function must be called anly when context and lock are obtained

	uint32 ID;
	do	ID = GetRand64() & MAX_FLOAT;
	while(!ID || m_Timers.find(ID) != m_Timers.end());

	m_Timers[ID].NextCall = GetCurTick() + Interval;
	m_Timers[ID].Interval = Periodic ? Interval : 0;

	ThisObject()->Set(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(GetTempName(ID, L"timer").c_str())), Action);

	return ID; 
}

void CJSTimer::ClearTimer(uint32 ID)
{
	// WARNING // this function must be called anly when context and lock are obtained

	map<uint32, STimer>::iterator I = m_Timers.find(ID);
	if(I != m_Timers.end())
	{
		ThisObject()->Delete(v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(GetTempName(ID, L"timer").c_str())));
		m_Timers.erase(I);
	}
}
