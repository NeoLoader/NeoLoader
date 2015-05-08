#pragma once

#include "KadOperation.h"

class CKadTask: public CKadOperation
{
public:
	DECLARE_OBJECT(CKadTask)

	CKadTask(const CUInt128& ID, CObject* pParent = NULL);
	~CKadTask();

	virtual bool		SetupScript(const CVariant& CID);

	virtual void		InitOperation();

	virtual void		Process(UINT Tick);

	virtual void		FlushCaches();

	virtual void		SetStoreKey(CPrivateKey* pStoreKey)			{m_pStoreKey = pStoreKey;}
	virtual CPrivateKey*GetStoreKey()								{return m_pStoreKey;}

	virtual void		AddCall(const string& Name, const CVariant& Arguments, const CVariant& XID);

	virtual CVariant	Store(const string& Path, const CVariant& Data);
	virtual bool		Store(const CVariant& XID, const string& Path, const CVariant& Data);
	virtual CVariant	Load(const string& Path);
	virtual bool		Load(const CVariant& XID, const string& Path);

	virtual CVariant	AddCallRes(const CVariant& CallRes, CKadNode* pNode);
	virtual CVariant	AddStoreRes(const CVariant& StoreRes, CKadNode* pNode);
	virtual CVariant	AddLoadRes(const CVariant& RetrieveRes, CKadNode* pNode);

	virtual void		QueryResults(TRetMap& Results);
	virtual void		QueryStored(TStoredMap& Retrieved);
	virtual void		QueryLoaded(TLoadedMap& Loaded);

	virtual CVariant	GetAccessKey();

	virtual const TRetMap& GetResults()					{return m_Results;}

	virtual const TLoadedMap& GetLoadRes()				{return m_LoadedMap;}

	virtual void		SetName(const wstring& Name)	{m_Name = Name;}
	virtual const wstring& GetName()					{return m_Name;}

protected:
	CHolder<CPrivateKey>m_pStoreKey;

	TCallMap			m_Calls;
	TRetMap				m_Results;
	map<CVariant, vector<time_t> > m_StoredMap;
	TLoadedMap			m_LoadedMap;

	wstring				m_Name; // for GUI
};

