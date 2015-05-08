#include "GlobalHeader.h"
#include "Functions.h"
#include "ObjectEx.h"
#include <QDir>
#include <QElapsedTimer>
#include <../../crypto++/dll.h>

#ifndef WIN32 // vswprintf
#include <stdio.h>
#include <stdarg.h>
#endif

CLogMsg::CLogMsg(const QString& Line)
{
	m_Msg = "Default";
	m_Str = "%1";
	m_Args.append(Line);
}

CLogMsg::CLogMsg(const QString& Msg, const QString& Str)
{
	m_Msg = Msg;
	m_Str = Str;
}

CLogMsg::CLogMsg(const QVariant& vMsg)
{
	QVariantMap Msg = vMsg.toMap();
	m_Msg = Msg["Msg"].toString();
	m_Str = Msg["Str"].toString();
	m_Args = Msg["Args"].toStringList();
	m_Prefix = Msg["Fix"].toString();
}

CLogMsg& CLogMsg::arg(QVariant arg)
{
	m_Args.append(arg.toString());
	return *this;
}

CLogMsg::operator QVariant() const	
{
	QVariantMap Msg;
	Msg["Msg"] = m_Msg;
	Msg["Str"] = m_Str;
	Msg["Args"] = m_Args;
	Msg["Fix"] = m_Prefix;
	return Msg;
}

QString CLogMsg::Print() const
{
	QString Msg = m_Str;
	foreach(const QString& Arg, m_Args)
		Msg = Msg.arg(Arg);
	if(!m_Prefix.isEmpty())
		Msg.prepend(m_Prefix + " :: ");
	return Msg;
}

void LogLine(uint32 uFlag, const CLogMsg& Line)
{
	CLogger::Instance()->AddLogLine(GetTime(), uFlag, Line);
}

#ifndef WIN32
int vswprintf_l(wchar_t * _String, size_t _Count, const wchar_t * _Format, va_list _Ap)
{
	wchar_t _Format_l[1025];
	ASSERT(wcslen(_Format) < 1024);
	wcscpy(_Format_l, _Format);

	for(int i=0; i<wcslen(_Format_l); i++)
	{
		if(_Format_l[i] == L'%')
		{
			switch(_Format_l[i+1])
			{
				case L's':	_Format_l[i+1] = 'S'; break;
				case L'S':	_Format_l[i+1] = 's'; break;
			}
		}
	}

	return vswprintf(_String, _Count, _Format_l, _Ap);
}
#endif

void LogLine(uint32 uFlag, const wchar_t* sLine, ...)
{
	ASSERT(sLine != NULL);

	const size_t bufferSize = 10241;
	wchar_t bufferline[bufferSize];

	va_list argptr;
	va_start(argptr, sLine);
#ifndef WIN32
	if (vswprintf_l(bufferline, bufferSize, sLine, argptr) == -1)
#else
	if (vswprintf(bufferline, bufferSize, sLine, argptr) == -1)
#endif
		bufferline[bufferSize - 1] = L'\0';
	va_end(argptr);

	QString sBufferLine = QString::fromWCharArray(bufferline);
	LogLine(uFlag, sBufferLine);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Time Functions
// 

time_t GetTime()
{
	QDateTime dateTime = QDateTime::currentDateTime();
	time_t time = dateTime.toTime_t(); // returns time in seconds (since 1970-01-01T00:00:00) in UTC !
	return time;
}

struct SCurTick
{
	SCurTick()	{Timer.start();}
	uint64 Get(){return Timer.elapsed();}
	QElapsedTimer Timer;
}	g_CurTick;

uint64 GetCurTick()
{
	return g_CurTick.Get();
}

UINT MkTick(UINT& uCounter)
{
	uCounter++;

	UINT Tick = E100PerSec;
	if (uCounter % 10 == 0) // every 100 ms
		Tick |= E10PerSec;
	if (uCounter % 50 == 0) // every 500 ms
		Tick |= E2PerSec;
	if (uCounter % 25 == 0) // every 250 ms
		Tick |= E4PerSec;
	if (uCounter % 100 == 0) // every 1 s
		Tick |= EPerSec;
	if (uCounter % 500 == 0) // every 5 s
		Tick |= EPer5Sec;
	if (uCounter % 1000 == 0) // every 10 s
		Tick |= EPer10Sec;
	if (uCounter % 10000 == 0) // every 100 s
	{
		Tick |= EPer100Sec;
		uCounter = 0;
	}

	return Tick;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Other Functions
// 

uint64 GetRand64()
{
	using namespace CryptoPP;

	uint64 Rand64;	
	AutoSeededRandomPool rng;
	rng.GenerateBlock((byte*)&Rand64, sizeof(uint64));
	return Rand64;
}

QString GetRand64Str(bool forUrl)
{
	uint64 Rand64 = GetRand64();
	QString sRand64 = QByteArray((char*)&Rand64,sizeof(uint64)).toBase64();
	if(forUrl)
		sRand64.replace("+","-").replace("/","_");
	return sRand64.replace("=","");
}

StrPair Split2(const QString& String, QString Separator, bool Back)
{
	int Sep = Back ? String.lastIndexOf(Separator) : String.indexOf(Separator);
	if(Sep != -1)
		return qMakePair(String.left(Sep).trimmed(), String.mid(Sep+Separator.length()).trimmed());
	return qMakePair(String.trimmed(), QString());
}

QStringList SplitStr(const QString& String, QString Separator)
{
	QStringList List = String.split(Separator);
	for(int i=0; i < List.count(); i++)
	{
		List[i] = List[i].trimmed();
		if(List[i].isEmpty())
			List.removeAt(i--);
	}
	return List;
}