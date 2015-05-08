#include "GlobalHeader.h"
#include "MuleKad.h"
#include "KadGUI.h"
#include <QApplication>
#include "../Framework/MT/ThreadEx.h"
#ifdef _DEBUG
//#include <vld.h>
#endif

int main(int argc, char *argv[])
{
#ifndef _DEBUG
	InitMiniDumpWriter(L"MuleKad");
#endif

	qsrand(QTime::currentTime().msec()); // needs to be done in every thread we use random numbers

	bool bSilent = false;
	for(int i=1; i < argc; i++)
	{
		if(strcmp(argv[i], "-silent") == 0)
			bSilent = true;
	}

	QCoreApplication* Application = !bSilent ? new QApplication(argc, argv) : new QCoreApplication(argc, argv);
	CMuleKad* MuleKad = new CMuleKad();
	
	CKadGUI* KadGUI = NULL;
	if(bSilent)
		MuleKad->SetEmbedded();
	else
	{
		KadGUI = new CKadGUI(MuleKad);
		KadGUI->show();
	}

	int Ret = Application->exec();

	delete KadGUI;
	delete MuleKad;
	delete Application;

	return Ret;
}
