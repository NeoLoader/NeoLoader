#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../../Common/v8Engine/JSScript.h"
#include "KadScript.h"

class CJSKadScript: public CJSObject, public CJSTimer
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSKadScript(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate() {return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pKadScript;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CKadScript	Type;

protected:
	virtual v8::Local<v8::Object>	ThisObject();

	CJSKadScript(CKadScript* pKadScript, CJSScript* pScript) : m_pKadScript(pKadScript, true) {Instantiate(pScript);} // Note: this pointer must be weak or else the script wil never be released

	static void	GetData(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetIndex(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetEngineVersion(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetSecretKey(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void SetSecretKey(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);

	static void	FxInvoke(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxTimer(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pKadScript;
};