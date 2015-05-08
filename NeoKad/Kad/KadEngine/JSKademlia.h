#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../Kademlia.h"

class CJSKademlia: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSKademlia(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate() {return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pKademlia;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CKademlia	Type;

protected:
	CJSKademlia(CKademlia* pKademlia, CJSScript* pScript) : m_pKademlia(pKademlia, true) {Instantiate(pScript);}

	static void	FxSetupRoute(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	GetIndex(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static void	GetNodeID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pKademlia;
};