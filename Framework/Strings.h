#pragma once

#include "./NeoHelper/neohelper_global.h"

QString NEOHELPER_EXPORT UnEscape(QString Text);

//////////////////////////////////////////////////////////////
// Number string Conversion
//

wstring NEOHELPER_EXPORT int2wstring(int Int);
int NEOHELPER_EXPORT wstring2int(const wstring& Str);

string NEOHELPER_EXPORT int2string(int Int);
int NEOHELPER_EXPORT string2int(const string& Str);

wstring NEOHELPER_EXPORT double2wstring(double Double);
double NEOHELPER_EXPORT wstring2double(const wstring& Str);

//////////////////////////////////////////////////////////////
// String Comparations
//

bool NEOHELPER_EXPORT CompareStr(const wstring &StrL,const wstring &StrR);

bool NEOHELPER_EXPORT CompareStrs(const wstring &StrL,const wstring &StrR, wchar_t SepC = ' ', bool bTrim = false);

int NEOHELPER_EXPORT compareex(const wchar_t *StrL, size_t OffL, size_t CountL, const wchar_t* StrR, size_t OffR, size_t CountR);

template <typename T>
const T* wildcmpex(const T* Wild, const T* Str)
{
	const T *cp = NULL, *mp = NULL;

	while ((*Str) && (*Wild != '*')) 
	{
		if ((*Wild != *Str) && (*Wild != '?')) 
			return NULL;
		Wild++;
		Str++;
	}

	while (*Str) 
	{
		if (*Wild == '*') 
		{
			if (!*++Wild) 
				return Str;
			mp = Wild;
			cp = Str+1;
		} 
		else if ((*Wild == *Str) || (*Wild == '?')) 
		{
			Wild++;
			Str++;
		}
		else 
		{
			Wild = mp;
			Str = cp++;
		}
	}

	while (*Wild == '*') 
		Wild++;
	return *Wild ? NULL : Str;
}

//////////////////////////////////////////////////////////////
// String Operations
//

__inline wstring s2w(const string& s) {return wstring(s.begin(),s.end());}
__inline string w2s(const wstring& w) {return string(w.begin(),w.end());}

wstring NEOHELPER_EXPORT MkLower(wstring Str);
wstring NEOHELPER_EXPORT MkUpper(wstring Str);

template <typename T>
T Trimmx(const T& String, const T& Blank)
{
	typename T::size_type Start = String.find_first_not_of(Blank);
	typename T::size_type End = String.find_last_not_of(Blank)+1;
	if(Start == wstring::npos)
		return T();
	return  String.substr(Start, End - Start);
}

__inline wstring Trimm(const wstring& String)	{return Trimmx(String, wstring(L" \r\n\t"));}
__inline string Trimm(const string& String)		{return Trimmx(String, string(" \r\n\t"));}

template <typename T>
pair<T,T> Split2x(const T& String, T Separator, bool Back)
{
	typename T::size_type Sep = Back ? String.rfind(Separator) : String.find(Separator);
	if(Sep != T::npos)
		return pair<T,T>(String.substr(0, Sep), String.substr(Sep+Separator.length()));
	return pair<T,T>(String, T());
}

__inline pair<string,string> Split2(const string& String, string Separator = ",", bool Back = false) {return Split2x(String, Separator, Back);}
__inline pair<wstring,wstring> Split2(const wstring& String, wstring Separator = L",", bool Back = false) {return Split2x(String, Separator, Back);}

template <typename T>
vector<T> SplitStrx(const T& String, const T& Separator, bool bKeepEmpty = true, bool bMulti = false)
{
	vector<T> StringList;
	typename T::size_type Pos = 0;
	for(;;)
	{
		typename T::size_type Sep = bMulti ? String.find_first_of(Separator,Pos) : String.find(Separator,Pos);
		if(Sep != T::npos)
		{
			if(bKeepEmpty || Sep-Pos > 0)
				StringList.push_back(String.substr(Pos,Sep-Pos));
			Pos = Sep+1;
		}
		else
		{
			if(bKeepEmpty || Pos < String.length())
				StringList.push_back(String.substr(Pos));
			break;
		}
	}
	return StringList;
}

__inline vector<string> SplitStr(const string& String, string Separator = ",", bool bKeepEmpty = true, bool bMulti = false) {return SplitStrx(String, Separator, bKeepEmpty, bMulti);}
__inline vector<wstring> SplitStr(const wstring& String, wstring Separator = L",", bool bKeepEmpty = true, bool bMulti = false) {return SplitStrx(String, Separator, bKeepEmpty, bMulti);}

template <typename T>
T JoinStrx(const vector<T>& StringList, const T& Separator)
{
	T String;
    for(typename vector<T>::const_iterator I = StringList.begin(); I != StringList.end(); I++)
	{
		if(!String.empty())
			String += Separator;
		String += *I;
	}
	return String;
}

__inline string JoinStr(const vector<string>& String, string Separator = ",") {return JoinStrx(String, Separator);}
__inline wstring JoinStr(const vector<wstring>& String, wstring Separator = L",") {return JoinStrx(String, Separator);}

wstring NEOHELPER_EXPORT SubStrAt(const wstring& String, const wstring& Separator, int Index);
wstring::size_type NEOHELPER_EXPORT FindNth(const wstring& String, const wstring& Separator, int Index);
wstring::size_type NEOHELPER_EXPORT FindNthR(const wstring& String, const wstring& Separator, int Index);
int NEOHELPER_EXPORT CountSep(const wstring& String, const wstring& Separator);

wstring::size_type NEOHELPER_EXPORT FindStr(const wstring& str, const wstring& find, wstring::size_type Off = 0);
wstring::size_type NEOHELPER_EXPORT RFindStr(const wstring& str, const wstring& find, wstring::size_type Off = wstring::npos);

__inline bool IsWhiteSpace(char Char) {return Char == ' ' || Char == '\t' || Char == '\r' || Char == '\n';}

template <typename T, typename S>
void toHexadecimal(T val, S *buf)
{
	int i;
    for (i = 0; i < sizeof(T) * 2; ++i) 
	{
        buf[i] = (val >> (4 * (sizeof(T) * 2 - 1 - i))) & 0xf;
        if (buf[i] < 10)
            buf[i] += '0';
        else
            buf[i] += 'A' - 10;
    }
    buf[i] = 0;
}

// templated version of my_equal so it could work with both char and wchar_t
template<typename charT>
struct my_equal {
    my_equal( const std::locale& loc ) : loc_(loc) {}
    bool operator()(charT ch1, charT ch2) {
        return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
    }
private:
    const std::locale& loc_;
};

// find substring (case insensitive)
template<typename T>
int ci_find( const T& str1, const T& str2, const std::locale& loc = std::locale() )
{
	typename T::const_iterator it = std::search( str1.begin(), str1.end(),
        str2.begin(), str2.end(), my_equal<typename T::value_type>(loc) );
    if ( it != str1.end() ) return it - str1.begin();
    else return -1; // not found
}

int NEOHELPER_EXPORT wmemcmpex(const wchar_t *Str1, const wchar_t *Str2, size_t size);

//////////////////////////////////////////////////////////////
// Manual conversion
//

NEOHELPER_EXPORT wchar_t* UTF8toWCHAR(const char *str);
NEOHELPER_EXPORT bool verify_encoding(std::string& target, bool fix_paths = false);
