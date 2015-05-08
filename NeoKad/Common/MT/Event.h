#pragma once

#ifndef WIN32
   #include <sys/time.h>
   #include <sys/uio.h>
   #include <pthread.h>
#else
   #include <windows.h>
   #undef GetObject
#endif

class CEvent
{
public:
	CEvent();
	~CEvent();
	
	bool		Lock(uint32 Wait);
	void		Lock();
	void		Release();

protected:
#ifndef WIN32
	pthread_mutex_t m_Lock;
	pthread_cond_t m_Cond;
#else
	HANDLE m_Cond;
#endif
};