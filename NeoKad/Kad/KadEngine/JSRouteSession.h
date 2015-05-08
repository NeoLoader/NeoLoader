#pragma once

#include "../../Common/v8Engine/JSEngine.h"
#include "../KadRouting/KadRoute.h"
#include "../KadRouting/RouteSession.h"

class CJSRouteSession;

class CRouteSessionObj: public CRouteSession
{
public:
	DECLARE_OBJECT(CRouteSessionObj)

	CRouteSessionObj(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent = NULL);
	~CRouteSessionObj();

	virtual void			SetupInstance(CJSScript* pScript);

	virtual bool			Process(UINT Tick);

	virtual void			HandleBytes(const CBuffer& Buffer, bool bStream);

	virtual void			QueuePacket(const string &Name, const CVariant& Data, bool bStream);
	virtual void			ProcessPacket(const string& Name, const CVariant& Data, bool bStream);

	size_t					GetQueuedPacketCount()			{return m_PacketQueue.GetCount();}

protected:
	virtual void			Closed(bool bError);

	CPacketQueue			m_PacketQueue;
	CBuffer					m_Stream;

	CJSRouteSession*		m_pJSRouteSession;
};

class CJSRouteSession: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSRouteSession(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pSession;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CRouteSessionObj	Type;

protected:
	CJSRouteSession(CRouteSessionObj* pSession, CJSScript* pScript) : m_pSession(pSession, true) {Instantiate(pScript, false);}

	static void	FxSendPacket(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxClose(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	GetRoute(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pSession;
};