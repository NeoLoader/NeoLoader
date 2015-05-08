#include "test_qt.h"

test_qt::test_qt(QWidget *parent, Qt::WFlags flags)
	: QMainWindow(parent, flags)
{
	ui.setupUi(this);
}

test_qt::~test_qt()
{

}
