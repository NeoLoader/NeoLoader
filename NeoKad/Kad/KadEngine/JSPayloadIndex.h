#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../PayloadIndex.h"

class CPayloadIndexObj: public CObject
{
public:
	DECLARE_OBJECT(CPayloadIndexObj);

	CPayloadIndexObj(CPayloadIndex* pIndex, const CVariant& ExclusiveCID = CVariant(), CObject* pParent = NULL)
		: CObject(pParent), m_pIndex(pIndex, true) {m_ExclusiveCID = ExclusiveCID;}

	CPayloadIndex*			Index()				{return m_pIndex;}
	const CVariant&			GetExclusiveCID()	{return m_ExclusiveCID;}

protected:
	CPointer<CPayloadIndex>	m_pIndex;
	CVariant				m_ExclusiveCID;
};

class CJSPayloadIndex: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSPayloadIndex(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pIndex;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CPayloadIndexObj	Type;

protected:
	CJSPayloadIndex(CPayloadIndexObj* pIndex, CJSScript* pScript) : m_pIndex(pIndex) {Instantiate(pScript);}

	static void	FxStore(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxList(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxLoad(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxRefresh(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxRemove(const v8::FunctionCallbackInfo<v8::Value> &args);
	// K-ToDo-Now: add some additional function for payload index metrics (load, etc)

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pIndex;
};