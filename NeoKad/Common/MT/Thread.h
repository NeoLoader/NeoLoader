#pragma once
#include "../Object.h"

#ifndef WIN32
   #include <sys/time.h>
   #include <sys/uio.h>
   #include <pthread.h>
#else
   #include <windows.h>
   #undef GetObject
#endif

class CThread: public CObject
{
public:
	DECLARE_OBJECT(CThread);

	CThread(void(*func)(const void*), const void* param);
	virtual ~CThread();

	virtual void		Start();
	virtual void		Stop();

	static void			Sleep(int sleep);

protected:
	bool				m_bRunning;

	void(*m_Func)(const void*);
	const void* m_Param;

#ifndef WIN32
	pthread_t m_Thread;
	static void* run(void* thread);
#else
	HANDLE m_Thread;
	HANDLE m_Exit;
	static DWORD WINAPI run(LPVOID thread);
#endif

};
