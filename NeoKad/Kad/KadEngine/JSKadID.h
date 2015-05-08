#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../UIntX.h"

class CJSKadID;

class CKadIDObj: public CObject
{
public:
	DECLARE_OBJECT(CKadIDObj);

	CKadIDObj() {}
	CKadIDObj(const CUInt128& Value) : m_Value(Value) {}

protected:
	friend class CJSKadID;

	CUInt128			m_Value;
};

class CJSKadID: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSKadID(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pKadID;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CKadIDObj	Type;

    static CUInt128		FromValue(v8::Local<v8::Value> value);

protected:
	CJSKadID(CKadIDObj* pKadID, CJSScript* pScript) : m_pKadID(pKadID) {Instantiate(pScript);}
	static void	JSKadID(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxToString(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxToVariant(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type> m_pKadID;
};
