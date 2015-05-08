#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../KadNode.h"
#include "../KadOperation.h"

class CJSKadNode: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSKadNode(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate() {return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pNode;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	virtual void		SetOperation(CKadOperation* pOperation)	{m_pLookup = CPointer<CKadOperation>(pOperation, true);}

	typedef CKadNode	Type;

protected:
	CJSKadNode(CKadNode* pNode, CJSScript* pScript) : m_pNode(pNode, true) {Instantiate(pScript, false);}

	static void	GetNodeID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetValue(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	//static void						SetValue(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info);

	static void	FxSendMessage(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxValue(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	GetLookup(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>				m_pNode;
	CPointer<CKadOperation>		m_pLookup;
};