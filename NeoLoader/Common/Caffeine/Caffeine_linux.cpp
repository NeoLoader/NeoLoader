#include "Caffeine.h"

#include <QtDBus/QtDBus>

class CCaffeinePrivate
{
public:
    CCaffeinePrivate()  
	{
        pItf = NULL;
        bGnome = false;
        uCookie = 0;
	}

    QDBusInterface* pItf;
    bool bGnome;
    uint uCookie;
};

CCaffeine::CCaffeine(QObject* parent)
 : QObject(parent) , d(new CCaffeinePrivate())
{
    if (!QDBusConnection::sessionBus().isConnected())
    {
        qWarning() << "Cannot connect to the D-Bus session bus.";
        return;
    }

    d->pItf = new QDBusInterface("org.gnome.SessionManager", "/org/gnome/SessionManager", "org.gnome.SessionManager");
    if(!d->pItf)
        return;
    if (!(d->bGnome = d->pItf->isValid()))
    {
        delete d->pItf;
        d->pItf = new QDBusInterface("org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver");
        if (!d->pItf->isValid())
        {
            delete d->pItf;
            d->pItf = NULL;
        }
    }
}

CCaffeine::~CCaffeine()
{
    if(IsRunning())
        Stop();
    delete d->pItf;
}

void CCaffeine::Start()
{
    if(!d->pItf)
        return;
    if(d->uCookie)
        return;
    /*
    1: Inhibit logging out
    2: Inhibit user switching
    4: Inhibit suspending the session or computer
    8: Inhibit the session being marked as idle
    */
    QDBusReply<uint> reply = d->pItf->call("Inhibit", QCoreApplication::applicationName(), (uint)0, "on Coffeine", (uint)4);
    if(!reply.isValid())
        qWarning() << reply.error();
    else
        d->uCookie = reply.value();
}

bool CCaffeine::IsRunning()
{
    return d->uCookie != 0;
}

void CCaffeine::Stop()
{
    if(!d->pItf)
        return;
    if(!d->uCookie)
        return;

    QDBusMessage reply = d->pItf->call(d->bGnome ? "Uninhibit" : "UnInhibit", d->uCookie);
    if(reply.type() == QDBusMessage::ErrorMessage)
        qWarning() << reply.errorMessage();
    else
        d->uCookie = 0;
}
