#pragma once
#include "miniupnp_global.h"

#include <QObject>
#include <QThread>
#include <QTimerEvent>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>

class MINIUPNP_EXPORT CMiniUPnP: public QThread
{
	Q_OBJECT

public:
	CMiniUPnP(QObject* parent = 0);
	~CMiniUPnP();

	bool				StartForwarding(const QString& Name, int Port, const QString& ProtoStr);
	void				StopForwarding(const QString& Name);
	int					GetStaus(const QString& Name, int* pPort = NULL, QString* pProtoStr = NULL);

protected:
	void				run();
	void				closeOld();

	struct SPort
	{
		QString	Name;

		int Port;
		int Proto;
		bool Enabled;
		bool Check;

		int Status;
		int Countdown;

		struct upnp*	pUPnP;

		struct natpmp*	pNatPMP;
	};

	void				DoForwarding(SPort* pPort);

	QMutex				m_Mutex;
	QMap<QString, SPort*>m_Ports;
	QList<SPort*>		m_OldPorts;

	bool				m_Running;
};
