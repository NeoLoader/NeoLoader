#pragma once

#include "JSEngine.h"

class CJSDebug;

class CDebugObj: public CObject
{
public:
	DECLARE_OBJECT(CDebugObj);

	CDebugObj(const wstring& Name, void (*Logger)(UINT Flags, const wstring& Trace), CObject* pParent = NULL);
	~CDebugObj() {}

	void				SetName(const wstring& Name)		{m_Name = Name;}
	const wstring&		GetName()							{return m_Name;}

protected:
	friend class CJSDebug;
	wstring				m_Name;

	void (*m_Logger)(UINT Flags, const wstring& Trace);
};

class CJSDebug: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSDebug(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate() {return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pDebug;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CDebugObj	Type;

protected:
	CJSDebug(CDebugObj* pDebug, CJSScript* pScript) : m_pDebug(pDebug) {Instantiate(pScript);}
	~CJSDebug() {}

	static void FxLogLine(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void FxTest(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pDebug;
};

extern bool (*g_LogFilter)(UINT Flags, const wstring& Trace);