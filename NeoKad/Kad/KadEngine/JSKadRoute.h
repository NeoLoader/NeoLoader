#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../KadRouting/KadRoute.h"

class CKadRouteObj: public CKadRoute
{
public:
	DECLARE_OBJECT(CKadRouteObj);

	CKadRouteObj(const CUInt128& ID, CPrivateKey* pEntityKey = NULL, CObject* pParent = NULL);

protected:
	virtual void		IncomingSession(CRouteSession* pSession);

	virtual CRouteSession*	MkRouteSession(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent);
};

class CJSKadRoute: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSKadRoute(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate() {return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pRoute;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CKadRouteObj	Type;

protected:
	CJSKadRoute(CKadRouteObj* pRoute, CJSScript* pScript) : m_pRoute(pRoute) {Instantiate(pScript);}

	static void	FxOpenSession(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	GetTargetID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetEntityID(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pRoute;
};