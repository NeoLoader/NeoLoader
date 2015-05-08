#pragma once

#include "../v8/include/v8.h"
#include "../../Common/Object.h"
#include "../../Common/Pointer.h"
#include "JSScript.h"

class CJSObject
{
public:
	virtual ~CJSObject();

	virtual v8::Persistent<v8::Object>&				GetInstance()	{return m_Instance;}
	virtual v8::Persistent<v8::ObjectTemplate>&		GetTemplate() = 0;
	virtual CObject*								GetObjectPtr() = 0;
	virtual char*									GetObjectName() = 0;
	template <typename T>
	static	T*										GetJSObject(const v8::Local<v8::Object>& object)
	{
		if(!object.IsEmpty() && object->InternalFieldCount() >= 2)
		{
			v8::Local<v8::Value> type = object->GetInternalField(0);
			v8::String::Utf8Value Value(type->ToString());
			if(*Value && strcmp(*Value, T::Type::StaticName()) == 0)
			{
				v8::Local<v8::Value> data = object->GetInternalField(1);
				if (!data.IsEmpty() && data->IsExternal())
				{
					v8::Local<v8::External> field = v8::Local<v8::External>::Cast(data);
					return (static_cast<T*>(field->Value()));
				}
			}
		}
		return NULL;
	}

	template <typename T>
	static T* GetCObject(const v8::Local<v8::Object>& object)
	{
		if(!object.IsEmpty() && object->InternalFieldCount() >= 2)
		{
			v8::Local<v8::Value> data = object->GetInternalField(1);
			if (!data.IsEmpty() && data->IsExternal())
			{
				v8::Local<v8::External> field = v8::Local<v8::External>::Cast(data);
				return (static_cast<CJSObject*>(field->Value()))->GetObjectPtr()->Cast<T>();
			}
		}
		return NULL;
	}

	static CJSScript*								GetScript(const v8::Local<v8::Object>& object)
	{
		if(!object.IsEmpty() && object->InternalFieldCount() >= 2)
		{
			v8::Local<v8::Value> data = object->GetInternalField(1);
			if(!data.IsEmpty() && data->IsExternal())
			{ 
				v8::Local<v8::External> field = v8::Local<v8::External>::Cast(data);
				return (static_cast<CJSObject*>(field->Value()))->m_pScript;
			}

		}
		return NULL;
	}

	virtual void MakeWeak();


protected:
	friend class CJSEngine;

	virtual void Instantiate(CJSScript* pScript, bool bWeak = true);
	static void CleanUpCallback(const v8::WeakCallbackData<v8::Object, CJSObject>& data);

	static void FxToString(const v8::FunctionCallbackInfo<v8::Value> &args);

	v8::Persistent<v8::Object>				m_Instance;
	CPointer<CJSScript>						m_pScript;
};

#ifndef WIN32
struct SWStr16
{
	SWStr16(const wchar_t* pWStr)
	{
		size_t length = wcslen(pWStr) + 1;
		int16buff = new uint16_t[length];
		for(int i=0; i<length; ++i)
			int16buff[i] = pWStr[i];
	}
	~SWStr16(){
		delete int16buff;
	}
	uint16_t* int16buff;
};
#define V8STR(x) (SWStr16(x).int16buff)
#else
#define V8STR(x) ((const uint16_t*)(x))
#endif


class CJSEngine: public CObject
{
public:
	DECLARE_OBJECT(CJSEngine);

	CJSEngine(CObject* pParent = NULL) : CObject(pParent) {}

	static void				Init();
	static void				Dispose();

	struct SConstrHolder
	{
		v8::Persistent<v8::ObjectTemplate>		Template;
	};

	template<class T>
	static void				RegisterObj(const char* AltName = NULL)
	{
		v8::Locker Lock(v8::Isolate::GetCurrent());
		v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
		v8::Local<v8::ObjectTemplate> ConstructorTemplate = T::Prepare();
		if (!ConstructorTemplate.IsEmpty())
		{
			v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), ConstructorTemplate);
			SConstrHolder* &pHolder = m_ConstrMap.Map[AltName ? AltName : string(T::Type::StaticName())];
			if (pHolder == NULL)
				pHolder = new SConstrHolder;
			pHolder->Template.Reset(v8::Isolate::GetCurrent(), Template);
		}
		m_NewMap.insert(map<string, TNew>::value_type(T::Type::StaticName(), T::New));	
	}

	static wstring GetWStr(v8::Local<v8::Value> Val)
	{
		if(Val.IsEmpty())
			return  L"(null string)";
#ifndef WIN32
		wstring WStr;
		v8::String::Value int16value(Val->ToString());
		WStr.reserve(int16value.length());
		if(uint16_t* int16buff = *int16value)
		{
			for(int i=0; i<int16value.length(); ++i)
				WStr += int16buff[i];
		}
		return WStr;
#else
		v8::String::Value Str(Val->ToString());
		return *Str ? (const wchar_t*)*Str : L"(null string)";
#endif
	}

	static string GetStr(v8::Local<v8::Value> Val)
	{
		if(Val.IsEmpty())
			return  "(null string)";
		v8::String::Utf8Value Str(Val->ToString());
		return *Str ? *Str : "(null string)";
	}

	static size_t			GetTotalMemory()	{return m_TotalMemory;}

	typedef CJSObject* (*TNew)(CObject*, CJSScript*);
protected:
	friend class CJSScript;
	friend class CJSObject;
	static CJSObject*		NewObject(CObject* pObject, CJSScript* pScript);
	static void				SetupNew(v8::Local<v8::Object> Global, CJSScript* pScript);

	static map<string, TNew>				m_NewMap;
	struct SConstrMap
	{
		SConstrMap() {}
		~SConstrMap() {
			for (map<string, SConstrHolder*>::iterator I = Map.begin(); I != Map.end(); I++)
				delete I->second;
		}
		map<string, SConstrHolder*> Map;
	};
	static SConstrMap m_ConstrMap;

	friend void MemoryAllocationCallback(v8::ObjectSpace space, v8::AllocationAction action, int size);
	static size_t							m_TotalMemory;
};

class CJSTerminatorThread;
extern CJSTerminatorThread* g_JSTerminator;

extern v8::Isolate* g_Isolate;

void FxAlert(const v8::FunctionCallbackInfo<v8::Value> &args);
