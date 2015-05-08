#pragma once

#include "../../Networking/UTPSocket.h"
#include "../../../Framework/Buffer.h"

class CBandwidthLimit;
class CStreamSocket;

class CTorrentServer: public CStreamServer
{
	Q_OBJECT

public:

	CTorrentServer();

	virtual	CStreamSocket*			AllocSocket(SOCKET Sock = INVALID_SOCKET)		{return AllocSocket(false, Sock != INVALID_SOCKET ? &Sock : NULL);}
	virtual	CStreamSocket*			AllocUTPSocket(struct UTPSocket* pSock = NULL)	{return AllocSocket(true, pSock);}

	const QByteArray&				GetInfoHash(const QByteArray& CryptoHash);
	void							AddInfoHash(const QByteArray& InfoHash);
	
public slots:
	void							SendDHTPacket(QByteArray Packet, CAddress Address, quint16 uDHTPort);

signals:
	void							ProcessDHTPacket(QByteArray Packet, CAddress Address, quint16 uDHTPort);

protected:
	friend class CTorrentUDP;

	virtual CStreamSocket*			AllocSocket(bool bUTP, void* p);

	QMap<QByteArray, QByteArray>	m_InfoHashes;

	QMutex							m_CryptoMutex;
};


////////////////////////////////////////////////////////////////////////////////////////////////
//

class CTorrentUDP: public CUtpListener
{
	Q_OBJECT

public:
	CTorrentUDP(QObject* qObject = NULL);

	virtual void					SendDatagram(const char *data, qint64 len, const CAddress &host, quint16 port);
	virtual void					ReciveDatagram(const char *data, qint64 len, const CAddress &host, quint16 port);
};