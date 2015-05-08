#ifndef QTPINGER_UNIX_H
#define QTPINGER_UNIX_H

#include "qtpinger.h"

#include <QProcess>

class QtPingerUnix: public QObject, public QtPinger
{
	Q_OBJECT

public:
	QtPingerUnix(QObject* parent);
	~QtPingerUnix();

	virtual QtPingStatus Ping(const QHostAddress& address, int nTTL, int nTimeOut = 3000);

	virtual bool	Start(const QHostAddress& address, int nTTL = 64, int nInterval = 1000);
	virtual bool	IsPinging()	{return m_Ping.state() != QProcess::NotRunning;}
	virtual void	Stop();

signals:
	void			PingResult(QtPingStatus Status);

private slots:
	void			OnOutput();
	void			OnFinished(int exitCode, QProcess::ExitStatus exitStatus);

	void			Start();

protected:
	QHostAddress address;
	int nTTL;
	int nTimeOut;
	int nInterval;

	bool bReadHead;
	bool bIPv6;

	QProcess		m_Ping; // Note: MS vc11 x64 messes up inheritence of QProcess::open
};


#endif // QTPINGER_UNIX_H