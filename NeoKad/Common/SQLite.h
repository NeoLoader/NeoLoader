#pragma once

struct sqlite3* OpenSQLite(const wstring& Path, bool* IsNew = NULL);
void CloseSQLite(struct sqlite3* pDB);

class CSQLiteQuery
{
public:
	CSQLiteQuery();
	~CSQLiteQuery();

	bool	Prepare(struct sqlite3* pDB, const string& Query);

	bool		BindInt(const char* pName, sint64 Int);
	bool		BindStr(const char* pName, const string& Str);
	bool		BindWStr(const char* pName, const wstring& WStr);
	bool		BindBlob(const char* pName, const byte* pBlob, size_t uSize);

	bool		Execute();

	bool		HasData() {return m_bData;}

	sint64		GetInt(int Index);
	string		GetString(int Index);
	wstring		GetWString(int Index);
	size_t		GetBlobSize(int Index);
	byte*		GetBlobBytes(int Index);

protected:
	struct sqlite3_stmt*	m_pQuery;
	bool					m_bData;
};
