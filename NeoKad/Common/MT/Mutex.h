#pragma once

#ifndef WIN32
   #include <sys/time.h>
   #include <sys/uio.h>
   #include <pthread.h>
#else
   #include <windows.h>
   #undef GetObject
#endif

class CMutex
{
public:
	CMutex();
	~CMutex();
	
	bool		Lock(uint32 Wait);
	void		Lock();
	void		Unlock();

protected:
#ifndef WIN32
	pthread_mutex_t m_Lock;
#else
	HANDLE m_Lock;
#endif
};