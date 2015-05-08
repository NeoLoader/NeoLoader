#pragma once

class CVariant;
class CBuffer;

string ToPlatformNotation(const wstring& Path);
bool RemoveFile(const wstring& Path);
bool RenameFile(const wstring& OldPath, const wstring& NewPath);
bool WriteFile(const wstring& Path, const wstring& Data);
bool ReadFile(const wstring& Path, wstring& Data);
bool WriteFile(const wstring& Path, const string& Data);
bool ReadFile(const wstring& Path, string& Data);
bool WriteFile(const wstring& Path, const CVariant& Data);
bool ReadFile(const wstring& Path, CVariant& Data);
bool WriteFile(const wstring& Path, uint64 Offset, const CBuffer& Data);
bool ReadFile(const wstring& Path, uint64 Offset, CBuffer& Data);
bool ListDir(const wstring& Path, vector <wstring>& Entries, const wchar_t* Filter = NULL);
long GetFileSize(const wstring& Path);