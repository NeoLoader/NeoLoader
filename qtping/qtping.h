#ifndef QTPING_H
#define QTPING_H

#include "qtping_global.h"

#include <QScopedPointer>
#include <QHostAddress>

class QtPinger;

struct QtPingStatus
{
	QtPingStatus() : success(false), delay(0), ttl(0) {}

	bool success;

	quint64 delay;
	QHostAddress address;
	quint32 ttl;

	QString error;
};

class QTPING_EXPORT QtPing: public QObject
{
	Q_OBJECT

public:
	QtPing(QObject* parent = 0);
	~QtPing();

	QtPingStatus	Ping(const QHostAddress& address, int nTTL = 64, int nTimeOut = 3000);
	QList<QHostAddress> Trace(const QHostAddress& address, int nTTL = 64, int nTimeOut = 3000, int nRetry = 3);

	bool			Start(const QHostAddress& address, int nTTL = 64, int nInterval = 1000);
	bool			IsPinging();
	void			Stop();

signals:
	void			PingResult(QtPingStatus Status);

protected:
	QScopedPointer<QtPinger> m_pPing;

private:
	Q_DISABLE_COPY(QtPing)
};

#endif // QTPING_H
