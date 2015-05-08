#include "GlobalHeader.h"
#include "SQLite.h"
#include "../../sqlite3/sqlite3.h"
#include "FileIO.h"

struct sqlite3* OpenSQLite(const wstring& Path, bool* IsNew)
{
	int Error;
	struct sqlite3* pDB = NULL;
	if(Path.empty())
		Error = sqlite3_open(":memory:", &pDB);
	else
	{
		if(IsNew)
			*IsNew = (GetFileSize(Path) == 0);
		Error = sqlite3_open(ToPlatformNotation(Path).c_str(), &pDB);
	}
	if(Error)
	{
		LogLine(LOG_ERROR, L"Can't open database: %S", sqlite3_errmsg(pDB));
		return NULL;
	}
	return pDB;
}

void CloseSQLite(struct sqlite3* pDB)
{
	sqlite3_close(pDB); 
}

CSQLiteQuery::CSQLiteQuery()
{
	m_bData = false; 
	m_pQuery = NULL;
}

CSQLiteQuery::~CSQLiteQuery()
{
	sqlite3_finalize(m_pQuery);
}

bool CSQLiteQuery::Prepare(struct sqlite3* pDB, const string& Query)
{
	if(sqlite3_prepare_v2(pDB, Query.c_str(), (int)((Query.length() + 1) * sizeof(string::value_type)), &m_pQuery, NULL) == SQLITE_OK)
		return true;
	string Error = sqlite3_errmsg(pDB);
	LogLine(LOG_ERROR, L"SQLite query \"%S\" failed: %S", Query.c_str(), Error.c_str());
	ASSERT(0);
	return false;
}

bool CSQLiteQuery::BindInt(const char* pName, sint64 Int)
{
	bool bOK = false;
	if(int Index = sqlite3_bind_parameter_index(m_pQuery, pName))
	{
		if(sqlite3_bind_int64(m_pQuery, Index, Int) == SQLITE_OK)
			bOK = true;
		else
			LogLine(LOG_ERROR, L"SQLite invalid parameter: %S", pName);
	}
	else
		LogLine(LOG_ERROR, L"SQLite missing parameter: %S", pName);
	ASSERT(bOK);
	return bOK;
}

bool CSQLiteQuery::BindStr(const char* pName, const string& Str)
{
	bool bOK = false;
	if(int Index = sqlite3_bind_parameter_index(m_pQuery, pName))
	{
		if(sqlite3_bind_text(m_pQuery, Index, Str.c_str(), (int)Str.length(), SQLITE_TRANSIENT) == SQLITE_OK)
			bOK = true;
		else
			LogLine(LOG_ERROR, L"SQLite invalid parameter: %S", pName);
	}
	else
		LogLine(LOG_ERROR, L"SQLite missing parameter: %S", pName);
	ASSERT(bOK);
	return bOK;
}

bool CSQLiteQuery::BindWStr(const char* pName, const wstring& WStr)
{
	bool bOK = false;
	if(int Index = sqlite3_bind_parameter_index(m_pQuery, pName))
	{
		if(sqlite3_bind_text16(m_pQuery, Index, WStr.c_str(), (int)(WStr.length() * sizeof(wstring::value_type)), SQLITE_TRANSIENT) == SQLITE_OK)
			bOK = true;
		else
			LogLine(LOG_ERROR, L"SQLite invalid parameter: %S", pName);
	}
	else
		LogLine(LOG_ERROR, L"SQLite missing parameter: %S", pName);
	ASSERT(bOK);
	return bOK;
}

bool CSQLiteQuery::BindBlob(const char* pName, const byte* pBlob, size_t uSize)
{
	bool bOK = false;
	if(int Index = sqlite3_bind_parameter_index(m_pQuery, pName))
	{
		if(pBlob)
			bOK = (sqlite3_bind_blob(m_pQuery, Index, pBlob, (int)uSize, SQLITE_TRANSIENT) == SQLITE_OK);
		else
			bOK = (sqlite3_bind_zeroblob(m_pQuery, Index, (int)uSize) == SQLITE_OK);
		if(!bOK)
			LogLine(LOG_ERROR, L"SQLite invalid parameter: %S", pName);
	}
	else
		LogLine(LOG_ERROR, L"SQLite missing parameter: %S", pName);
	ASSERT(bOK);
	return bOK;
}

bool CSQLiteQuery::Execute()
{
	int Error = sqlite3_step(m_pQuery);
	if(Error == SQLITE_ROW)
		m_bData = true;
	else if(Error == SQLITE_DONE)
		m_bData = false;
	else
	{
		LogLine(LOG_ERROR, L"SQLite execute failed");
		return false;
	}
	return true;
}

sint64 CSQLiteQuery::GetInt(int Index)
{
	return sqlite3_column_int64(m_pQuery, Index);
}

string CSQLiteQuery::GetString(int Index)
{
	return string((string::value_type*)sqlite3_column_text(m_pQuery, Index), sqlite3_column_bytes(m_pQuery, Index));
}

wstring CSQLiteQuery::GetWString(int Index)
{
	return wstring((wstring::value_type*)sqlite3_column_text16(m_pQuery, Index), sqlite3_column_bytes16(m_pQuery, Index)/sizeof(wstring::value_type));
}

size_t CSQLiteQuery::GetBlobSize(int Index)
{
	int Type = sqlite3_column_type(m_pQuery, Index);
	if(Type == SQLITE_BLOB)
		return sqlite3_column_bytes(m_pQuery, Index);
	return 0;
}

byte* CSQLiteQuery::GetBlobBytes(int Index)
{
	int Type = sqlite3_column_type(m_pQuery, Index);
	if(Type == SQLITE_BLOB)
		return (byte*)sqlite3_column_blob(m_pQuery, Index);
	return NULL;
}