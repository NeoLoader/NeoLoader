#include "GlobalHeader.h"
#include "KadHeader.h"
#include "Kademlia.h"
#include "PayloadIndex.h"
#include "KadConfig.h"
#include "../Common/SQLite.h"

IMPLEMENT_OBJECT(CPayloadIndex, CObject)

bool SQLiteExec(sqlite3* pDB, const string& Query)
{
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return false;
	return SQLiteQuery.Execute();
}

CPayloadIndex::CPayloadIndex(CObject* pParent)
: CObject(pParent) 
{
	m_NextCleanup = 0;

	bool bNew = true;
	wstring ConfigPath = GetParent<CKademlia>()->Cfg()->GetString("ConfigPath");
	m_pDataBase = OpenSQLite(ConfigPath.empty() ? L"" : ConfigPath + L"Cache/Index.sq3", &bNew);
	if(!m_pDataBase)
		return;

	SQLiteExec(m_pDataBase, "pragma synchronous = off");
	SQLiteExec(m_pDataBase, "pragma journal_mode = off");
	SQLiteExec(m_pDataBase, "pragma locking_mode = exclusive");
	SQLiteExec(m_pDataBase, "pragma cache_size = 2000"); //*1024 -> 2 MB // 2000 - 2MB pages is default

	int Version = -1;
	if(!bNew)
	{
		Version = 0;

		CSQLiteQuery SQLiteQuery;
		if(SQLiteQuery.Prepare(m_pDataBase, "SELECT value FROM infos WHERE key = 'ver'") && SQLiteQuery.Execute() && SQLiteQuery.HasData())
			Version = string2int(SQLiteQuery.GetString(0));
	}

	if(Version < 1) // check of DB is up to date
	{
		if(Version < 1)
			SQLiteExec(m_pDataBase, "CREATE TABLE infos (key TEXT, value TEXT, UNIQUE(key) ON CONFLICT REPLACE)");
		SQLiteExec(m_pDataBase, "INSERT INTO infos (key, value) VALUES ('ver', '1')"); // update DB Version

		// Update DB content
		if(Version >= 0)
			SQLiteExec(m_pDataBase, "DROP TABLE entries");
		SQLiteExec(m_pDataBase, "CREATE TABLE entries (idx INTEGER PRIMARY KEY AUTOINCREMENT, id BLOB, path TEXT, payload BLOB, pub INTEGER, exp INTEGER, ak BLOB, cid BLOB)");
		SQLiteExec(m_pDataBase, "CREATE INDEX id_index ON entries (id)");
	}
}

CPayloadIndex::~CPayloadIndex()
{
	CloseSQLite(m_pDataBase); 
}

void CleanUpEntries(sqlite3* pDB)
{
	string Query = "DELETE FROM entries WHERE exp < :exp";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return;

	SQLiteQuery.BindInt(":exp", GetTime());
	SQLiteQuery.Execute();
}

void CPayloadIndex::Process(UINT Tick)
{
	if((Tick & EPer100Sec) != 0 && m_NextCleanup < GetCurTick())
	{
		CleanUpEntries(m_pDataBase);
		m_NextCleanup = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt64("IndexCleanupInterval"));
	}
}

////////////////////////////////////////////
// Remote Ops

pair<uint64, time_t> FindEntry(sqlite3* pDB, const CUInt128& ID, const string& Path, const CVariant& AccessKey, const CVariant& ExclusiveCID = CVariant())
{
	string Query = "SELECT idx, pub FROM entries WHERE id = :id AND path = :path AND ak = :ak AND cid = :cid";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return make_pair(0, 0);

	SQLiteQuery.BindBlob(":id", ID.GetData(), ID.GetSize());
	SQLiteQuery.BindStr(":path", Path);
	SQLiteQuery.BindBlob(":ak", AccessKey.GetData(), AccessKey.GetSize());
	SQLiteQuery.BindBlob(":cid", ExclusiveCID.GetData(), ExclusiveCID.GetSize());
	if(!SQLiteQuery.Execute() || !SQLiteQuery.HasData())
		return make_pair(0, 0);

	return make_pair(SQLiteQuery.GetInt(0), SQLiteQuery.GetInt(1));
}

bool SetEntry(sqlite3* pDB, const CUInt128& ID, const CVariant& Payload, time_t Expire, const CVariant& AccessKey, const CVariant& ExclusiveCID = CVariant())
{
	string Query = "INSERT INTO entries (id, path, payload, pub, exp, ak, cid) VALUES (:id, :path, :payload, :pub, :exp, :ak, :cid)";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return false;

	string Path = Payload["PATH"];
	SQLiteQuery.BindBlob(":id", ID.GetData(), ID.GetSize());
	SQLiteQuery.BindStr(":path", Path);
	CBuffer Packet;
	Payload.ToPacket(&Packet);
	SQLiteQuery.BindBlob(":payload", Packet.GetBuffer(), Packet.GetSize());
	SQLiteQuery.BindInt(":pub", Min(GetTime(), Payload["RELD"].To<uint64>())); // now or the release data, Min prevents RELD faking
	SQLiteQuery.BindInt(":exp", Expire); // in future
	SQLiteQuery.BindBlob(":ak", AccessKey.GetData(), AccessKey.GetSize());
	SQLiteQuery.BindBlob(":cid", ExclusiveCID.GetData(), ExclusiveCID.GetSize());
	return SQLiteQuery.Execute();
}

bool DelData(sqlite3* pDB, uint64 Index, const CVariant& ExclusiveCID = CVariant())
{
	string Query = "DELETE FROM entries WHERE idx = :idx AND cid = :cid";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return false;

	SQLiteQuery.BindInt(":idx", Index);
	SQLiteQuery.BindBlob(":cid", ExclusiveCID.GetData(), ExclusiveCID.GetSize());
	return SQLiteQuery.Execute();
}

bool RefreshEntry(sqlite3* pDB, uint64 Index, time_t Expire, time_t ReleaseDate, const CVariant& ExclusiveCID = CVariant())
{
	string Query = "UPDATE entries SET pub = :pub, exp = :exp WHERE idx = :idx AND cid = :cid";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return false;

	SQLiteQuery.BindInt(":idx", Index);
	SQLiteQuery.BindBlob(":cid", ExclusiveCID.GetData(), ExclusiveCID.GetSize());
	SQLiteQuery.BindInt(":pub", ReleaseDate);
	SQLiteQuery.BindInt(":exp", Expire); // future date
	return SQLiteQuery.Execute();
}

pair<uint64, uint64> GetLoad(sqlite3* pDB, const CUInt128& ID, uint64 Interval, const CVariant& ExclusiveCID = CVariant())
{
	string Query = "SELECT COUNT(id), MIN(pub) FROM entries WHERE id = :id AND cid = :cid AND pub > :pub";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return make_pair(0,0);

	time_t Now = GetTime();

	SQLiteQuery.BindBlob(":id", ID.GetData(), ID.GetSize());
	SQLiteQuery.BindBlob(":cid", ExclusiveCID.GetData(), ExclusiveCID.GetSize());
	SQLiteQuery.BindInt(":pub", Now - Interval);

	if(!SQLiteQuery.Execute() || !SQLiteQuery.HasData())
		return make_pair(0,0);

	return make_pair(SQLiteQuery.GetInt(0), Now - SQLiteQuery.GetInt(1));
}

CVariant CPayloadIndex::DoStore(const CUInt128& ID, const CVariant& StoreReq, uint32 TTL, const CVariant& AccessKey)
{
	time_t MaxTTL = GetParent<CKademlia>()->Cfg()->GetInt("MaxPayloadExpiration");
	if(!TTL || TTL > MaxTTL)
		TTL = MaxTTL;

	CScoped<CPublicKey> pPubKey = NULL;
	if(AccessKey.IsValid())
	{
		pPubKey = new CPublicKey();	
		if(!pPubKey->SetKey(AccessKey.GetData(), AccessKey.GetSize()))
		{
			CVariant StoreRes;
			LogLine(LOG_ERROR, L"Recived Invalid Key for Payload");
			StoreRes["ERR"] = "InvalidKey";
			return StoreRes;
		}
	}

	const CVariant& StoreList = StoreReq["REQ"];
	CVariant StoredList;
	for(uint32 i=0; i < StoreList.Count(); i++)
	{
		const CVariant& Store = StoreList.At(i);

		CVariant Result;
		Result["XID"] = Store["XID"];

		CVariant Payload = Store["PLD"];

		if(pPubKey && !Payload.Verify(pPubKey))
		{
			LogLine(LOG_ERROR, L"recived payload with an invalid signature");
			Result["ERR"] = "InvalidSign";
			StoredList.Append(Result);
			continue;
		}

		pair<uint64, time_t> OldEntry = make_pair(0,0);
		if(AccessKey.IsValid()) // Note: we store as many instances of uncontrolled data as we get till thay expire, ther for there is no entry refreshing in this mode
			OldEntry = FindEntry(m_pDataBase, ID, Payload["PATH"], AccessKey);

		if(OldEntry.first == 0 && !Payload.Has("DATA")) // no DATA means this was an update attempts, if teh paylaod is not storred quit with an error
		{
			Result["ERR"] = "NotStored";
			StoredList.Append(Result);
			continue;
		}

		time_t ReleaseDate = Min(Payload["RELD"].To<uint64>(), GetTime()); // now or the release data, Min prevents RELD faking
		if(ReleaseDate < OldEntry.second) // check if the storred entry is newer than the one we just got, deny overwriting a newer entry
		{
			Result["ERR"] = "Outated";
			StoredList.Append(Result);
			continue;
		}

        Result["EXP"] = (uint64)ReleaseDate + TTL;

		if(Payload.Has("DATA")) // missing for refresh only, empty/invalid for delete now
		{
			if(OldEntry.first != 0) // remove old entry
				DelData(m_pDataBase, OldEntry.first);
			SetEntry(m_pDataBase, ID, Payload, ReleaseDate + TTL, AccessKey);
		}
		else
			RefreshEntry(m_pDataBase, OldEntry.first, ReleaseDate + TTL, ReleaseDate);

		StoredList.Append(Result);
	}

	pair<uint64, uint64> LoadData = GetLoad(m_pDataBase, ID, GetParent<CKademlia>()->Cfg()->GetInt("LoadInterval"));
	CVariant Load;
	Load["CNT"] = LoadData.first;
	Load["INT"] = LoadData.second;

	CVariant StoreRes;
	StoreRes["RES"] = StoredList;
	StoreRes["LOAD"] = Load;
	return StoreRes;
}

bool EnumData(multimap<time_t, uint64>& Paths, sqlite3* pDB, const CUInt128& ID, const string& Wildcard, const CVariant& ExclusiveCID = CVariant())
{
	string Query = "SELECT idx, path, pub FROM entries WHERE id = :id AND cid = :cid";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return false;

	SQLiteQuery.BindBlob(":id", ID.GetData(), ID.GetSize());
	SQLiteQuery.BindBlob(":cid", ExclusiveCID.GetData(), ExclusiveCID.GetSize());
	
	while(SQLiteQuery.Execute() && SQLiteQuery.HasData())
	{
		string Path = SQLiteQuery.GetString(1);
		if(!wildcmpex(Wildcard.c_str(), Path.c_str()))
			continue;
			
		Paths.insert(multimap<time_t, uint64>::value_type(SQLiteQuery.GetInt(2), SQLiteQuery.GetInt(0)));
	}
	return true;
}

CVariant GetData(sqlite3* pDB, uint64 Index, const CVariant& ExclusiveCID = CVariant())
{
	string Query = "SELECT payload FROM entries WHERE idx = :idx AND cid = :cid";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return CVariant();

	SQLiteQuery.BindInt(":idx", Index);
	SQLiteQuery.BindBlob(":cid", ExclusiveCID.GetData(), ExclusiveCID.GetSize());

	if(!SQLiteQuery.Execute() || !SQLiteQuery.HasData())
		return CVariant();

	CVariant Data;
	CBuffer Packet(SQLiteQuery.GetBlobBytes(0), SQLiteQuery.GetBlobSize(0), true);
	Data.FromPacket(&Packet);
	return Data;
}

CVariant CPayloadIndex::DoLoad(const CUInt128& ID, const CVariant& LoadReq, uint32 Count)
{
	int MaxCount = GetParent<CKademlia>()->Cfg()->GetInt("MaxPayloadReturn");
	if(!Count || Count > MaxCount)
		Count = MaxCount;

	const CVariant& LoadList = LoadReq["REQ"];
	CVariant LoadedList;
	for(uint32 i=0; i < LoadList.Count(); i++)
	{
		const CVariant& Load = LoadList.At(i);

		multimap<time_t, uint64> Found; // sort ba age
		EnumData(Found, m_pDataBase, ID, Load["PATH"]);

		CVariant Payloads(CVariant::EList);
		for(multimap<time_t, uint64>::iterator I = Found.end(); I != Found.begin() && Payloads.Count() < Count;) // sort by date newest first
		{
			I--;
			Payloads.Append(GetData(m_pDataBase, I->second));
		}

		CVariant Loaded;
		Loaded["XID"] = Load["XID"];
		Loaded["PLD"] = Payloads;
		LoadedList.Append(Loaded);
	}

	CVariant RetrieveRes;
	RetrieveRes["RES"] = LoadedList;
	return RetrieveRes;
}


////////////////////////////////////////////
// Local Ops

CVariant CPayloadIndex::Store(const CUInt128& ID, const string& Path, const CVariant& Data, time_t Expire, const CVariant& ExclusiveCID)
{
	time_t Now = GetTime();
	time_t MaxExpire = Now + GetParent<CKademlia>()->Cfg()->GetInt("MaxPayloadExpiration");
	
	if(Expire == -1)
		Expire = MaxExpire;
	else if(Expire > MaxExpire)
		Expire = MaxExpire;

	CVariant Payload;
	Payload["PATH"] = Path;
	Payload["DATA"] = Data;
	Payload["RELD"] = GetTime();

	pair<uint64, time_t> Entry = FindEntry(m_pDataBase, ID, Path, CVariant(), ExclusiveCID);
	if(Entry.first) // kill old entry
		Remove(Entry.first, ExclusiveCID);
	SetEntry(m_pDataBase, ID, Payload, Expire, CVariant(), ExclusiveCID);
	
	pair<uint64, uint64> LoadData = GetLoad(m_pDataBase, ID, GetParent<CKademlia>()->Cfg()->GetInt("LoadInterval"));
	CVariant Load;
	Load["CNT"] = LoadData.first;
	Load["INT"] = LoadData.second;
	return Load;
}

uint64 CPayloadIndex::Find(const CUInt128& ID, const string& Path, const CVariant& ExclusiveCID)
{
	return FindEntry(m_pDataBase, ID, Path, CVariant(), ExclusiveCID).first;
}

void CPayloadIndex::Refresh(uint64 Index, time_t Expire, const CVariant& ExclusiveCID)
{
	RefreshEntry(m_pDataBase, Index, Expire, GetTime(), ExclusiveCID);
}

bool EnumData(vector<SKadEntryInfo>& Found, sqlite3* pDB, const CUInt128& ID, const string& Wildcard, const CVariant& ExclusiveCID = CVariant())
{
	string Query = ID == 0 ? "SELECT idx, path, pub, exp, id FROM entries WHERE cid = :cid" : "SELECT idx, path, pub, exp, id FROM entries WHERE id = :id AND cid = :cid";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return false;

	if(ID != 0)
		SQLiteQuery.BindBlob(":id", ID.GetData(), ID.GetSize());
	SQLiteQuery.BindBlob(":cid", ExclusiveCID.GetData(), ExclusiveCID.GetSize());
	
	while(SQLiteQuery.Execute() && SQLiteQuery.HasData())
	{
		string Path = SQLiteQuery.GetString(1);
		if(!wildcmpex(Wildcard.c_str(),Path.c_str()))
			continue;

		SKadEntryInfo Entry;
		memcpy(Entry.ID.GetData(), SQLiteQuery.GetBlobBytes(4), Entry.ID.GetSize());
		Entry.Index = SQLiteQuery.GetInt(0);
		Entry.Path = Path;
		Entry.Date = SQLiteQuery.GetInt(2);
		Entry.Expire = SQLiteQuery.GetInt(3);
		Found.push_back(Entry);	
	}
	return true;
}

bool CPayloadIndex::List(const CUInt128& ID, const string& Path, vector<SKadEntryInfo>& Entries, const CVariant& ExclusiveCID)
{
	EnumData(Entries, m_pDataBase, ID, Path, ExclusiveCID);
	return Entries.size() > 0;
}

CVariant CPayloadIndex::Load(uint64 Index, const CVariant& ExclusiveCID)
{
	return GetData(m_pDataBase, Index, ExclusiveCID).Get("DATA");
}

void CPayloadIndex::Remove(uint64 Index, const CVariant& ExclusiveCID)
{
	DelData(m_pDataBase, Index, ExclusiveCID);
}

int CountEntries(sqlite3* pDB, const string& Path)
{
	string Query = "SELECT COUNT(id) FROM entries";
	if(!Path.empty())
		Query += " WHERE path LIKE :path";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return 0;

	if(!Path.empty())
		SQLiteQuery.BindStr(":path", Path);

	if(!SQLiteQuery.Execute() || !SQLiteQuery.HasData())
		return 0;

	return SQLiteQuery.GetInt(0);
}

int CPayloadIndex::CountEntries(const string& Path)
{
	return ::CountEntries(m_pDataBase, Path);
}

bool DumpEntries(sqlite3* pDB, multimap<CUInt128, SKadEntryInfoEx>& Entries, const string& Path, int Offset, int MaxCount)
{
	string Query = "SELECT idx, path, pub, exp, id, ak, cid FROM entries";
	if(!Path.empty())
		Query += " WHERE path LIKE :path";
	CSQLiteQuery SQLiteQuery;
	if(!SQLiteQuery.Prepare(pDB, Query))
		return false;

	if(!Path.empty())
		SQLiteQuery.BindStr(":path", Path);

	int Index = -1;
	while(SQLiteQuery.Execute() && SQLiteQuery.HasData())
	{
		Index++;
		if(Index < Offset)
			continue;
		else if(Index > Offset + MaxCount)
			break;

		SKadEntryInfoEx Entry;
		memcpy(Entry.ID.GetData(), SQLiteQuery.GetBlobBytes(4), Entry.ID.GetSize());
		Entry.Index = SQLiteQuery.GetInt(0);
		Entry.Path = SQLiteQuery.GetString(1);
		Entry.Date = SQLiteQuery.GetInt(2);
		Entry.Expire = SQLiteQuery.GetInt(3);
		if(SQLiteQuery.GetBlobSize(5) > 0)
			Entry.pAccessKey = new CAbstractKey(SQLiteQuery.GetBlobBytes(5), SQLiteQuery.GetBlobSize(5));
		if(SQLiteQuery.GetBlobSize(6) > 0)
			Entry.ExclusiveCID = CVariant(SQLiteQuery.GetBlobBytes(6), SQLiteQuery.GetBlobSize(6));
		Entries.insert(map<CUInt128, SKadEntryInfoEx>::value_type(Entry.ID,Entry));	
	}
	return true;
}

void CPayloadIndex::DumpEntries(multimap<CUInt128, SKadEntryInfoEx>& Entries, const string& Path, int Offset, int MaxCount)
{
	::DumpEntries(m_pDataBase, Entries, Path, Offset, MaxCount);
}
