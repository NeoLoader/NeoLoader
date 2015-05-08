#include "GlobalHeader.h"
#include "NeoKad.h"
#include "GUI/NeoKadGUI.h"
#include "Common/v8Engine/JSEngine.h"
#include <QApplication>
#include "GUI/ScriptDebugger/KadScriptDebugger.h"
#ifdef _DEBUG
//#include <vld.h>
#endif

int main(int argc, char *argv[])
{
#ifndef _DEBUG
	InitMiniDumpWriter(L"NeoKad");
#endif

	qsrand(QTime::currentTime().msec()); // needs to be done in every thread we use random numbers

	bool bSilent = false;
	bool bHidden = false;
	for(int i=1; i < argc; i++)
	{
		if(strcmp(argv[i], "-silent") == 0)
			bSilent = true;
		else if(strcmp(argv[i], "-hidden") == 0)
			bHidden = true;
		else if(strcmp(argv[i], "-debug") == 0)
		{
			QString Pipe = ++i < argc ? argv[i] : "KadDebug";

			QApplication App(argc, argv);
			CKadScriptDebugger Debugger(Pipe);
			return App.exec();
		}
	}

	CJSEngine::Init();
	v8::V8::SetFlagsFromCommandLine(&argc, argv, true);

	QCoreApplication* Application = !bSilent ? new QApplication(argc, argv) : new QCoreApplication(argc, argv);
	CNeoKad* NeoKad = new CNeoKad();
	
	CNeoKadGUI* NeoKadGUI = NULL;
	if(bSilent)
		NeoKad->SetEmbedded();
	else
	{
		NeoKadGUI = new CNeoKadGUI();
		if(bHidden)
			NeoKad->SetEmbedded();
		else
			NeoKadGUI->show();
	}

	int Ret = Application->exec();

	delete NeoKadGUI;
	delete NeoKad;
	delete Application;

	CJSEngine::Dispose();

	// force garbage collection
	/*{
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::V8::LowMemoryNotification();
	while(!v8::V8::IdleNotification()) {};
	}
	v8::V8::Dispose();*/

	return Ret;
}
