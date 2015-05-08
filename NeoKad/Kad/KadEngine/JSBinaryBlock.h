#pragma once

#include "../../Common/v8Engine/JSEngine.h"

class CBinaryBlock: public CObject
{
public:
	DECLARE_OBJECT(CBinaryBlock);

	CBinaryBlock(bool bTemporary, CObject* pParent = NULL);
	CBinaryBlock(const wstring& FileName, CObject* pParent = NULL);
	virtual ~CBinaryBlock();

	void				Remove();
	bool				Read(uint64 Offset, uint64 Length, CJSScript* pScript = NULL, uint32 Callback = 0);
	bool				Write(uint64 Offset, const CBuffer& Data, CJSScript* pScript = NULL, uint32 Callback = 0);
	uint64				GetSize();
	const wstring&		GetName()		{return m_FileName;}
	void				SetPermanent();
	bool				IsTemporary()	{return m_LastUsed == 0;}
	time_t				GetLastUsed()	{return m_LastUsed;}
	void				Dispose()		{m_FileName.clear();}

	void				SaveData(const wstring& Path);
	bool				LoadData(const wstring& Path);

protected:
	wstring				m_FileName;
	time_t				m_LastUsed;
};

class CJSBinaryBlock: public CJSObject
{
public:
	static v8::Local<v8::ObjectTemplate> Prepare();
	static CJSObject*	New(CObject* pObject, CJSScript* pScript)			{return new CJSBinaryBlock(pObject->Cast<Type>(), pScript);}
	v8::Persistent<v8::ObjectTemplate>& GetTemplate() {return m_Template;}
	virtual CObject*	GetObjectPtr()					{return m_pBlock;}
	virtual char*		GetObjectName()					{return Type::StaticName();}

	typedef CBinaryBlock	Type;

protected:
	friend class CJSBinaryCache;
	CJSBinaryBlock(CBinaryBlock* pBlock, CJSScript* pScript) : m_pBlock(pBlock) {Instantiate(pScript);}

	//static void	JSBinaryBlock(const v8::FunctionCallbackInfo<v8::Value> &args);

	static void	FxRemove(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxRead(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	FxWrite(const v8::FunctionCallbackInfo<v8::Value> &args);
	static void	GetName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);
	static void	GetSize(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info);

	static v8::Persistent<v8::ObjectTemplate>	m_Template;

	CPointer<Type>	m_pBlock;
};