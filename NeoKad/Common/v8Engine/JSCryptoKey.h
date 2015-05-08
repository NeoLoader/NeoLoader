#pragma once

#include "JSEngine.h"

#include "../../../Framework/Cryptography/AbstractKey.h"

class CCryptoKeyObj: public CAbstractKey, public CObject
{
public:
	DECLARE_OBJECT(CCryptoKeyObj);

	CCryptoKeyObj(CObject* pParent = NULL)
		: CObject(pParent) {}
};

class CJSSymKey: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSSymKey(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pCryptoKey;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CCryptoKeyObj	Type;

protected:
	friend class CJSPrivKey;
	friend class CJSPubKey;

	static CCryptoKeyObj*			NewObj(const v8::FunctionCallbackInfo<v8::Value> &args, bool bReadAlgo = true);

	CJSSymKey(CCryptoKeyObj* pCryptoKey, CJSScript* pScript) : m_pCryptoKey(pCryptoKey) {Instantiate(pScript);}
	static void	JSCryptoKey(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxMake(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxIsValid(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxEncrypt(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxDecrypt(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetAlgorithm(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static void	FxToString(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pCryptoKey;
};

class CJSPrivKey: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSPrivKey(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pCryptoKey;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CCryptoKeyObj	Type;

protected:
	CJSPrivKey(CCryptoKeyObj* pCryptoKey, CJSScript* pScript) : m_pCryptoKey(pCryptoKey) {Instantiate(pScript);}
	static void	JSCryptoKey(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxMake(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	GetPubKey(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static void	FxIsValid(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxDecrypt(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxSign(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pCryptoKey;
};

class CJSPubKey: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSPubKey(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pCryptoKey;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CCryptoKeyObj	Type;

protected:
	friend class CJSPrivKey;
	CJSPubKey(CCryptoKeyObj* pCryptoKey, CJSScript* pScript) : m_pCryptoKey(pCryptoKey) {Instantiate(pScript);}
	static void	JSCryptoKey(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxIsValid(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxEncrypt(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxVerify(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pCryptoKey;
};
