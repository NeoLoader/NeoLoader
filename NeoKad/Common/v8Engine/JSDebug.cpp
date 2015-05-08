#include "GlobalHeader.h"
#include "JSDebug.h"

bool (*g_LogFilter)(UINT Flags, const wstring& Trace) = NULL;

IMPLEMENT_OBJECT(CDebugObj, CObject)

CDebugObj::CDebugObj(const wstring& Name, void (*Logger)(UINT Flags, const wstring& Trace), CObject* pParent)
 : CObject(pParent)
{
	m_Name = Name;
	m_Logger = Logger;
}

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSDebug::m_Template;

v8::Local<v8::ObjectTemplate> CJSDebug::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"log"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxLogLine, v8::Integer::New(v8::Isolate::GetCurrent(), LOG_NOTE)), v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"done"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxLogLine, v8::Integer::New(v8::Isolate::GetCurrent(), LOG_SUCCESS)),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"info"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxLogLine, v8::Integer::New(v8::Isolate::GetCurrent(), LOG_INFO)),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"warn"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxLogLine, v8::Integer::New(v8::Isolate::GetCurrent(), LOG_WARNING)),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"error"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxLogLine, v8::Integer::New(v8::Isolate::GetCurrent(), LOG_ERROR)),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"test"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxTest),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString), v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

void CJSDebug::FxLogLine(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CDebugObj* pDebug = GetCObject<CDebugObj>(args.Holder());

	for(int i=0; i < args.Length(); i++)
	{
		UINT Flags = args.Data()->Uint32Value();
		wstring Trace = CJSEngine::GetWStr(args[i]);
		if(!g_LogFilter || !g_LogFilter(Flags, Trace))
		{
			if(pDebug->m_Logger)
				pDebug->m_Logger(Flags, Trace);
			else
				pDebug->LogLine(LOG_DEBUG | Flags, L"JS '%s' Debug: %s", pDebug->GetName().c_str(), Trace.c_str());
		}
	}
}

void CJSDebug::FxTest(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
}
