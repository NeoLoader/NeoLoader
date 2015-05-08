#include "GlobalHeader.h"
#include "JSDataStore.h"
#include "../../../Framework/Strings.h"

IMPLEMENT_OBJECT(CDataStoreObj, CObject)

CDataStoreObj::CDataStoreObj(CObject* pParent)
 : CObject(pParent)
{
}

void CDataStoreObj::Store(const wstring& Path, const CVariant& Value)
{
	m_Data[Path] = Value;
}

bool CDataStoreObj::Retrieve(const wstring& Path, CVariant& Value)
{
	map<wstring, CVariant>::iterator I = m_Data.find(Path);
	if(I == m_Data.end())
		return false;
	Value = I->second;
	return true;
}

int CDataStoreObj::Remove(const wstring& Path)
{
	vector<wstring> Paths;
	if(!List(Path, Paths))
		return 0;

	for(size_t i = 0; i < Paths.size(); i++)
	{
		map<wstring, CVariant>::iterator I = m_Data.find(Paths[i]);
		ASSERT(I != m_Data.end());
		m_Data.erase(I);
	}
	return (int)Paths.size();
}

int CDataStoreObj::Copy(const wstring& Path, const wstring& NewPath, bool bMove)
{
	vector<wstring> Paths;
	if(!List(Path, Paths))
		return 0;

	for(size_t i = 0; i < Paths.size(); i++)
	{
		const wstring& CurPath = Paths[i];
		wstring TmpPath = NewPath;
		const wchar_t* Rest = wildcmpex(Path.c_str(),CurPath.c_str());
		ASSERT(Rest);
		if(*Rest)
			TmpPath.append(Rest);

		map<wstring, CVariant>::iterator I = m_Data.find(CurPath);
		ASSERT(I != m_Data.end());
		m_Data[TmpPath] = I->second;
		if(bMove)
			m_Data.erase(I);
	}
	return (int)Paths.size();
}

bool CDataStoreObj::List(const wstring& Path, vector<wstring>& Paths)
{
	for(map<wstring, CVariant>::iterator I = m_Data.begin(); I != m_Data.end(); I++)
	{
		if(wildcmpex(Path.c_str(),I->first.c_str()))
			Paths.push_back(I->first);
	}
	return true;
}

CVariant CDataStoreObj::Get()
{
	CVariant Data;
	for(map<wstring, CVariant>::iterator I = m_Data.begin(); I != m_Data.end(); I++)
	{
		CVariant Entry;
		Entry["Path"] = I->first;
		Entry["Value"] = I->second;
		Data.Append(Entry);
	}
	return Data;
}

void CDataStoreObj::Set(const CVariant& Data)
{
	m_Data.clear();
	for(uint32 i=0; i < Data.Count(); i++)
	{
		const CVariant& Entry = Data.At(i);
		m_Data[Entry["Path"]] = Entry["Value"].Clone(); // note: Data has ben read from a packet and is read only
	}
}

///////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> CJSDataStore::m_Template;

v8::Local<v8::ObjectTemplate> CJSDataStore::Prepare()
{
	v8::EscapableHandleScope HandleScope(v8::Isolate::GetCurrent());

	v8::Local<v8::ObjectTemplate> InstanceTemplate = v8::ObjectTemplate::New();
	InstanceTemplate->SetInternalFieldCount(2);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"store"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxStore),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"load"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxLoad),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"remove"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxRemove),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"copy"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxCopy),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"rename"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxRename),v8::ReadOnly);
	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"list"),v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FxList),v8::ReadOnly);

	InstanceTemplate->Set(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"toString"), v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), CJSObject::FxToString),v8::ReadOnly);

	v8::Local<v8::ObjectTemplate> Template = v8::Local<v8::ObjectTemplate>::New(v8::Isolate::GetCurrent(), InstanceTemplate);
	m_Template.Reset(v8::Isolate::GetCurrent(), Template);

	return HandleScope.Escape(v8::Local<v8::ObjectTemplate>());
}

/** store:
* store data
* 
* @param string path
*	path for the data to be storred at
*
* @param value
*	value to be storred
*/
void CJSDataStore::FxStore(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CDataStoreObj* pDataStore = GetCObject<CDataStoreObj>(args.Holder());
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	CVariant Value = CJSVariant::FromValue(args[1]);
	pDataStore->Store(CJSEngine::GetWStr(args[0]), Value);
	args.GetReturnValue().SetUndefined();
}

/** load:
* retrieve data
* 
* @param string path
*	path to retrive data from
*
* @return value
*	retrived value, or undefined if value wasnt found
*/
void CJSDataStore::FxLoad(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CJSDataStore* jDataStore = GetJSObject<CJSDataStore>(args.Holder());
	CDataStoreObj* pDataStore = jDataStore->m_pDataStore;
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	CVariant Value;
	if(pDataStore->Retrieve(CJSEngine::GetWStr(args[0]), Value))
	{
		CJSObject* jValue = CJSVariant::New(new CVariantPrx(Value), jDataStore->m_pScript);
		args.GetReturnValue().Set(jValue->GetInstance());
	}
	else
		args.GetReturnValue().SetUndefined();
}

/** remove:
* remove data
* 
* @param string path
*	path to remove data at, the path can contain wildcard
*
* @return int
*	removed entry count
*/
void CJSDataStore::FxRemove(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CDataStoreObj* pDataStore = GetCObject<CDataStoreObj>(args.Holder());
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	args.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pDataStore->Remove(CJSEngine::GetWStr(args[0]))));
}

/** copy:
* copy data
* 
* @param string path
*	source path, the path can contain wildcard
*
* @param string new path
*	target path
*
* @return int
*	copyed entry count
*/
void CJSDataStore::FxCopy(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CDataStoreObj* pDataStore = GetCObject<CDataStoreObj>(args.Holder());
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	args.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pDataStore->Copy(CJSEngine::GetWStr(args[0]), CJSEngine::GetWStr(args[1]))));
}

/** rename:
* rename data
* 
* @param string path
*	source path, the path can contain wildcard
*
* @param string new path
*	target path
*
* @return int
*	renamed entry count
*/
void CJSDataStore::FxRename(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CDataStoreObj* pDataStore = GetCObject<CDataStoreObj>(args.Holder());
	if (args.Length() < 2)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	args.GetReturnValue().Set(v8::Integer::New(v8::Isolate::GetCurrent(), pDataStore->Copy(CJSEngine::GetWStr(args[0]), CJSEngine::GetWStr(args[1]), true)));
}

/** list:
* list data
* 
* @param string path
*	path to be searched contain wildcard
*
* @return int
*	found paths
*/
void CJSDataStore::FxList(const v8::FunctionCallbackInfo<v8::Value> &args)
{
	v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
	CDataStoreObj* pDataStore = GetCObject<CDataStoreObj>(args.Holder());
	if (args.Length() < 1)
	{
		args.GetIsolate()->ThrowException(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (uint8_t*)"Invalid Argument"));
		return;
	}
	vector<wstring> Paths;
	if (pDataStore->List(CJSEngine::GetWStr(args[0]), Paths))
	{
		v8::Local<v8::Array> Array = v8::Array::New(v8::Isolate::GetCurrent(), (int)Paths.size());
		for (uint32 i = 0; i < Paths.size(); i++)
			Array->Set(i, v8::String::NewFromTwoByte(v8::Isolate::GetCurrent(), V8STR(Paths[i].c_str())));
		args.GetReturnValue().Set(Array);
	}
	else
		args.GetReturnValue().SetUndefined();
}
