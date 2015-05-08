#ifndef TEST_QT_H
#define TEST_QT_H

#include <QtGui/QMainWindow>
#include "ui_test_qt.h"

class test_qt : public QMainWindow
{
	Q_OBJECT

public:
	test_qt(QWidget *parent = 0, Qt::WFlags flags = 0);
	~test_qt();

private:
	Ui::test_qtClass ui;
};

#endif // TEST_QT_H
