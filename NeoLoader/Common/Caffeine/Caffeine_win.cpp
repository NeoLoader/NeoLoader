#include "Caffeine.h"

#include <windows.h>
#include <winbase.h>

class CCaffeinePrivate
{
public:
    CCaffeinePrivate()  
	{
		bRunning = false;
	}

	bool bRunning;
};

CCaffeine::CCaffeine(QObject* parent)
 : QObject(parent) , d(new CCaffeinePrivate())
{
}

CCaffeine::~CCaffeine()
{
    if(IsRunning())
        Stop();
}

void CCaffeine::Start()
{
	d->bRunning = true;
	SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
}

bool CCaffeine::IsRunning()
{
	return d->bRunning;
}

void CCaffeine::Stop()
{
	SetThreadExecutionState(ES_CONTINUOUS);
	d->bRunning = false;
}