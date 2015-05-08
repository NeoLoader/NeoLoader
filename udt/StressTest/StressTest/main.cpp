#include "stresstest.h"
#include <QtGui/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	StressTest w;
	return a.exec();
}
