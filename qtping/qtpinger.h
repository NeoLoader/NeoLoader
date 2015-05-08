#ifndef QTPINGER_H
#define QTPINGER_H

#include "qtping.h"

class QtPinger
{
public:
	virtual ~QtPinger() {}

	virtual QtPingStatus Ping(const QHostAddress& address, int nTTL = 64, int nTimeOut = 3000) = 0;

	virtual bool	Start(const QHostAddress& address, int nTTL = 64, int nInterval = 1000) = 0;
	virtual bool	IsPinging() = 0;
	virtual void	Stop() = 0;
};

QtPinger* newQtPinger(QObject* parent = 0);

#endif // QTPINGER_H
