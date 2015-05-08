#pragma once

#include "./NeoHelper/neohelper_global.h"

#include <QObject>
#include "Functions.h"

/**********************************************************************************************
* QObjectEx, this objects are tracked and provide log heirarchy, but dont retain own logs
*/

#include "Tracker.h"

//class NEOHELPER_EXPORT QObjectEx: public QTracked<QObject>
class NEOHELPER_EXPORT QObjectEx: public QObject
{
	Q_OBJECT

public:
	//QObjectEx(QObject* qObject = NULL) : QTracked<QObject>(qObject)	{}
	QObjectEx(QObject* qObject = NULL) : QObject(qObject)	{}
	~QObjectEx() {}

	virtual void							LogLine(uint32 uFlag, const CLogMsg& Line)	{AddLogLine(GetTime(), uFlag, Line);}

protected:
	friend class QThreadEx;

	virtual void							AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line);
};

/**********************************************************************************************
* 
*/

class NEOHELPER_EXPORT CLog
{
public:
	CLog() /*: m_LogMutex(QMutex::Recursive)*/ {m_LogLimit = 1000;}

	struct SLine
	{
		SLine(time_t stamp, uint32 flag, const CLogMsg& line)
		{
			uStamp = stamp;
			uFlag = flag;
			Line = line;
			static uint64 ID = 0;
			uID = ++ID;
		}
		inline bool operator==(const SLine &Other) {return uID == Other.uID;}

		time_t	uStamp;
		uint32	uFlag;
		CLogMsg Line;
		uint64	uID;
	};

	virtual QList<SLine>					GetLog();
	virtual QList<SLine>					GetLog(uint64 Mark);
	virtual void							SetLogLimit(int Limit) {m_LogLimit = Limit;}

	static	bool							FlterLog(uint32 uMask, uint32 uFlag);

protected:
	virtual void							AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line);

	QMutex									m_LogMutex;
	QList<SLine>							m_LogLines;
	int										m_LogLimit;
};

template <class OBJ>
class CLogTmpl: public OBJ, public CLog
{
public:
	CLogTmpl(QObject* qObject = NULL) : OBJ(qObject) {}

	virtual void							LogLine(uint32 uFlag, const CLogMsg& Line)	{AddLogLine(GetTime(), uFlag, Line);}

protected:
	virtual void							AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line) 
	{
		OBJ::AddLogLine(uStamp, uFlag, Line);
		CLog::AddLogLine(uStamp, uFlag, Line);
	}
};

/**********************************************************************************************
*
*/

class NEOHELPER_EXPORT CLogger: public CLog
{
public:
	CLogger(const QString& Name);
	~CLogger();

	virtual void							AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line);

	virtual void							Process();

	virtual void							SetLogPath(const QString& LogPath);

	static CLogger*							Instance() {return m_Instance;}

protected:
	virtual void							CreateLogFile();
	virtual void							CloseLogFile();

	QString									m_Name;
	QFile*									m_LogFile;
	QTextStream*							m_LogStream;
	QDate									m_LastDate;
	QString									m_LogPath;

	static CLogger*		m_Instance;
};

template <class OBJ>
class CLoggerTmpl: public OBJ, public CLogger
{
public:
	CLoggerTmpl(const QString& Name, QObject* qObject = NULL)
	 :OBJ(qObject), CLogger(Name) {m_pLogInterceptor = NULL; m_uInterceptorMask = 0;}

	void				SetLogInterceptor(QList<CLog::SLine>* pLog, uint32 uMask = 0)
	{
		QMutexLocker Locker(&m_InterceptorMutex); 
		m_pLogInterceptor = pLog;
		m_uInterceptorMask = uMask;
	}

protected:
	virtual void							AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line)
	{
		CLogger::AddLogLine(uStamp, uFlag, Line);

		QMutexLocker Locker(&m_InterceptorMutex); 
		if(m_pLogInterceptor && FlterLog(m_uInterceptorMask, uFlag))
			m_pLogInterceptor->append(SLine(uStamp, uFlag, Line));
	}

	QMutex				m_InterceptorMutex;
	QList<CLog::SLine>* m_pLogInterceptor;
	uint32				m_uInterceptorMask;
};
