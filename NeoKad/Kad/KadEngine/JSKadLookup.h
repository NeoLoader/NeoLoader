#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../../Common/v8Engine/JSScript.h"
#include "../KadOperation.h"

class CJSKadLookup: public CJSObject, public CJSTimer
{
public:
	virtual ~CJSKadLookup() {}

	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSKadLookup(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate() {return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pLookup;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CKadOperation	Type;

protected:
	virtual v8::Local<v8::Object>	ThisObject();

	CJSKadLookup(CKadOperation* pLookup, CJSScript* pScript) : m_pLookup(pLookup, true) {Instantiate(pScript, false);}

	//static void	GetData(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static void	FxAddRequest(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxStore(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxLoad(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxFinish(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxSetManual(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	GetNode(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetNodes(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static void	FxSelectNode(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxFindNodes(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxAddNode(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	GetTargetID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	SetValue(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
	static void	FxValue(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxGetAccessKey(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxTimer(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pLookup;
};