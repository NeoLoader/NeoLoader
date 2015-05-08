#include "Caffeine.h"

#include <QDebug>

/*
//#include <dispatch/dispatch.h>
//#include <CoreFoundation/CFNumber.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
//#include "IOKit/pwr_mgt/IOPMLibPrivate.h"
*/

class CCaffeinePrivate
{
public:
    CCaffeinePrivate()  
	{
        /*assertionID = 0;*/
	}

    /*IOPMAssertionID assertionID;*/
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
/*
static CFStringRef kHumanReadableReason = CFSTR("on Coffeine");
static CFStringRef kLocalizationBundlePath = CFSTR("/System/Library/CoreServices/powerd.bundle");
*/
void CCaffeine::Start()
{
    /*
    if(d->assertionID)
        return;

    CFStringRef assertionDetailsString = NULL;
    char assertionDetails[128];
    assertionDetailsString = CFStringCreateWithCString(kCFAllocatorDefault, assertionDetails, kCFStringEncodingMacRoman);

    // QCoreApplication::applicationName() // TODO use this instead of CFSTR("Caffeine")
    IOReturn result = IOPMAssertionCreateWithDescription(kIOPMAssertionTypePreventUserIdleSystemSleep,
                                                CFSTR("Caffeine"), assertionDetailsString,
                                                kHumanReadableReason, kLocalizationBundlePath, 0.0, NULL, &d->assertionID);

    if (result != kIOReturnSuccess)
    {
        qWarning() << "Failed to create" << CFStringGetCStringPtr(kIOPMAssertionTypePreventUserIdleSystemSleep, kCFStringEncodingMacRoman) << "assertion";
        return;
    }
*/
}

bool CCaffeine::IsRunning()
{
    /*return d->assertionID != 0;*/
    return false;
}

void CCaffeine::Stop()
{
    /*if(!d->assertionID)
        return;

    IOPMAssertionRelease(d->assertionID);
    d->assertionID = 0;*/
}
