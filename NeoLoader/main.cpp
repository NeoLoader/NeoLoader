#include "GlobalHeader.h"
#include "NeoCore.h"
#include <QSplashScreen>
#include "GUI/NeoLoader.h"
#include "../Framework/MT/ThreadEx.h"
#include "../Framework/Scope.h"
//#include "../qtservice/src/qtservice.h"
//#include "../qtsingleapp/src/qtsingleapplication.h"
#include "../qtsingleapp/src/qtlocalpeer.h"
#include <QMessageBox>
#include <QApplication>
#ifdef _DEBUG
//#include <vld.h>
#endif
#include "./NeoVersion.h" 

#ifndef WIN32
#include <signal.h>

bool g_NoHup = false;
bool g_NoInt = false;

void unix_signal_handler(int sig)
{
    printf("got Signal: %i\r\n", sig);
    if(sig == SIGHUP && g_NoHup)
        return;
    if(sig == SIGINT && g_NoInt)
        return;
    if(QApplication::instance() && theCore)
    {
        printf("posting quit...\r\n");
        QMetaObject::invokeMethod(QApplication::instance(), "quit", Qt::QueuedConnection);
    }
    else
        exit(0);
}

int setup_unix_signal_handlers()
{
    struct sigaction sigHup, sigTerm, sigInt;

    sigHup.sa_handler = unix_signal_handler;
    sigemptyset(&sigHup.sa_mask);
    sigHup.sa_flags = 0;
    sigHup.sa_flags |= SA_RESTART;

    if (sigaction(SIGHUP, &sigHup, 0) > 0)
        return 1;

    sigTerm.sa_handler = unix_signal_handler;
    sigemptyset(&sigTerm.sa_mask);
    sigTerm.sa_flags |= SA_RESTART;

    if (sigaction(SIGTERM, &sigTerm, 0) > 0)
        return 2;

    sigInt.sa_handler = unix_signal_handler;
    sigemptyset(&sigInt.sa_mask);
    sigInt.sa_flags |= SA_RESTART;

    if (sigaction(SIGINT, &sigInt, 0) > 0)
        return 3;

    // Prevent termination is stdio pipes close
    struct sigaction noaction;
    memset(&noaction, 0, sizeof(noaction));
    noaction.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &noaction, 0) > 0)
        return 4;

    return 0;
}
#endif

/*
class CNeoService : public QtServiceBase
{
public:
    CNeoService(int argc, char **argv, CSettings* Settings)
     : QtServiceBase(argc, argv, Settings->GetString("Core/ServiceName"))
    {
        setStartupType(Settings->GetBool("Core/AutoStart") ? QtServiceController::AutoStartup : QtServiceController::ManualStartup);
        m_Settings = Settings;
    }

protected:
    void createApplication(int &argc, char **argv) { }

    int executeApplication()
    {
        int Ret = QApplication::instance()->exec();
        return Ret;
    }

    void start()	{new CNeoCore(m_Settings);}
    void stop()		{delete theCore;}

    // No Pausing
    void pause()    {ASSERT(0);}
    void resume()   {ASSERT(0);}

    CSettings* m_Settings;
};
*/

extern int g_iUpdateReady;

int main(int argc, char *argv[])
{
#ifndef _DEBUG
	InitMiniDumpWriter(GetNeoVersion(true).toStdWString().c_str());
#endif

#ifndef WIN32
    setup_unix_signal_handlers();
#endif

    bool bGui = false;
    bool bCore = false;
	bool bTray = false;
    bool bDaemon = false;

    for(int i=0; i<argc; i++)
    {
        if(strcmp(argv[i],"-unify") == 0)
        {
            bGui = true;
            bCore = true;
        }
        else if(strcmp(argv[i],"-gui") == 0)
            bGui = true;
        else if(strcmp(argv[i],"-core") == 0)
            bCore = true;
        else if(strcmp(argv[i],"-daemon") == 0)
            bDaemon = true;
		else if(strcmp(argv[i],"-tray") == 0)
			bTray = true;
    }

    bool bConsole = (bCore || bDaemon) && !bTray;

    // Note: we need a Q*Applicatio instance to read the settings, so here we can start the core always onl with command lin
    CScoped<QCoreApplication> pApp = bConsole ? new QCoreApplication(argc, argv) : new QApplication(argc, argv);

	if(bDaemon)
    {
        setbuf(stdout, NULL);

        if(bCore)
        {
#ifndef WIN32
            // Note: when a process becomes orphan (that is, its parent dies) it is adopted by init.
            g_NoInt = true;
#endif
        }
        else
        {
            QProcess* pCoreProcess = new QProcess();
            pCoreProcess->setProcessChannelMode(QProcess::MergedChannels);
            QStringList Arguments;
            Arguments.append("-core");
            Arguments.append("-daemon");
            pCoreProcess->start(QApplication::applicationFilePath(), Arguments);

            pCoreProcess->waitForStarted();

            for(;pCoreProcess->state() == QProcess::Running;)
            {
                QCoreApplication::processEvents();

                if(!pCoreProcess->canReadLine())
                {
                    QThread::currentThread()->msleep(10);
                    continue;
                }

                printf("%s", pCoreProcess->readLine().data());
            }

            return 0;
        }
    }

	CScoped<QtLocalPeer> pPeer = new QtLocalPeer(bConsole ? "NeoCore" : (bGui && bTray ? "NeoGUI" : "NeoApp"));
	if(pPeer->isClient())
	{
		QStringList Args = QApplication::instance()->arguments();
		for(int i=1; i < Args.count(); i++)
		{
			QString Arg = Args.at(i);
			if(Arg.left(1) != "-")
				pPeer->sendMessage(Arg);
		}
		return 0;
	}

    CSettings::InitSettingsEnvironment(APP_ORGANISATION, APP_NAME, APP_DOMAIN);
    CScoped<CSettings> Settings = new CSettings("NeoLoader", CNeoLoader::GetDefaultSettings());

#ifndef WIN32
    g_NoHup = Settings->GetBool("Integration/NoHUP") || bDaemon;
#endif

    for(int i=0; i<argc; i++)
    {

        if(strcmp(argv[i],"-name") == 0 && ++i<argc)
            Settings->SetSetting("Core/LocalName", QString(argv[i]));
        else if(strcmp(argv[i],"-port") == 0 && ++i<argc)
            Settings->SetSetting("Core/RemotePort", QString(argv[i]).toInt());
		else if(strcmp(argv[i],"-password") == 0 && ++i<argc)
            Settings->SetSetting("Core/Password", QString(argv[i]));
    }

	int NeoMode = CNeoLoader::eNoNeo;
	if(bConsole)
		NeoMode = CNeoLoader::eNeoCore;
	else // Some kind of GUI mode
	{
		if(bGui)
			NeoMode = bTray ? CNeoLoader::eGUIProcess : CNeoLoader::eRemoteGUI;
		else if(bCore) // Core with Tray
		{
			NeoMode = CNeoLoader::eCoreProcess;
			if(bTray)
				NeoMode |= CNeoLoader::eMinimized;
		}
		else
		{
			QString Mode = Settings->GetString("Core/Mode");
			if(Mode == "Separate")
				NeoMode = CNeoLoader::eCoreProcess;
			else if(Mode == "Local")
				NeoMode = CNeoLoader::eLocalGUI;
			else if(Mode == "Remote")
				NeoMode = CNeoLoader::eRemoteGUI;
			else //if(Mode == "Unified")
				NeoMode = CNeoLoader::eUnified;
		}
	}

    if ((NeoMode & CNeoLoader::eNeoCore) != 0)
		new CNeoCore(Settings);

	if ((NeoMode & CNeoLoader::eNeoGUI) != 0)
        new CNeoLoader(Settings, NeoMode);

	int Ret = 0;

    if(theLoader || theCore)
    {
		QObject::connect(pPeer, SIGNAL(messageReceived(const QString&)), bConsole ? (QObject*)theCore : (QObject*)theLoader, SLOT(OnMessage(const QString&)));

		QStringList Args = QApplication::instance()->arguments();
		for(int i=1; i < Args.count(); i++)
		{
			QString Arg = Args.at(i);
			if(Arg.left(1) != "-")
				QMetaObject::invokeMethod(bConsole ? (QObject*)theCore : (QObject*)theLoader, "OnMessage", Qt::QueuedConnection, Q_ARG(const QString&, Arg));
		}

        Ret = pApp->exec();

        delete theCore;
        delete theLoader;
    }

    int AutoUpdate = Settings->GetInt("Updater/AutoUpdate");

    Settings->sync();

    // install pending updates
    if(g_iUpdateReady && (AutoUpdate == 2 || bConsole || QMessageBox(CNeoLoader::tr("NeoLoader Update"), CNeoLoader::tr("There are NeoLoader Updates available, update now?"),
                                                QMessageBox::Question, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No, QMessageBox::NoButton).exec() == QMessageBox::Yes))
    {
        QStringList Args;
        if(AutoUpdate == 2 || bConsole)
            Args.append("-embedded");
		if(g_iUpdateReady == 2)
			Args.append("-download");
        Args.append("-install");
#ifndef WIN32
        QProcess::startDetached(Settings->GetAppDir() + "/NeoSetup", Args, Settings->GetAppDir());
#else
        QProcess::startDetached(Settings->GetAppDir() + "/NeoSetup.exe", Args, Settings->GetAppDir());
#endif
    }

    return Ret;
}
