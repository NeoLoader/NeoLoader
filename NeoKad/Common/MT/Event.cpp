#include "GlobalHeader.h"
#include "Event.h"

CEvent::CEvent()
{
#ifndef WIN32
	pthread_mutex_init(&m_Lock, NULL);
	pthread_cond_init(&m_Cond, NULL);
#else
	m_Cond = CreateEvent(NULL, false, false, NULL);
#endif
}

CEvent::~CEvent()
{
#ifndef WIN32
	pthread_mutex_destroy(&m_Lock);
	pthread_cond_destroy(&m_Cond);
#else
	CloseHandle(m_Cond);
#endif
}

bool CEvent::Lock(uint32 Wait)
{
#ifndef WIN32
	timeval now;
	timespec timeout;
	gettimeofday(&now, 0);
	timeout.tv_sec = now.tv_sec + 1;
	timeout.tv_nsec = now.tv_usec * Wait;

	return pthread_cond_timedwait(&m_Cond, &m_Lock, &timeout) == 0;
#else
	return WaitForSingleObject(m_Cond, Wait) == WAIT_TIMEOUT;
#endif
}

void CEvent::Lock()
{
#ifndef WIN32
	pthread_cond_wait(&m_Cond, &m_Lock);
#else
	WaitForSingleObject(m_Cond, INFINITE);
#endif
}

void CEvent::Release()
{
#ifndef WIN32
	pthread_cond_signal(&m_Cond);
#else
	SetEvent(m_Cond);
#endif
}
