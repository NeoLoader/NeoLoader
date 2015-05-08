#include "GlobalHeader.h"
#include "ThreadEx.h"

/**********************************************************************************************
* QThreadEx
*/

//QMutex QThreadEx::m_PoolMutex;
//QList<QThreadEx*> QThreadEx::m_PoolList;


QThreadEx::QThreadEx(QObject* qObject)
 : QThread(qObject)//, m_Mutex(QMutex::Recursive) 
{
	//QMutexLocker Locker(&m_PoolMutex);
	//m_PoolList.append(this);
}

QThreadEx::~QThreadEx()
{
	//QMutexLocker Locker(&m_PoolMutex);
	//m_PoolList.removeOne(this);
}

void QThreadEx::AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line)
{
	if(QObjectEx* pParentEx = qobject_cast<QObjectEx*>(parent()))
		pParentEx->AddLogLine(uStamp, uFlag, Line);
	else
		::LogLine(uFlag, Line);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// 

/*bool g_bTrapSet = false; 

int ExceptionFilter()
{
	//return 1; //EXCEPTION_EXECUTE_HANDLER; // this one would cause entering the __except block
	// Note: first we have to say continue and on the second occurence we have to say search,
	//			this way we can forward the exception from where it occured to the VS debuger,
	//			bypassing the try{notify()}catch(...){} from qt
	if(g_bTrapSet == false)
	{
		g_bTrapSet = true;
		return -1; //EXCEPTION_CONTINUE_EXECUTION;
	}
	g_bTrapSet = false;
	return 0; //EXCEPTION_CONTINUE_SEARCH;
}*/
