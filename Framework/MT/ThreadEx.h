#pragma once

#include "../NeoHelper/neohelper_global.h"
//#include <QApplication>
#include <QThread>
#include "../ObjectEx.h"

/**********************************************************************************************
* QThreadEx
* This class provides a Thread that have an own Mutex, that controlls slot execution
* QApplicationEx::notify will lock the mutex on any slot event
*/

class NEOHELPER_EXPORT QThreadEx: public QThread
{
	Q_OBJECT

public:
	QThreadEx(QObject* qObject = NULL);
	~QThreadEx();

	//void run()

	static void usleep(long iSleepTime)	{QThread::usleep(iSleepTime);}
	static void sleep(long iSleepTime)	{QThread::sleep(iSleepTime);}
	static void msleep(long iSleepTime)	{QThread::msleep(iSleepTime);}

	//QMutex*							GetMutex()					{return &m_Mutex;}

	//static const QList<QThreadEx*>&	AcquirePoolList()			{m_PoolMutex.lock(); return m_PoolList;}
	//static void						ReleasePoolList()			{m_PoolMutex.unlock();}

	virtual void					LogLine(uint32 uFlag, const CLogMsg& Line)	{AddLogLine(GetTime(), uFlag, Line);}

protected:
	friend class QObjectEx;
	virtual void					AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line);

	//QMutex							m_Mutex;

	//static QMutex					m_PoolMutex;
	//static QList<QThreadEx*>		m_PoolList;
};

/////////////

/*int NEOHELPER_EXPORT ExceptionFilter();

class QApplicationEx: public QApplication
{
public:
	QApplicationEx(int &argc, char **argv) : QApplication(argc, argv) {}
	bool notify(QObject *Object, QEvent *Event)
	{
		bool bRet = false;

		//QThreadEx* pThread = qobject_cast<QThreadEx*>(QThread::currentThread());
		//if(pThread)
		//	pThread->GetMutex()->lock();
		
		bRet = QApplication::notify(Object, Event);
		//bRet = TryExcept(this, Object, Event);

		//if(pThread)
		//	pThread->GetMutex()->unlock();

		return bRet;
	}

private:
	//bool QTnotify(QObject *Object, QEvent *Event) {return QApplication::notify(Object, Event);}
	//static bool TryExcept(QApplicationEx* pApplication, QObject *Object, QEvent *Event)
	//{
	//	__try{
	//		return pApplication->QTnotify(Object, Event);
	//	}__except(ExceptionFilter()){}
	//	return false;
	//}
};
*/