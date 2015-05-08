#pragma once

#include "JSEngine.h"
#include "../../../Framework/Cryptography/HashFunction.h"

class CHashingObj: public CHashFunction, public CObject
{
public:
	DECLARE_OBJECT(CHashingObj);

	CHashingObj(UINT eAlgorithm, CObject* pParent = NULL)
		: CHashFunction(eAlgorithm), CObject(pParent) {}
};

class CJSHashing: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSHashing(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pHashing;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CHashingObj	Type;

protected:
	CJSHashing(CHashingObj* pHashing, CJSScript* pScript) : m_pHashing(pHashing) {Instantiate(pScript);}
	static void	JSHashing(const v8::FunctionCallbackInfo<v8::Value> &args);
	~CJSHashing() {}

	static void	FxReset(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxUpdate(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxFinish(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxHash(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetAlgorithm(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static void	FxToString(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pHashing;
};

