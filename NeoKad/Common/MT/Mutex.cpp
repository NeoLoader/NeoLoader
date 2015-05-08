#include "GlobalHeader.h"
#include "Mutex.h"

CMutex::CMutex()
{
#ifndef WIN32
	pthread_mutex_init(&m_Lock, NULL);
#else
	m_Lock = CreateMutex(NULL, false, NULL);
#endif
}

CMutex::~CMutex()
{
#ifndef WIN32
	pthread_mutex_destroy(&m_Lock);
#else
	CloseHandle(m_Lock);
#endif
}

#ifdef __APPLE__
#include<errno.h>

int pthread_mutex_timedlock (pthread_mutex_t *mutex, const struct timespec *timeout)
{
 struct timeval timenow;
 struct timespec sleepytime;

 /* This is just to avoid a completely busy wait */
 sleepytime.tv_sec = 0;
 sleepytime.tv_nsec = 10000000; /* 10ms */

 while (pthread_mutex_trylock (mutex) == EBUSY) {
  gettimeofday (&timenow, NULL);

  if (timenow.tv_sec >= timeout->tv_sec &&
      (timenow.tv_usec * 1000) >= timeout->tv_nsec) {
   return ETIMEDOUT;
  }

  nanosleep (&sleepytime, NULL);
 }

 return 0;
}
#endif

bool CMutex::Lock(uint32 Wait)
{
#ifndef WIN32
	timeval now;
	timespec timeout;
	gettimeofday(&now, 0);
	timeout.tv_sec = now.tv_sec + 1;
	timeout.tv_nsec = now.tv_usec * Wait;

    return pthread_mutex_timedlock(&m_Lock, &timeout) == ETIMEDOUT;
#else
	return WaitForSingleObject(m_Lock, Wait) == WAIT_TIMEOUT;
#endif
}

void CMutex::Lock()
{
#ifndef WIN32
	pthread_mutex_lock(&m_Lock);
#else
	WaitForSingleObject(m_Lock, INFINITE);
#endif
}

void CMutex::Unlock()
{
#ifndef WIN32
	pthread_mutex_unlock(&m_Lock);
#else
	ReleaseMutex(m_Lock);
#endif
}
