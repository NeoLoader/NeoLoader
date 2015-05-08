#pragma once
#include "../../../../Framework/HttpServer/HttpServer.h"
#include "../TorrentPeer.h"

struct STorrentPeerX: STorrentPeer
{
	STorrentPeerX() 
	 : LastSeen(0) {}
	time_t	LastSeen;
};

class CTrackerServer: public QObjectEx, public CHttpHandler
{
	Q_OBJECT

public:
	CTrackerServer(QObject* qObject = NULL);

public slots:
	void							OnRequestCompleted();

protected:
	virtual void					HandleRequest(CHttpSocket* pRequest);
	virtual void					ReleaseRequest(CHttpSocket* pRequest);

	QMap<QByteArray, QList<STorrentPeerX> >	m_PeerList;
};
