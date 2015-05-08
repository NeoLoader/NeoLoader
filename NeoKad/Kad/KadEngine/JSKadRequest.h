#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../../Common/v8Engine/JSScript.h"
#include "KadOperator.h"

class CJSKadRequest: public CJSObject
{
public:
	virtual ~CJSKadRequest() {}

	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSKadRequest(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pRequest;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CKadRequest	Type;

protected:
	CJSKadRequest(CKadRequest* pRequest, CJSScript* pScript) : m_pRequest(pRequest) {Instantiate(pScript, false);}

	//static void	JSKadRequest(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxAddResponse(const v8::FunctionCallbackInfo<v8::Value> &args);
	
	static void	FxValue(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void						SetValue(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pRequest;
};