#include "GlobalHeader.h"
#include "ObjectEx.h"
#include "MT/ThreadEx.h"
#include "OtherFunctions.h"

/**********************************************************************************************
* QObjectEx
*/

void QObjectEx::AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line)
{
	if(QObjectEx* pParentEx = qobject_cast<QObjectEx*>(parent()))
		pParentEx->AddLogLine(uStamp, uFlag, Line);
	else if(QThreadEx* pThreadEx = qobject_cast<QThreadEx*>(thread()))
		pThreadEx->AddLogLine(uStamp, uFlag, Line);
	else
		::LogLine(uFlag, Line);
}

/**********************************************************************************************
* CLog
*/

void CLog::AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line) 
{
	QMutexLocker Locker(&m_LogMutex); 
	m_LogLines.append(SLine(uStamp, uFlag, Line));

	while(m_LogLimit < m_LogLines.count())
		m_LogLines.removeFirst();
}

QList<CLog::SLine> CLog::GetLog() 
{
	QMutexLocker Locker(&m_LogMutex); 
	return m_LogLines;
}

QList<CLog::SLine> CLog::GetLog(uint64 Mark)
{
	QList<CLog::SLine> LogLines;
	QMutexLocker Locker(&m_LogMutex); 
	for(QList<CLog::SLine>::iterator I = m_LogLines.begin(); I != m_LogLines.end(); I++)
	{
		if(I->Line.ChkMark(Mark))
			LogLines.append(*I);
	}
	return LogLines;
}

bool CLog::FlterLog(uint32 uMask, uint32 uFlag)
{
	if(uint32 uTemp = (uMask & LOG_MASK))
	{
		if(uTemp != (uFlag & LOG_MASK))
			return false;
	}
	if(uint32 uTemp = (uMask & 0xF8))
	{
		if((uFlag & uTemp) == 0)
			return false;
	}
	if(uint32 uTemp = (uMask & LOG_MOD_MASK))
	{
		if(uTemp != (uFlag & LOG_MOD_MASK))
			return false;
	}
	return true;
}

/**********************************************************************************************
* CLogger
*/

CLogger* CLogger::m_Instance = NULL;

CLogger::CLogger(const QString& Name)
{
	m_Name = Name;

	m_LastDate = QDateTime::currentDateTime().date();

	m_LogFile = NULL;
	m_LogStream = NULL;

	ASSERT(m_Instance == NULL);
	m_Instance = this;
}

CLogger::~CLogger()
{
	CloseLogFile();

	m_Instance = NULL;
}

void CLogger::Process()
{
	QMutexLocker Locker(&m_LogMutex);
	QDateTime now = QDateTime::currentDateTime();
	if(m_LastDate != now.date())
	{
		// and once a day we create a new log file
		m_LastDate = now.date();
		CreateLogFile();
	}
}

void CLogger::SetLogPath(const QString& LogPath)
{
	m_LogPath = LogPath;
	CreateDir(m_LogPath);
	CreateLogFile();
}

void CLogger::CreateLogFile()
{
	CloseLogFile();

	if (!m_LogPath.isEmpty())
	{
		m_LogFile = new QFile(QString(m_LogPath).append("/%1Log_%2.txt").arg(m_Name).arg(m_LastDate.toString(Qt::ISODate)));
		m_LogFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
		m_LogStream = new QTextStream(m_LogFile);
	}
}

void CLogger::CloseLogFile()
{
	if (m_LogFile)
	{
		m_LogFile->close();
		delete m_LogStream;
		m_LogStream = NULL;
		delete m_LogFile;
		m_LogFile = NULL;
	}
}

void CLogger::AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line)
{
	CLog::AddLogLine(uStamp, uFlag, Line);

	if((uFlag & LOG_EXT) != 0)
		return;

#ifndef win32
    QDateTime then = QDateTime::fromTime_t(uStamp).toLocalTime();
    qDebug() << then.toString().append(": ").append(Line.Print());
#endif

	QMutexLocker Locker(&m_LogMutex);
	if (m_LogStream)
	{
		QDateTime then = QDateTime::fromTime_t(uStamp).toLocalTime();
		*m_LogStream << then.toString().append(": ").append(Line.Print());
		endl(*m_LogStream);
	}
}
