#pragma once

#include "../v8/include/v8.h"
#include "../../Common/Object.h"
#include "../../Common/Pointer.h"
#include "../../../Framework/Exception.h"

class CJSException: public CException
{
public:
	CJSException(const char* Error, const wchar_t *sLine, ...)
	 : CException (LOG_ERROR)
	{
		m_Error = Error;
	
		va_list argptr; va_start(argptr, sLine); StrFormat(sLine, argptr); va_end(argptr);
	}

	const string&		GetError() const {return m_Error;}

protected:
	string	m_Error;
};


class CJSEngine;
class CJSObject;

typedef map<string, CObject*> TObjMap;

class CJSScript: public CObject
{
public:
	DECLARE_OBJECT(CJSScript);

	CJSScript(CObject* pParent = NULL);
	virtual ~CJSScript();

	virtual void			Initialize(const wstring& Source, const wstring& Name = L"", const TObjMap& Objects = TObjMap());

	virtual bool			Has(const string& jObject, const string& Name);
	virtual void			Call(const string& jObject, const string& Name, const vector<CPointer<CObject> >& Arguments, CPointer<CObject>& Return);		// execute on a named object in the global scope
	virtual bool			Has(CObject* pObject, const string& Name);
	virtual void			Call(CObject* pObject, const string& Name, const vector<CPointer<CObject> >& Arguments, CPointer<CObject>& Return);			// execute on a known c object in an arbitrary scope

	virtual uint32			SetCallback(v8::Local<v8::Value> Action, CObject* pObject = NULL);
	virtual void			ClearCallback(uint32 ID, CObject* pObject = NULL);
	virtual void			Callback(uint32 ID, const vector<CPointer<CObject> >& Arguments, CPointer<CObject>& Return, bool bClear = true, CObject* pObject = NULL);

	virtual CJSObject*		SetObject(CObject* pObject);
	virtual CJSObject*		GetJSObject(CObject* pObject);
	v8::Local<v8::Value>	GetObject(CObject* pObject, bool bCanAdd = true);

	virtual CObject*		GetObjectPtr()					{return this;}
	virtual char*			GetObjectName()					{return StaticName();}
	typedef CJSScript Type;

	v8::Persistent<v8::Context>& GetContext()				{return m_Context;}

protected:
	friend class CJSObject;
	friend class CJSTimer;

	virtual void			Call(const wstring& Name, const vector<v8::Local<v8::Value> >& Args, v8::Local<v8::Value>& Return, v8::Local<v8::Object> Object);
	virtual void			Call(v8::Local<v8::Object> jObject, const string& Name, const vector<CPointer<CObject> >& Arguments, CPointer<CObject>& Return);

	v8::Persistent<v8::Context>	m_Context;
	map<CObject*, CJSObject*>	m_Objects;
};

///////////////////////////////////////
//

class CJSTimer
{
public:
	CJSTimer();

	virtual void			RunTimers(CJSScript* pScript);
	virtual uint32			SetTimer(v8::Local<v8::Value> Action, uint32 Interval, bool Periodic);
	virtual void			ClearTimer(uint32 ID);

protected:
	virtual v8::Local<v8::Object>	ThisObject() = 0;

	friend class CJSObject;
	struct STimer
	{
		uint64	Interval;
		uint64	NextCall;
	};

	map<uint32, STimer>		m_Timers;
};