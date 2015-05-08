#include "qtping.h"
#include "qtpinger.h"

QtPing::QtPing(QObject* parent)
 : QObject(parent), m_pPing(newQtPinger(this))
{
}

QtPing::~QtPing()
{
}

QtPingStatus QtPing::Ping(const QHostAddress& address, int nTTL, int nTimeOut)
{
	return m_pPing->Ping(address, nTTL, nTimeOut);
}

QList<QHostAddress> QtPing::Trace(const QHostAddress& address, int nTTL, int nTimeOut, int nRetry)
{
	//qDebug() << "";
	//qDebug() << "Tracing: " << address.toString();
	//qDebug() << "------------";

	QList<QHostAddress> Route;
	QHostAddress LastHoop;
	int nLastTTL = 0;
	for(int nCurTTL = 1; nCurTTL <= nTTL; nCurTTL++)
	{
		if(nCurTTL - nLastTTL > qMax(3, nLastTTL/5))
			break; // just give up

		QtPingStatus Status;
		for(int Try = 0; Try < nRetry; Try ++)
		{
			Status = m_pPing->Ping(address, nCurTTL, nTimeOut);
			if(!Status.address.isNull())
				break;
		}

		if(!Status.address.isNull())
		{
			if(LastHoop == Status.address)
				break;

			LastHoop = Status.address;
			nLastTTL = nCurTTL;
		}

		//qDebug() << "IP: "  << Status.address.toString();
		Route.append(Status.address);
		if(Status.success)
			break; // we reached the target finish
	}
	while(Route.count() > nLastTTL) // clear empty entries from the bottom
		Route.removeAt(nLastTTL);
	return Route;
}

bool QtPing::Start(const QHostAddress& address, int nTTL, int nInterval)
{
	return m_pPing->Start(address, nTTL, nInterval);
}

bool QtPing::IsPinging()
{
	return m_pPing->IsPinging();
}

void QtPing::Stop()
{
	m_pPing->Stop();
}