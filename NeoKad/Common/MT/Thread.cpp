#include "GlobalHeader.h"
#include "Thread.h"

#ifndef WIN32
   #include <unistd.h>
#endif

IMPLEMENT_OBJECT(CThread, CObject)

CThread::CThread(void(*func)(const void*), const void* param)
{
	m_bRunning = false;

	m_Func = func;
	m_Param = param;

#ifndef WIN32
	m_Thread = 0;
#else
	m_Thread = NULL;
	m_Exit = NULL;
#endif
}

CThread::~CThread()
{
	if(m_bRunning)
		Stop();
}

void CThread::Start()
{
	m_bRunning = true;

#ifndef WIN32
	if(0 != pthread_create(&m_Thread, NULL, run, this))
		m_Thread = 0;
#else
	m_Exit = CreateEvent(NULL, false, false, NULL); 
	m_Thread = CreateThread(NULL, 0, run, this, 0, NULL);
#endif
}

void CThread::Stop()
{
	m_bRunning = false;

#ifndef WIN32
	if (0 != m_Thread)
		pthread_join(m_Thread, NULL);
#else
	if (NULL != m_Thread)
		WaitForSingleObject(m_Exit, INFINITE);
	CloseHandle(m_Thread);
	m_Thread = NULL;
	CloseHandle(m_Exit);
	m_Exit = NULL;
#endif
}

void CThread::Sleep(int sleep)
{
#ifndef WIN32
	usleep(sleep);
#else
	::Sleep(sleep);
#endif
}

#ifndef WIN32
void* CThread::run(void* thread)
{
	CThread* Thread = ((CThread*)thread); 
	Thread->m_Func(Thread->m_Param);
	return 0;
}
#else
DWORD CThread::run(LPVOID thread)
{
	CThread* Thread = ((CThread*)thread); 
	Thread->m_Func(Thread->m_Param);
	SetEvent(Thread->m_Exit); 
	return NULL;
}
#endif
