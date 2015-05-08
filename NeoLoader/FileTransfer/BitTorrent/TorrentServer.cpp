#include "GlobalHeader.h"
#include "TorrentServer.h"
#include "TorrentSocket.h"
#include "../../NeoCore.h"
#include "../../Networking/BandwidthControl/BandwidthLimiter.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"
#include "../../../Framework/Cryptography/HashFunction.h"

CTorrentServer::CTorrentServer() 
{
	m_ServerV4uTP = new CTorrentUDP(this);
	connect(m_ServerV4uTP, SIGNAL(Connection(CStreamSocket*)), this, SIGNAL(Connection(CStreamSocket*)));
	
	if(m_ServerV6)
	{
		m_ServerV6uTP = new CTorrentUDP(this);
		connect(m_ServerV6uTP, SIGNAL(Connection(CStreamSocket*)), this, SIGNAL(Connection(CStreamSocket*)));
	}
}

CStreamSocket* CTorrentServer::AllocSocket(bool bUTP, void* p)
{
	CTorrentSocket* pSocket = new CTorrentSocket((CSocketThread*)thread());
	pSocket->AddUpLimit(m_UpLimit);
	pSocket->AddDownLimit(m_DownLimit);
	if(bUTP)
		InitUTP(pSocket, (struct UTPSocket*)p);
	else
		Init(pSocket, p ? *(SOCKET*)p : INVALID_SOCKET);
	AddSocket(pSocket);
	return pSocket;
}

const QByteArray& CTorrentServer::GetInfoHash(const QByteArray& CryptoHash)
{
	QMutexLocker Locker(&m_CryptoMutex); 
	return m_InfoHashes[CryptoHash];
}

void CTorrentServer::AddInfoHash(const QByteArray& InfoHash)
{
	CHashFunction Hash2(CAbstractKey::eSHA1);
	Hash2.Add((byte*)"req2", 4);
	Hash2.Add((byte*)InfoHash.data(), InfoHash.size());
	Hash2.Finish();

	QMutexLocker Locker(&m_CryptoMutex);
	m_InfoHashes[Hash2.ToByteArray()] = InfoHash;
}

void CTorrentServer::SendDHTPacket(QByteArray Packet, CAddress Address, quint16 uDHTPort)
{
	CountUpUDP((int)Packet.size(), Address.Type());

	if(Address.Type() == CAddress::IPv4)
		((CTorrentUDP*)m_ServerV4uTP)->SendDatagram(Packet.data(), Packet.size(), Address, uDHTPort);
	else if(Address.Type() == CAddress::IPv6)
		((CTorrentUDP*)m_ServerV6uTP)->SendDatagram(Packet.data(), Packet.size(), Address, uDHTPort);
}

////////////////////////////////////////////////////////////////////////////////////////////////
//

CTorrentUDP::CTorrentUDP(QObject* qObject)
:CUtpListener(qObject)
{
}

void CTorrentUDP::SendDatagram(const char *data, qint64 len, const CAddress &host, quint16 port)
{
	CUtpListener::SendDatagram(data, len, host, port);
}

void CTorrentUDP::ReciveDatagram(const char *data, qint64 len, const CAddress &host, quint16 port)
{
	if (len > 20 && *data == 'd' && data[len-1] == 'e') // this is probably a dht message
	{
		CTorrentServer* pServer = qobject_cast<CTorrentServer*>(parent());
		pServer->CountDownUDP((int)len, host.Type());

		emit pServer->ProcessDHTPacket(QByteArray(data, len), host, port);
	}
	else // UTP packet
		CUtpListener::ReciveDatagram(data, len, host, port);
}