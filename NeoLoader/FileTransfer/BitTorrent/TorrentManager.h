#pragma once

#include "../../../Framework/HttpServer/HttpServer.h"
//#include "../../../Framework/ObjectEx.h"
#include "TorrentSocket.h"
#include "../Transfer.h"
#include "../../../DHT/Peer.h"
#include "TorrentPeer.h"
#include "../FWWatch.h"

class CTorrentClient;
class CFile;
class CTorrentServer;
class CTrackerServer;
class CTorrentInfo;
class CTrackerClient;
class CTorrent;
struct STorrentPeer;
class CBandwidthLimit;
class CStreamServer;
class CDHT;

class CTorrentManager: public QObjectEx, public CHttpHandler
{
	Q_OBJECT

public:
	CTorrentManager(QObject* qObject = NULL);
	~CTorrentManager();

	void							Process(UINT Tick);

	void							AddConnection(CTorrentClient* pClient, bool bPending = false);
    void							RemoveConnection(CTorrentClient* pClient);
	const QList<CTorrentClient*>&	GetClients()								{return m_Connections;}
	int								GetConnectionCount();
	CTorrentServer*					GetServer()									{return m_Server;}
	bool							MakeRendezvous(CTorrentClient* pClient);

	void							AnnounceNow(CFile* pFile);

	void							AddDHTNode(const CAddress& Address, uint16 Port);

	QString							GetTrackerStatus(CTorrent* pTorrent, const QString& Url, uint64* pNext = NULL);

	void							RegisterInfoHash(const QByteArray& InfoHash);

	bool							DispatchClient(CTorrentClient* pClient);

	CTorrentClient*					FindClient(const CAddress& Address, uint16 uPort);

	void							AddToFile(CTorrent* pTorrent, const STorrentPeer& Peer, EFoundBy FoundBy);

	bool							IsFirewalled(CAddress::EAF eAF, bool bUTP = false) const;
	CAddress						GetAddress(CAddress::EAF eAF) const;
	int								GetIPv6Support() const;

	const TTorrentID				GetClientID() const							{return m_ClientID;}
	QString							GetTorrentDir() const						{return m_TorrentDir;}

	// Note: this function may be called from the network thread
	bool							SupportsCryptLayer() const					{return m_SupportsCryptLayer;}
	bool							RequestsCryptLayer() const					{return m_RequestsCryptLayer;}
	bool							RequiresCryptLayer() const					{return m_RequiresCryptLayer;}

	QVariantMap						GetDHTStatus() const						{return m_DHTStatus;}
	QString							GetDHTStatus(CTorrent* pTorrent, uint64* pNext = NULL) const;
	const QByteArray&				GetNodeID() const							{return m_NodeID;}

	const QString&					GetVersion() const							{return m_Version;}

	uint64							GrabTorrent(const QByteArray& FileData, const QString& FileName);

	const STransferStats&			GetStats() const							{return m_TransferStats;}

	CFWWatch*						FW()										{return &m_FWWatch;}

public slots:
	void							OnConnection(CStreamSocket* pSocket);

	void							OnPeersFound(QByteArray InfoHash, TPeerList PeerList);
	void							OnEndLookup(QByteArray InfoHash);

	void							OnRequestCompleted();
	void							OnFilePosted(QString Name, QString File, QString Type);

	void							RestartDHT();

public slots:
	void							OnBytesWritten(qint64 Bytes);
    void 							OnBytesReceived(qint64 Bytes);

protected:
	virtual void					HandleRequest(CHttpSocket* pRequest);
	virtual void					ReleaseRequest(CHttpSocket* pRequest);

	void							ManageConnections(CFile* pFile);

	void							UpdateCache();

	void							SetupDHT();

	TTorrentID						m_ClientID;

	CTorrentServer*					m_Server;
	CTrackerServer*					m_Tracker;
	QList<CTorrentClient*>			m_Connections;
	QList<CTorrentClient*>			m_Pending;

	// Note: this function may be called from the network thread
	volatile bool					m_SupportsCryptLayer;
	volatile bool					m_RequestsCryptLayer;
	volatile bool					m_RequiresCryptLayer;

	CDHT*							m_pDHT;
	time_t							m_LastSave;
	QVariantMap						m_DHTStatus;
	QByteArray						m_NodeID;

	bool							m_bEnabled;

	QString							m_Version;

	struct STorrentTracking
	{
		QByteArray					InfoHash;
		uint64						NextDHTAnounce;

		uint64						LastInRange;
		QMap<QString, CTrackerClient*> Trackers;
	};
	QMap<CTorrent*, STorrentTracking*>	m_TorrentTracking;
	QMap<QByteArray, QPointer<CTorrent> > m_TorrentTrackingRev; // Note: CTorrent may be gone at any time, this is not synchronized, so we use a QPointer to guard it

	QString							m_TorrentDir;

	STransferStats					m_TransferStats;

	CFWWatch						m_FWWatch;

	uint64							m_LastID;
};
