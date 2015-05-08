#pragma once

#include "dht_global.h"

#include <QObject>
#include <QString>
#include <QHostInfo>
#include <QTimerEvent>
#include <QByteArray>
#include "Peer.h"

namespace libtorrent{
namespace dht {
	class node_impl;
} 
	class dht_settings;
}

class CDHTPrivate;
class DHT_EXPORT CDHT: public QObject
{
	Q_OBJECT

public: 
	CDHT(const QByteArray& NodeID, const CAddress& Address, QObject* parent = 0);
	~CDHT();

	CAddress		GetAddress();
	QPair<QByteArray, TPeerList> GetState();
	QVariantMap		GetStatus();

public slots:
	void			Bootstrap(const TPeerList& PeerList);
	void			AddNode(const SPeer& Peer);
	void			AddRouterNode(const QString& Host, quint16 Port = 6881);
	void			Restart();

	void			Announce(const QByteArray& InfoHash, quint16 port = 0, bool seed = false);

	void			ProcessDHTPacket(QByteArray Packet, CAddress Address, quint16 uDHTPort);

signals:
	void			SendDHTPacket(QByteArray Packet, CAddress Address, quint16 uDHTPort);

	void			AddressChanged();
	
	void			PeersFound(QByteArray InfoHash, TPeerList PeerList);
	void			EndLookup(QByteArray InfoHash);

private slots:
	void			OnHostInfo(const QHostInfo& HostInfo);

protected:
	void			timerEvent(QTimerEvent* pEvent);
	int				m_uTimerID;

	libtorrent::dht::node_impl*	m_dht;
	libtorrent::dht_settings* m_dht_settings;
	quint64			m_last_new_key;
	quint64			m_next_tick;
	quint64			m_next_timeout;

private:
	Q_DECLARE_PRIVATE(CDHT)
	Q_DISABLE_COPY(CDHT)
};
