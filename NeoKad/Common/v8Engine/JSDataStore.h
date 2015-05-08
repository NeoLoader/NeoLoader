#pragma once

#include "JSEngine.h"
#include "JSVariant.h"

class CDataStoreObj: public CObject
{
public:
	DECLARE_OBJECT(CDataStoreObj);

	CDataStoreObj(CObject* pParent = NULL);

	void				Store(const wstring& Path, const CVariant& Value);
	bool				Retrieve(const wstring& Path, CVariant& Value);
	int					Remove(const wstring& Path);
	int					Copy(const wstring& Path, const wstring& NewPath, bool bMove = false);
	bool				List(const wstring& Path, vector<wstring>& Paths);

	bool				IsEmpty()	{return m_Data.empty();}
	CVariant			Get();
	void				Set(const CVariant& Data);

protected:
	map<wstring, CVariant>	m_Data;
};

class CJSDataStore: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)	{return new CJSDataStore(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate() {return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pDataStore;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CDataStoreObj	Type;

protected:
	CJSDataStore(CDataStoreObj* pDataStore, CJSScript* pScript) : m_pDataStore(pDataStore) {Instantiate(pScript);}

	static void	FxStore(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxLoad(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxRemove(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxCopy(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxRename(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxList(const v8::FunctionCallbackInfo<v8::Value> &args);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pDataStore;
};