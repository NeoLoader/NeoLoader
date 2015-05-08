#pragma once

#include "JSEngine.h"

#include "../../../Framework/Buffer.h"

class CBufferObj: public CBuffer, public CObject
{
public:
	DECLARE_OBJECT(CBufferObj);

	CBufferObj(const CBuffer& Buffer, CObject* pParent = NULL)
		: CBuffer(Buffer), CObject(pParent) {}
	CBufferObj(CObject* pParent = NULL)
		: CObject(pParent) {}
};

class CJSBuffer: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSBuffer(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate()	{return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pBuffer;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CBufferObj	Type;

protected:
	CJSBuffer(CBufferObj* pBuffer, CJSScript* pScript) : m_pBuffer(pBuffer) {Instantiate(pScript);}
	static void	JSBuffer(const v8::FunctionCallbackInfo<v8::Value> &args);
	~CJSBuffer() {}

	static void	FxRead(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxWrite(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxReadValue(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxWriteValue(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxReadString(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxWriteString(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxSeek(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxResize(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxToString(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	GetLength(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetPosition(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pBuffer;
};
