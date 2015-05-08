#include "GlobalHeader.h"
#include "FileIO.h"
#include "../../Framework/Buffer.h"
#include "Variant.h"
#include "../../Framework/Strings.h"
#include "../../Framework/Exception.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef WIN32
#include "dirent.h"
#else
#include <dirent.h>
#endif

string ToPlatformNotation(const wstring& Path)
{
	string UPath;
	WStrToUtf8(UPath, Path);
	for(;;)
	{
#ifdef WIN32
		wstring::size_type Pos = UPath.find("/");
		if(Pos == wstring::npos)
			break;
		UPath.replace(Pos,1,"\\");
#else
		wstring::size_type Pos = UPath.find("\\");
		if(Pos == wstring::npos)
			break;
		UPath.replace(Pos,1,"/");
#endif
	}
	return UPath;
}

wstring ToApplicationNotation(const string& UPath)
{
	wstring Path;
	Utf8ToWStr(Path, UPath);
	for(;;)
	{
		wstring::size_type Pos = Path.find(L"\\");
		if(Pos == wstring::npos)
			break;
		Path.replace(Pos,1,L"/");
	}
	return Path;
}

bool RemoveFile(const wstring& Path)
{
	return remove(ToPlatformNotation(Path).c_str()) == 0;
}

bool RenameFile(const wstring& OldPath, const wstring& NewPath)
{
	return rename(ToPlatformNotation(OldPath).c_str(), ToPlatformNotation(NewPath).c_str()) == 0;
}

bool WriteFile(const wstring& Path, const wstring& Data)
{
	FILE* f = fopen(ToPlatformNotation(Path).c_str(), "wb");
	if(!f)
		return false;

	string UData;
	WStrToUtf8(UData, Data);
	fwrite(UData.data(), sizeof(char), UData.size(), f);

	fclose(f);
	return true;
}

bool ReadFile(const wstring& Path, wstring& Data)
{
	FILE* f = fopen(ToPlatformNotation(Path).c_str(), "rb");
	if(!f)
		return false;
	
	fseek (f, 0, SEEK_END);
	string::size_type Size = ftell(f);
	fseek (f, 0, SEEK_SET);

	string UData(Size, '\0');
	fread((char*)UData.data(), sizeof(char), UData.size(), f);
	Utf8ToWStr(Data, UData);

	fclose(f);
	return true;
}

bool WriteFile(const wstring& Path, const string& Data)
{
	FILE* f = fopen(ToPlatformNotation(Path).c_str(), "wb");
	if(!f)
		return false;

	fwrite(Data.data(), sizeof(char), Data.size(), f);

	fclose(f);
	return true;
}

bool ReadFile(const wstring& Path, string& Data)
{
	FILE* f = fopen(ToPlatformNotation(Path).c_str(), "rb");
	if(!f)
		return false;
	
	fseek (f, 0, SEEK_END);
	string::size_type Size = ftell(f);
	fseek (f, 0, SEEK_SET);

	Data.resize(Size, '\0');
	fread((char*)Data.data(), sizeof(char), Data.size(), f);

	fclose(f);
	return true;
}

bool WriteFile(const wstring& Path, const CVariant& Data)
{
	FILE* f = fopen(ToPlatformNotation(Path).c_str(), "wb");
	if(!f)
		return false;

	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	fwrite(Buffer.GetBuffer(), sizeof(byte), Buffer.GetSize(), f);

	fclose(f);
	return true;
}

bool ReadFile(const wstring& Path, CVariant& Data)
{
	FILE* f = fopen(ToPlatformNotation(Path).c_str(), "rb");
	if(!f)
		return false;
	
	fseek (f, 0, SEEK_END);
	size_t Size = ftell(f);
	fseek (f, 0, SEEK_SET);

	CBuffer Buffer(Size,true);
	fread(Buffer.GetBuffer(), sizeof(byte), Buffer.GetSize(), f);
			
	fclose(f);

	try
	{
		Data.FromPacket(&Buffer);
	}
	catch(const CException&)
	{
		return false;
	}
	return true;
}

bool WriteFile(const wstring& Path, uint64 Offset, const CBuffer& Data)
{
	FILE* f = fopen(ToPlatformNotation(Path).c_str(), "wb");
	if(!f)
		return false;

	fseek (f, Offset, SEEK_SET);
	fwrite(Data.GetBuffer(), sizeof(byte), Data.GetSize(), f);

	fclose(f);
	return true;
}

bool ReadFile(const wstring& Path, uint64 Offset, CBuffer& Data)
{
	FILE* f = fopen(ToPlatformNotation(Path).c_str(), "rb");
	if(!f)
		return false;

	size_t ToGo = Data.GetSize();
	if(ToGo == 0)
	{
		fseek (f, 0, SEEK_END);
		ToGo = ftell(f);
		fseek (f, 0, SEEK_SET);
		Data.SetSize(ToGo, true);
		if(Data.GetBuffer() == NULL)
			return false; //malloc failed file to big to fit into memmory
	}

	fseek (f, Offset, SEEK_SET);
	fread(Data.GetBuffer(), sizeof(byte), ToGo, f);

	fclose(f);
	return true;
}

bool ListDir(const wstring& Path, vector <wstring>& Entries, const wchar_t* Filter)
{
	ASSERT(Path.back() == L'/');

	DIR* d = opendir (ToPlatformNotation(Path).c_str());
	if (d == NULL)
		return false;

	dirent* e;
	while ((e = readdir(d)) != NULL) 
	{
		wstring Name = ToApplicationNotation(e->d_name);
		if(Filter && !wildcmpex(Filter,Name.c_str()))
			continue;

		switch (e->d_type) 
		{
			case DT_DIR:
				if(Name.compare(L"..") == 0 || Name.compare(L".") == 0)
					continue;
				Entries.push_back(Path + Name + L"/");
				break;
			default:
				Entries.push_back(Path + Name);
		}
	}
	closedir (d);
	return true;
}

long GetFileSize(const wstring& Path)
{
	FILE* f = fopen(ToPlatformNotation(Path).c_str(), "rb");
	if(!f)
		return 0;
	
	fseek (f, 0, SEEK_END);
	long Size = ftell(f);
	fseek (f, 0, SEEK_SET);

	fclose(f);
	return Size;
}