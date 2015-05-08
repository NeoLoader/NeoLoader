#pragma once

#include "./NeoHelper/neohelper_global.h"

#include <QStringList>
#include <QPair>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include "Types.h"

#define LOG_NOTE	0	// Black
#define LOG_ERROR	1	// Red
#define LOG_WARNING	2	// Yellow
#define LOG_SUCCESS	3	// Green
#define LOG_INFO	4	// Blue
//#define LOG_		5	//
//#define LOG_		6	//
#define LOG_MASK	7

#define LOG_NOTIFY	8	// pop up the error in the web GUI
//#define LOG_ALERT	16	// pop up that error on the desktop
//#define LOG_		32	// 
//#define LOG_		64	// 
#define LOG_DEBUG	128 // dont show the info in the normal log

#define LOG_MOD_MASK	0xFF00
#define LOG_MOD(x)		((x << 8) & 0x7F00)		// internal module
#define LOG_EXT			0x8000					// extern flag
#define LOG_XMOD(x)		(LOG_MOD(x) | LOG_EXT)	// eXternal module

class NEOHELPER_EXPORT CLogMsg
{
public:
	CLogMsg(){}
	CLogMsg(const QString& Line);
	CLogMsg(const QString& Msg, const QString& Str);
	CLogMsg(const QVariant& vMsg);
	CLogMsg& arg(QVariant arg);
	operator QVariant() const;
	void Prefix(const QString& str) const	{m_Prefix = str;}
	QString Print() const;
	void AddMark(uint64 Mark) const			{m_Marks.insert(Mark);}
	bool ChkMark(uint64 Mark) const			{return m_Marks.contains(Mark);}
protected:
	QString					m_Msg;
	mutable QString			m_Prefix;
	mutable QSet<uint64>	m_Marks;
	QString					m_Str;
	QStringList				m_Args;
};

inline CLogMsg	trx(const char* m, const char* s)			{return CLogMsg(m, s);}

void NEOHELPER_EXPORT LogLine(uint32 uFlag, const CLogMsg& Line);
void NEOHELPER_EXPORT LogLine(uint32 uFlag, const wchar_t* sLine, ...);

#ifndef WIN32
int vswprintf_l(wchar_t * _String, size_t _Count, const wchar_t * _Format, va_list _Ap);
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Time Functions
// 

NEOHELPER_EXPORT time_t GetTime();
NEOHELPER_EXPORT uint64 GetCurTick();

enum EEProcessTicks
{
	EPer100Sec		= 128,		// 1000 0000
	EPer10Sec		= 64,		// 0100 0000
	EPer5Sec		= 32,		// 0010 0000
	EPerSec			= 16,		// 0001 0000
	E2PerSec		= 8,		// 0000 1000
	E4PerSec		= 4,		// 0000 0100
	E10PerSec		= 2,		// 0000 0010
	E100PerSec		= 1,		// 0000 0001
};

UINT NEOHELPER_EXPORT MkTick(UINT& uCounter);

//////////////////////////////////////////////////////////////////////////////////////////
// Other Functions
// 

NEOHELPER_EXPORT uint64 GetRand64();
NEOHELPER_EXPORT QString GetRand64Str(bool forUrl = true);

inline int		GetRandomInt(int iMin, int iMax)			{return qrand() % ((iMax + 1) - iMin) + iMin;}

typedef QPair<QString,QString> StrPair;
NEOHELPER_EXPORT StrPair Split2(const QString& String, QString Separator = "=", bool Back = false);
NEOHELPER_EXPORT QStringList SplitStr(const QString& String, QString Separator);