#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "../KadConfig.h"
#include "../Kademlia.h"
#include "JSRouteSession.h"
#include "KadScript.h"
#include "../../Common/v8Engine/JSScript.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "KadDebugging.h"

IMPLEMENT_OBJECT(CRouteSessionObj, CRouteSession)

CRouteSessionObj::CRouteSessionObj(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent)
: CRouteSession(EntityID, TargetID, pParent) 
{
	m_pJSRouteSession = NULL;
}

CRouteSessionObj::~CRouteSessionObj()
{
	if(m_pJSRouteSession)
		m_pJSRouteSession->MakeWeak();
}

void CRouteSessionObj::SetupInstance(CJSScript* pScript)
{
	if(CJSObject* pObject = pScript->SetObject(this)) // Note: the instance handler for this object is persistent!
		m_pJSRouteSession = (CJSRouteSession*)pObject;
}

bool CRouteSessionObj::Process(UINT Tick)
{
	if(IsConnected())
	{
		for(SVarPacket* pPacket = NULL; (pPacket = m_PacketQueue.Front()) != NULL; m_PacketQueue.Pop())
		{
			if(pPacket->iPriority > 0)
			{
				// K-ToDo-Now: break if tomany frames queued, make a more precise size based estimation
				if(m_FrameQueue.size() >= GetWindowSize() * 2)
					break;

				CBuffer Buffer;
				MakePacket(pPacket->Name, pPacket->Data, Buffer);

				QueueBytes(Buffer, false);
			}
			else if(m_SegmentQueue.size() < GetWindowSize())
			{
				CBuffer Buffer;
				MakePacket(pPacket->Name, pPacket->Data, Buffer);

				QueueBytes(Buffer, true);
			}
			else
				break;
		}
	}

	return CRouteSession::Process(Tick);
}

void CRouteSessionObj::HandleBytes(const CBuffer& Buffer, bool bStream)
{
	if(bStream)
	{
		m_Stream.SetData(-1, Buffer.GetData(), Buffer.GetSize());

		while(m_Stream.GetSize() > 0)
		{
			string Name;
			CVariant Packet;
			if(!StreamPacket(m_Stream, Name, Packet))
				break; // incomplete

			ProcessPacket(Name, Packet, true);
		}
	}
	else
	{
		string Name;
		CVariant Packet;
        CBuffer Temp(Buffer.GetData(), Buffer.GetSize(), true);
        StreamPacket(Temp, Name, Packet);

		ProcessPacket(Name, Packet, false);
	}
}

void CRouteSessionObj::QueuePacket(const string &Name, const CVariant& Data, bool bStream)
{
	m_PacketQueue.Push(Name, Data, bStream ? 0 : 1);
}

void CRouteSessionObj::ProcessPacket(const string& Name, const CVariant& Data, bool bStream)
{
	CKadScript* pKadScript = GetParent<CKadScript>();
	ASSERT(pKadScript);
	CJSScript* pJSScript = pKadScript->GetJSScript();
	if(!pJSScript) // if a script gets terminated all routes get closed to, this should not happen
		return; 

	CDebugScope Debug(pKadScript, this);

	try
	{
		vector<CPointer<CObject> > Arguments;
		Arguments.push_back(new CVariantPrx(Name));
		Arguments.push_back(new CVariantPrx(Data));
		Arguments.push_back(new CVariantPrx(bStream)); 
		CPointer<CObject> Return;
		pJSScript->Call(this, "onPacket", Arguments, Return);
	}
	catch(const CJSException& Exception)
	{
		pKadScript->LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), L"Recived malformated packet from: %s; error %s", ToHex(m_EntityID.GetData(), m_EntityID.GetSize()).c_str(), Exception.GetLine().c_str());
	}
}

void CRouteSessionObj::Closed(bool bError)
{
	CRouteSession::Closed(bError);

	CKadScript* pKadScript = GetParent<CKadScript>();
	ASSERT(pKadScript);
	CJSScript* pJSScript = pKadScript->GetJSScript();
	if(!pJSScript) // if a script gets terminated all routes get closed to, this should not happen
		return;

	CDebugScope Debug(pKadScript, this);

	try
	{
		vector<CPointer<CObject> > Arguments;
		CPointer<CObject> Return;
		pJSScript->Call(this, "onClose", Arguments, Return);
	}
	catch(const CJSException& Exception)
	{
		pKadScript->LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
	}
}

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSRouteSession::m_Template;

v8::Local<v8::ObjectTemplate> CJSRouteSession::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"sendPacket"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxSendPacket),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"close"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxClose),v8::ReadOnly);
	InstanceTemplate->SetAccessor(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"route"), GetRoute, 0, v8::Local<v8::Value>(), v8::DEFAULT, v8::ReadOnly); 

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

void CJSRouteSession::FxSendPacket(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CRouteSessionObj* pSession = GetCObject<CRouteSessionObj>(args.Holder());
	if (!pSession)
	{
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), false));
		return;
	}
	CKadScript* pKadScript = pSession->GetParent<CKadScript>();
	ASSERT(pKadScript);
	if (args.Length() < 2 || !args[1]->IsObject())
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	string Name;
	if(args[0]->IsString())
		Name = CJSEngine::GetStr(args[0]);
	else if(args[0]->IsObject())
	{
		if(CVariantPrx* pName = GetCObject<CVariantPrx>(args[0]->ToObject()))
			Name = pName->GetCopy().To<string>();
	}
	CVariantPrx* pData = GetCObject<CVariantPrx>(args[1]->ToObject());
	if (!pData)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	bool bStream = true;
	if(args.Length() >= 3)
		bStream = args[2]->BooleanValue();
	pSession->QueuePacket(Name, pData->GetCopy(), bStream);
	args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), true));
}

void CJSRouteSession::FxClose(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	CRouteSessionObj* pSession = GetCObject<CRouteSessionObj>(args.Holder());
	if (pSession)
	{
		pSession->CloseSession();
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), true));
	}
	else
		args.GetReturnValue().Set(v8::Boolean::New(v8::Isolate::GetCurrent(), false));
}

void CJSRouteSession::GetRoute(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSRouteSession* jSession = GetJSObject<CJSRouteSession>(info.Holder());
	if(jSession->m_pSession)
		info.GetReturnValue().Set(jSession->m_pScript->GetObject(jSession->m_pSession->GetParent<CKadRoute>()));
	else
		info.GetReturnValue().SetNull();
}
