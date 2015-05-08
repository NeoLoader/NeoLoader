#pragma once

#include "../../Framework/Cryptography/AsymmetricKey.h"

class CAbstractKey;

class CPayloadIndex: public CObject
{
public:
	DECLARE_OBJECT(CPayloadIndex)

	CPayloadIndex(CObject* pParent = NULL);
	virtual ~CPayloadIndex();

	void				Process(UINT Tick);

	CVariant			DoStore(const CUInt128& ID, const CVariant& StoreReq, uint32 TTL, const CVariant& AccessKey);
	CVariant			DoLoad(const CUInt128& ID, const CVariant& LoadReq, uint32 Count);

	CVariant			Store(const CUInt128& ID, const string& Path, const CVariant& Data, time_t Expire, const CVariant& ExclusiveCID = CVariant());
	uint64				Find(const CUInt128& ID, const string& Path, const CVariant& ExclusiveCID = CVariant());
	void				Refresh(uint64 Index, time_t Expire, const CVariant& ExclusiveCID = CVariant());
	bool				List(const CUInt128& ID, const string& Path, vector<SKadEntryInfo>& Entries, const CVariant& ExclusiveCID = CVariant());
	CVariant			Load(uint64 Index, const CVariant& ExclusiveCID = CVariant());
	void				Remove(uint64 Index, const CVariant& ExclusiveCID = CVariant());

	int					CountEntries(const string& Path);
	void				DumpEntries(multimap<CUInt128, SKadEntryInfoEx>& Entries, const string& Path, int Offset, int MaxCount);

private:
	uint64				m_NextCleanup;

	struct sqlite3*		m_pDataBase;
};
