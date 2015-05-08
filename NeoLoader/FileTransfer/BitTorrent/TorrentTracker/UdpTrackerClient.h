#pragma once

#include <QHostInfo>
#include "TrackerClient.h"

class CUdpTrackerClient : public CTrackerClient
{
    Q_OBJECT

public:
    CUdpTrackerClient(const QUrl& Url, const QByteArray& InfoHash, QObject* qObject = NULL);

	virtual void			SetupSocket(const CAddress& IPv4);

	virtual void			Announce(EEvent Event);

	virtual bool			IsBusy()							{return m_TransactionID != 0;}

private slots:
	virtual void			OnDatagrams();
	virtual void			OnLookupHost(const QHostInfo &Host);

protected:
	void timerEvent(QTimerEvent* pEvent)				
	{
		if(pEvent->timerId() == m_uTimerID)
			TimedOut();
	}

	virtual void			Connect();
	virtual void			TimedOut();

	QUdpSocket*				m_Socket;
	QHostAddress			m_Address;

	uint64					m_ConnectionID;
	uint64					m_IDTimeOut;

	enum EAction
	{
		eIdle		= -1,
		eConnect	= 0,
		eAnnounce	= 1,
		eScrape		= 2,
		eError		= 3
	};
	uint32					m_TransactionID;

	EEvent					m_PendingEvent;

	int						m_FailCount;
	int						m_uTimerID;
};
