#include "GlobalHeader.h"
#include "../../../../qbencode/lib/bencode.h"
#include "../TorrentManager.h"
#include "../TorrentPeer.h"
#include "../TorrentServer.h"
#include "../Torrent.h"
#include "../../../NeoCore.h"
#include "TrackerClient.h"
#include "UdpTrackerClient.h"
#include "../../../../Framework/RequestManager.h"
#include "../../../../Framework/Buffer.h"
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

CTrackerClient* CTrackerClient::New(const QUrl& Url, const QByteArray& InfoHash, QObject* qObject)
{
	if(Url.scheme() == "http" || Url.scheme() == "https")
		return new CHttpTrackerClient(Url, InfoHash, qObject);
	if(Url.scheme() == "udp")
		return new CUdpTrackerClient(Url, InfoHash, qObject);
	return NULL;
}

CTrackerClient::CTrackerClient(const QUrl& Url, const QByteArray& InfoHash, QObject* qObject) 
 : QObjectEx(qObject)
{
	m_Url = Url;
	ASSERT(InfoHash.size() == 20);
	m_InfoHash = InfoHash;
	m_uUploaded = 0;
	m_uDownloaded = 0;
	m_uLeft = 0;

	m_Started = false;
	m_Completed = false;
	m_Stopped = false;

	m_NextAnnounce = GetTime();
	m_AnnounceInterval = theCore->Cfg()->GetInt("BitTorrent/AnnounceInterval");
}

void CTrackerClient::Announce(EEvent Event, uint64 uUploaded, uint64 uDownloaded, uint64 uLeft)
{
	m_uUploaded = uUploaded;
	m_uDownloaded = uDownloaded;
	m_uLeft = uLeft;
	Announce(Event);
}

void CTrackerClient::SetError(const QString& Error)
{
	m_NextAnnounce = 0;
	LogLine(LOG_WARNING, tr("Tracker %1 Error: %2").arg(GetUrl()).arg(Error));
	m_Error = Error;
}

void CTrackerClient::ClearError()
{
	m_Error.clear();
	m_NextAnnounce = GetTime();
}

time_t CTrackerClient::NextAnnounce()
{
	time_t uNow = GetTime();
	if(m_NextAnnounce > uNow)
		return m_NextAnnounce - uNow;
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//

CHttpTrackerClient::CHttpTrackerClient(const QUrl& Url, const QByteArray& InfoHash, QObject* qObject) 
 : CTrackerClient(Url, InfoHash, qObject)
{
	m_pReply = NULL;
}

CHttpTrackerClient::~CHttpTrackerClient()
{
}

void CHttpTrackerClient::Announce(EEvent Event)
{
	if(m_pReply)
	{
		m_pReply->abort();
		m_pReply->deleteLater();
		m_pReply = NULL;
	}

#if QT_VERSION < 0x050000
	QUrl Url = m_Url;

    // Percent encode the hash
    QString EncHash;
    for (int i = 0; i < m_InfoHash.size(); ++i)
	{
        EncHash += '%';
        EncHash += QString::number(m_InfoHash[i], 16).right(2).rightJustified(2, '0');
    }
	Url.addEncodedQueryItem("info_hash",EncHash.toLatin1());
	Url.addQueryItem("peer_id", theCore->m_TorrentManager->GetClientID());
	Url.addQueryItem("port", QString::number(theCore->m_TorrentManager->GetServer()->GetPort()));

	if(theCore->m_TorrentManager->SupportsCryptLayer())
		Url.addQueryItem("supportcrypto", "1");		

	Url.addQueryItem("compact", "1");

	Url.addQueryItem("numwant", QString::number(theCore->Cfg()->GetInt("BitTorrent/AnnounceWanted")));

	Url.addQueryItem("uploaded", QString::number(m_uUploaded));
	Url.addQueryItem("downloaded", QString::number(m_uDownloaded));
    Url.addQueryItem("left", QString::number(m_uLeft));

    if (Event == eCompleted)
	{
		m_Completed = true;
		Url.addQueryItem("event", "completed");
	}
	else if(Event == eStarted)
	{
		m_Started = true;
		if(m_uLeft == 0)
			m_Completed = true;
		Url.addQueryItem("event", "started");
	}
	else if(Event == eStopped)
	{
		m_Stopped = true;
		Url.addQueryItem("event", "stopped");
	}
#else
	QUrlQuery Query;

    // Percent encode the hash
	//Query.addQueryItem("info_hash",QString(m_InfoHash));
    QString EncHash;
    for (int i = 0; i < m_InfoHash.size(); ++i)
	{
        EncHash += '%';
        EncHash += QString::number(m_InfoHash[i], 16).right(2).rightJustified(2, '0');
    }
	Query.addQueryItem("info_hash",EncHash.toLatin1());
	Query.addQueryItem("peer_id", theCore->m_TorrentManager->GetClientID().ToArray());
	Query.addQueryItem("port", QString::number(theCore->m_TorrentManager->GetServer()->GetPort()));

	if(theCore->m_TorrentManager->SupportsCryptLayer())
		Query.addQueryItem("supportcrypto", "1");		

	Query.addQueryItem("compact", "1");

	Query.addQueryItem("numwant", QString::number(theCore->Cfg()->GetInt("BitTorrent/AnnounceWanted")));

	Query.addQueryItem("uploaded", QString::number(m_uUploaded));
	Query.addQueryItem("downloaded", QString::number(m_uDownloaded));
    Query.addQueryItem("left", QString::number(m_uLeft));

    if (Event == eCompleted)
	{
		m_Completed = true;
		Query.addQueryItem("event", "completed");
	}
	else if(Event == eStarted)
	{
		m_Started = true;
		if(m_uLeft == 0)
			m_Completed = true;
		Query.addQueryItem("event", "started");
	}
	else if(Event == eStopped)
	{
		m_Stopped = true;
		Query.addQueryItem("event", "stopped");
	}

	QUrl Url = m_Url;
	Url.setQuery(Query);
#endif

	ASSERT(m_pReply == NULL);
	QNetworkRequest Request = QNetworkRequest(Url);
	m_pReply = theCore->m_RequestManager->get(Request);
	connect(m_pReply, SIGNAL(finished()), this, SLOT(OnFinished()));
	m_NextAnnounce = -1;
}

void CHttpTrackerClient::OnFinished()
{
	QNetworkReply* pReply = (QNetworkReply*)sender();
	ASSERT(pReply == m_pReply);

	QByteArray Reply = m_pReply->readAll();
	QNetworkReply::NetworkError Error = m_pReply->error();
	m_pReply->deleteLater();
	m_pReply = NULL;

	if(Error != QNetworkReply::NoError)
	{
		SetError("Network Error");
		return;
	}

    Bdecoder Decoder(Reply);
    QVariantMap Dict;
    Decoder.read(&Dict);
    if (Decoder.error())
	{
		SetError(QString("Error parsing bencode response from tracker: %1").arg(Decoder.errorString()));
        return;
	}

    if (Dict.contains("failure reason"))
	{
        SetError(QString::fromUtf8(Dict["failure reason"].toByteArray()));
        return;
    }

    if (Dict.contains("warning message"))
	{
		LogLine(LOG_WARNING, tr("Tracker %1 warning: %2").arg(GetUrl()).arg(QString::fromUtf8(Dict["warning message"].toByteArray())));
    }

    if (Dict.contains("interval")) 
	{
        // Mandatory item
		m_AnnounceInterval = Dict["interval"].toUInt();
    }

	m_NextAnnounce = GetTime() + m_AnnounceInterval;

	TPeerList PeerList;

	if (Dict.contains("peers")) 
	{
		QVariant PeersEntry = Dict["peers"];
		if(PeersEntry.type() == QVariant::List)
		{
			QVariantList Peers = PeersEntry.toList();
			for (int i = 0; i < Peers.size(); i++)
			{
				QVariantMap PeerEntry = Peers.at(i).toMap();

				STorrentPeer Peer;
				Peer.ID = PeerEntry["peer id"].toByteArray();
				CAddress Address(QString::fromUtf8(PeerEntry["ip"].toByteArray()));
				quint16 Port = PeerEntry["port"].toUInt();
				PeerList.append(SPeer(Address, Port));
			}
		}
		else
		{
			CBuffer Peers(PeersEntry.toByteArray());
			while(Peers.GetSizeLeft() >= 4 + 2)
			{
				CAddress Address(Peers.ReadValue<uint32>(true));
				quint16 Port = Peers.ReadValue<uint16>(true);
				PeerList.append(SPeer(Address, Port));
			}
		}
	}

	if (Dict.contains("peers6")) 
	{
		QVariant PeersEntry = Dict["peers6"];
		CBuffer Peers(PeersEntry.toByteArray());
		while(Peers.GetSizeLeft() >= 16 + 2)
		{
			CAddress Address(Peers.ReadData(16));
			quint16 Port = Peers.ReadValue<uint16>(true);
			PeerList.append(SPeer(Address, Port));
		}
	}

	emit PeersFound(m_InfoHash, PeerList);
}
