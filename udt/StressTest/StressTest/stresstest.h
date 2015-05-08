#pragma once

#include <QtGui/QMainWindow>
#include <QTimerEvent>
#include <QElapsedTimer>
#include <QMap>
#include "../../src/udt.h"

class StressTest : public QMainWindow
{
	Q_OBJECT

public:
	StressTest(QWidget *parent = 0, Qt::WFlags flags = 0);
	~StressTest();

	void Process();

protected:
	void				timerEvent(QTimerEvent* pEvent)
	{
		if(pEvent->timerId() == m_uTimerID)
			Process();
	}

	int m_uTimerID;
	int m_uTimerCounter;
	QElapsedTimer m_Timer;

	int m_SvrBasePort;
	int m_TestRange;
	int m_TestTime;
	int m_SvrPortCounter;

	QMap<UDTSOCKET, sockaddr*>	m_Servers;
	struct SInfo
	{
		qint64 StartTime;
		bool Connected;
	};
	QMap<UDTSOCKET, SInfo>	m_Clients;
};
