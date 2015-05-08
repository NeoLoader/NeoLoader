#pragma once

#include "JSEngine.h"
#include "../../Common/Variant.h"
#include "../../../Framework/Exception.h"

class CCryptoKeyObj;

class CVariantRef: public CVariant, public CObject
{
public:
	DECLARE_OBJECT(CVariantRef);

	CVariantRef() {}
	CVariantRef(const CVariant& Variant) 
		: CVariant(Variant) {}
};

class CVariantPrx: public CObject
{
public:
	DECLARE_OBJECT(CVariantPrx);

	CVariantPrx(CObject* pParent = NULL)
		: CObject(pParent) {m_pVariant = new CVariantRef();}
	CVariantPrx(const CVariant& Variant, CObject* pParent = NULL) 
		: CObject(pParent) {m_pVariant = new CVariantRef(Variant);}
	CVariantPrx(CVariantPrx* pVariant, const vector<string>& Path, CObject* pParent = NULL) 
		: CObject(pParent) {m_pVariant = pVariant->m_pVariant; m_Path = Path;}

	CVariant				GetCopy() const		
	{
		try
		{
			return GetVariantFromPath<CVariant>();
		}
		catch(const CException&) 
		{
			return CVariant("Invalid Variant");
		}
	}
	const vector<string>&	GetPath()			{return m_Path;}

protected:
	friend class CJSVariant;
	const CVariant&			GetVariant() const	{return GetVariantFromPath<const CVariant>();}
	CVariant&				GetVariant()		{return GetVariantFromPath<CVariant>();}

	template <class T>
	T&						GetVariantFromPath() const
	{
		T* pVariant = m_pVariant;
		for(vector<string>::const_iterator I = m_Path.begin(); I != m_Path.end(); I++)
		{
			const string& Name = *I;
			if(Name.size() == 5 && Name.at(0) == 0)
			{
				if(!pVariant->IsList())
                    throw CException(LOG_ERROR | LOG_DEBUG, L"variant type violation; Not a List");
				uint32 Index;
				memcpy(&Index, Name.data() + 1, 4);
				pVariant = &pVariant->At(Index);
			}
			else
			{
				if(!pVariant->IsMap())
                    throw CException(LOG_ERROR | LOG_DEBUG, L"variant type violation; Not a Map");
				pVariant = &pVariant->At(Name.c_str());
			}
		}
		return *pVariant;
	}

	CPointer<CVariantRef>	m_pVariant;
	vector<string>			m_Path;
};


class CJSVariant: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate>	Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSVariant(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pVariant;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CVariantPrx	Type;

	static v8::Local<v8::Value>		ToValue(const CVariant& Variant, CJSScript* pScript);
	static CVariant					FromValue(v8::Local<v8::Value> value_obj);

protected:
	CJSVariant(CVariantPrx* pVariant, CJSScript* pScript) : m_pVariant(pVariant) {Instantiate(pScript);}
	static void	JSVariant(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxIsValid(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxIsSigned(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxIsEncrypted(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxFromPacket(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxToPacket(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxFromCode(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxToCode(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxToString(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	GetByName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	SetByName(v8::Local<v8::String> name, v8::Local<v8::Value> value_obj, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	QueryByName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Integer>& info);
	static void	DelByName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Boolean>& info);
	static void	EnumNames(const v8::PropertyCallbackInfo<v8::Array>& info);

	static void	GetAtIndex(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	SetAtIndex(uint32_t index, v8::Local<v8::Value> value_obj, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	QueryAtIndex(uint32_t index, const v8::PropertyCallbackInfo<v8::Integer>& info);
	static void	DelAtIndex(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info);
	static void	EnumIndexes(const v8::PropertyCallbackInfo<v8::Array>& info);

	static void	GetLength(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	FxAppend(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxSign(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxVerify(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxEncrypt(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxDecrypt(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxConvert(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxClear(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxFreeze(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxUnfreeze(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	static bool				GetCredentials(const v8::FunctionCallbackInfo<v8::Value> &args, CCryptoKeyObj* &pCryptoKey, UINT* eAlgorithm, CCryptoKeyObj** pIV);

	CPointer<Type>			m_pVariant;
};
