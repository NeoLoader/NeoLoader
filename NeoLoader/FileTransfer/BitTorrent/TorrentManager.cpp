#include "GlobalHeader.h"
#include "TorrentManager.h"
#include "TorrentInfo.h"
#include "../../FileList/FileManager.h"
#include "../../FileList/FileStats.h"
#include "../../NeoCore.h"
#include "../../NeoVersion.h"
#include "TorrentClient.h"
#include "TorrentServer.h"
#include "Torrent.h"
#include "../../../Framework/HttpServer/HttpSocket.h"
#include "../FileGrabber.h"
#include "../../../Framework/RequestManager.h"
#include "../PeerWatch.h"
#include "./TorrentTracker/TrackerClient.h"
#include "./TorrentTracker/UdpTrackerClient.h"
#include "./TorrentTracker/TrackerServer.h"
#include "../../../Framework/qzlib.h"
#include "../../../Framework/Cryptography/SymmetricKey.h"
#include "../../FileTransfer/UploadManager.h"
#include "../../Interface/CoreServer.h"
#include "../../Interface/WebRoot.h"
#include "../../Networking/SocketThread.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"
#include "../../../DHT/DHT.h"
#include "../../../MiniUPnP/MiniUPnP.h"
#include "../../../Framework/OtherFunctions.h"

CTorrentManager::CTorrentManager(QObject* qObject)
: QObjectEx(qObject)
{
	m_TorrentDir = theCore->Cfg()->GetSettingsDir() + "/Torrents/";
	CreateDir(m_TorrentDir);

    // Generate peer id
	QByteArray ClientID;
	ASSERT(NEO_VERSION_MJR < 10);
	ASSERT(NEO_VERSION_MIN < 100);
#if NEO_VERSION_UPD <= 9
	ClientID += "-NL" + QString::number(NEO_VERSION_MJR) + QString::number(NEO_VERSION_MIN).rightJustified(2, '0') + QString::number(NEO_VERSION_UPD) +  "- ";
#else
	ClientID += "-NL" + QString::number(NEO_VERSION_MJR) + QString::number(NEO_VERSION_MIN).rightJustified(2, '0') + QString::number(0) +  "- ";
#endif
    ClientID += GetRand64Str(true);
	m_ClientID = ClientID;

	// setup listner
	m_Server = new CTorrentServer();
	connect(m_Server, SIGNAL(Connection(CStreamSocket*)), this, SLOT(OnConnection(CStreamSocket*)));
	theCore->m_Network->AddServer(m_Server);
	m_Server->SetPorts(theCore->Cfg()->GetInt("BitTorrent/ServerPort"), theCore->Cfg()->GetInt("BitTorrent/ServerPort"));

	UpdateCache();

	m_pDHT = NULL;
	m_bEnabled = false;

	m_TransferStats.UploadedTotal = theCore->Stats()->value("BitTorrent/Uploaded").toULongLong();
	m_TransferStats.DownloadedTotal = theCore->Stats()->value("BitTorrent/Downloaded").toULongLong();

	m_Version = GetNeoVersion();

	theCore->m_HttpServer->RegisterHandler(this,"/Torrent");

	m_Tracker = new CTrackerServer(this);
}

CTorrentManager::~CTorrentManager()
{
	// Note: the CTrackerClient inside are children of CTorrentManager and will be deleted by QT automatically
	foreach(STorrentTracking* pTracking, m_TorrentTracking)
		delete pTracking;
}

void CTorrentManager::UpdateCache()
{
	m_SupportsCryptLayer = theCore->Cfg()->GetString("BitTorrent/Encryption") != "Disable";
	m_RequestsCryptLayer = theCore->Cfg()->GetString("BitTorrent/Encryption") == "Request" || theCore->Cfg()->GetString("BitTorrent/Encryption") == "Require";
	m_RequiresCryptLayer = theCore->Cfg()->GetString("BitTorrent/Encryption") == "Require";
}

void CTorrentManager::SetupDHT()
{
	m_pDHT = new CDHT(theCore->Cfg()->GetBlob("MainlineDHT/NodeID"), CAddress(theCore->Cfg()->GetString("MainlineDHT/Address")), this);

	connect(m_Server, SIGNAL(ProcessDHTPacket(QByteArray, CAddress, quint16)), m_pDHT, SLOT(ProcessDHTPacket(QByteArray, CAddress, quint16)));
	connect(m_pDHT, SIGNAL(SendDHTPacket(QByteArray, CAddress, quint16)), m_Server, SLOT(SendDHTPacket(QByteArray, CAddress, quint16)));

	connect(m_pDHT, SIGNAL(PeersFound(QByteArray, TPeerList)), this, SLOT(OnPeersFound(QByteArray, TPeerList)));
	connect(m_pDHT, SIGNAL(EndLookup(QByteArray)), this, SLOT(OnEndLookup(QByteArray)));

	connect(m_pDHT, SIGNAL(AddressChanged()), this, SLOT(RestartDHT()));

	foreach(const QString& RouterNode, theCore->Cfg()->GetStringList("MainlineDHT/RouterNodes"))
	{
		StrPair HostPort = Split2(RouterNode, ":", true);
		if(HostPort.second.isEmpty())
			HostPort.second = "6881";
		m_pDHT->AddRouterNode(HostPort.first, HostPort.second.toUInt());
	}

	TPeerList PeerList;
	QFile File(CSettings::GetSettingsDir() + "/DHTNodes.txt");
	if(File.open(QFile::ReadOnly))
	{
		while(!File.atEnd())
		{
			StrPair IPPort = Split2(File.readLine(),":",true);
			PeerList.append(SPeer(CAddress(IPPort.first), IPPort.second.toUInt()));
		}
		File.close();
	}
	m_pDHT->Bootstrap(PeerList);

	QPair<QByteArray, TPeerList> State = m_pDHT->GetState();
	m_NodeID = State.first;
	m_LastSave = GetTime();
}

void CTorrentManager::Process(UINT Tick)
{
	foreach(CTorrentClient* pClient, m_Connections)
		pClient->Process(Tick);

	// Incoming connections
	foreach(CTorrentClient* pClient, m_Pending)
	{
		if (pClient->IsDisconnected())
		{
			RemoveConnection(pClient);
			delete pClient;
		}
	}

	if ((Tick & EPerSec) == 0)
		return;

	if ((m_bEnabled != false) != theCore->Cfg()->GetBool("BitTorrent/Enable"))
	{
		if (!m_bEnabled)
		{
			ASSERT(!m_pDHT);
			SetupDHT();
			m_bEnabled = true;
		}
		else
		{
			ASSERT(m_pDHT);
			delete m_pDHT;
			m_pDHT = NULL;
			m_bEnabled = false;

			foreach(CFile* pFile, CFileList::GetAllFiles())
			{
				if (pFile->GetTorrents().isEmpty())
					continue;
				foreach(CTransfer* pTransfer, pFile->GetTransfers())
				{
					if (CTorrentPeer* pPeer = qobject_cast<CTorrentPeer*>(pTransfer))
						pFile->RemoveTransfer(pPeer);
				}
			}
		}
	}

	if (!m_bEnabled)
		return;

	ASSERT(m_pDHT);
	m_DHTStatus = m_pDHT->GetStatus();

	if (m_LastSave + theCore->Cfg()->GetInt("Content/SaveInterval") < GetTime())
	{
		m_LastSave = GetTime();

		theCore->Cfg()->SetSetting("MainlineDHT/Address", m_pDHT->GetAddress().ToQString());
		QPair<QByteArray, TPeerList> State = m_pDHT->GetState();
		m_NodeID = State.first;
		theCore->Cfg()->SetBlob("MainlineDHT/NodeID", m_NodeID);

		QFile File(CSettings::GetSettingsDir() + "/DHTNodes.txt");
		if (File.open(QFile::WriteOnly))
		{
			foreach(const SPeer& Peer, State.second)
				File.write(QString("%1:%2\r\n").arg(Peer.Address.ToQString()).arg(Peer.Port).toLatin1());
			File.close();
		}
	}

	UpdateCache();
	int iPriority = theCore->Cfg()->GetInt("BitTorrent/Priority");
	m_Server->GetUpLimit()->SetLimit(theCore->Cfg()->GetInt("BitTorrent/Upload"));
	//m_Server->GetUpLimit()->SetPriority(iPriority);
	m_Server->GetDownLimit()->SetLimit(theCore->Cfg()->GetInt("BitTorrent/Download"));
	m_Server->GetDownLimit()->SetPriority(iPriority);

	if ((Tick & E100PerSec) == 0)
	{
		theCore->Stats()->setValue("BitTorrent/Uploaded", m_TransferStats.UploadedTotal);
		theCore->Stats()->setValue("BitTorrent/Downloaded", m_TransferStats.DownloadedTotal);
	}

	bool bVPNMode = theCore->m_Network->GetIPv4().Type() == CAddress::None || !theCore->m_Network->GetIPv4().IsNull(); // NIC set or adapter disabled
	CAddress IPv4 = theCore->m_Network->GetIPv4();
	bool bResetTracking = false;

	if(m_Server->GetPort() != theCore->Cfg()->GetInt("BitTorrent/ServerPort") 
		|| m_Server->GetIPv4() != theCore->m_Network->GetIPv4() || m_Server->GetIPv6() != theCore->m_Network->GetIPv6())
	{
		m_Server->SetIPs(theCore->m_Network->GetIPv4(), theCore->m_Network->GetIPv6());
		m_Server->SetPorts(theCore->Cfg()->GetInt("BitTorrent/ServerPort"), theCore->Cfg()->GetInt("BitTorrent/ServerPort"));

		m_Server->UpdateSockets();

		bResetTracking = true;

		if(theCore->Cfg()->GetBool("Bandwidth/UseUPnP"))
		{
			int TCPPort = 0;
			if(theCore->m_MiniUPnP->GetStaus("TorrentTCP", &TCPPort) == -1 || TCPPort != m_Server->GetPort())
				theCore->m_MiniUPnP->StartForwarding("TorrentTCP", m_Server->GetPort(), "TCP");

			int UDPPort = 0;
			if(theCore->m_MiniUPnP->GetStaus("TorrentUDP", &UDPPort) == -1 || UDPPort != m_Server->GetUTPPort())
				theCore->m_MiniUPnP->StartForwarding("TorrentUDP", m_Server->GetUTPPort(), "UDP");
		}
		else
		{
			if(theCore->m_MiniUPnP->GetStaus("TorrentTCP") != -1)
				theCore->m_MiniUPnP->StopForwarding("TorrentTCP");

			if(theCore->m_MiniUPnP->GetStaus("TorrentUDP") != -1)
				theCore->m_MiniUPnP->StopForwarding("TorrentUDP");
		}
	}

	QList<CTorrent*> Tracked = m_TorrentTracking.keys();

	QMap<int, CFile*> Files; // Note: grabbed files have queue ID 0 and those only one is handled
	foreach(CFile* pFile, CFileList::GetAllFiles())
	{
		if(!pFile->IsStarted() || pFile->IsDuplicate() || pFile->GetTorrents().isEmpty())
			continue;

		int QueuePos = pFile->GetQueuePos();
		if(pFile->IsComplete(true)) // drop down complete torrents...
			QueuePos += (INT_MAX/2);
		Files.insert(QueuePos, pFile);
	}

	// Note: this applyes to files with torrents, not the count of torrents as such
	//	that means one fiel with 2 torrents will count a one t orrent
	//	on the otehr hand howeever all torents on one file wil share the max peers/torrentFile limit
	int MaxTorrents = theCore->Cfg()->GetInt("BitTorrent/MaxTorrents");
	int MaxTorrentsAux = Max(MaxTorrents / 10, 3);

	QList<CFile*> ActiveFiles;
	foreach(CFile* pFile, Files)
	{
		bool bTorrenting = false;
		foreach(CTorrent* pTorrent, pFile->GetTorrents())
		{
			if(pTorrent == NULL || pTorrent->GetInfoHash().isEmpty())
				continue; // no tirrent or a torent being made

			CFile* pFile = pTorrent->GetFile();
			CTorrentInfo* pTorrentInfo = pTorrent->GetInfo();
			if(pTorrentInfo->IsEmpty())
			{
				if(!pTorrent->TryInstallMetadata()) // this may delete the pTorrent - if neo is master
					continue;
			}

			// We do not count metadata gethering to active torrents
			// Note: Pending files are being stopped as soon asthay get metadata
			bTorrenting = --MaxTorrents >= 0 || pFile->GetProperty("Force").toBool();
			if(!bTorrenting && pFile->MetaDataMissing()) 
				bTorrenting = --MaxTorrentsAux >= 0;

			STorrentTracking* pTracking = m_TorrentTracking.value(pTorrent);
			if(pTracking == NULL)
			{
				if(!bTorrenting)
					continue;

				pTracking = new STorrentTracking();
				m_TorrentTracking.insert(pTorrent, pTracking);
				pTracking->InfoHash = pTorrentInfo->GetInfoHash();
				pTracking->NextDHTAnounce = GetCurTick();
				pTracking->LastInRange = GetCurTick();

				m_TorrentTrackingRev[pTracking->InfoHash] = pTorrent;
			}

			if(bTorrenting)
				pTracking->LastInRange = GetCurTick();
			else if(GetCurTick() - pTracking->LastInRange > SEC2MS(20))
				continue;

			Tracked.removeOne(pTorrent);
			bTorrenting = true;

			uint64 uUploaded = pFile->DownloadedBytes();
			uint64 uDownloaded = pFile->UploadedBytes();
			uint64 uLeft = pFile->IsComplete() ? 0 : pFile->GetFileSize() - pFile->DownloadedBytes();


			// announce DHT, but not for private torrents
			if(!pTorrentInfo->IsPrivate()
			&& pTracking->NextDHTAnounce <= GetCurTick())
			{
				pTracking->NextDHTAnounce = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("BitTorrent/AnnounceInterval"));

				QVariantMap Request;
				QByteArray InfoHash = pTorrent->GetInfoHash();
				ASSERT(InfoHash.size() == 20);
				m_pDHT->Announce(InfoHash, m_Server->GetPort(), pFile->IsComplete());
			}


			QList<CTrackerClient*> Trackers = pTracking->Trackers.values();

			QString Tracking = pFile->HasProperty("Tracking") ? pFile->GetProperty("Tracking").toString() : theCore->Cfg()->GetString("BitTorrent/Tracking");
			if(Tracking != "No-Trackers")
			{
				// Comment copyed form LibTorrent (Fair Use):
				// this announces to all trackers within the current
				// tier. Trackers within a tier are supposed to share
				// peers, this could be used for trackers that don't,
				// and require the clients to announce to all of them.
				bool bAllTrackers = false;

				// Comment copyed form LibTorrent (Fair Use):
				// if set to true, multi tracker torrents are treated
				// the same way uTorrent treats them. It defaults to
				// false in order to comply with the extension definition.
				// When this is enabled, one tracker from each tier is
				// announced
				bool bAllTiers = false;

				if(Tracking == "All-Trackers")	// announce to all trackers within the current tier
					bAllTrackers = true;
				else if(Tracking == "All-Tiers") // announce to all trackers on all tiers, like utorrent
				{
					bAllTrackers = true;
					bAllTiers = true;
				}

				if(!bResetTracking && (!bVPNMode || !IPv4.IsNull()))
				{
					bool bSuccess = false;
					foreach(int Tier, pTorrentInfo->GetTrackerList().uniqueKeys())
					{
						foreach(const QString& Url, pTorrentInfo->GetTrackerList().values(Tier))
						{
							if(bVPNMode && Url.left(3).compare("udp", Qt::CaseInsensitive) != 0)
								continue; // X-ToDo-Now: HTTP trackers are diss because we cant bind thair sockets with Qt, fix this

							CTrackerClient* pTracker = pTracking->Trackers.value(Url);
							if(pTracker == NULL)
							{
								pTracker = CTrackerClient::New(Url, pTorrentInfo->GetInfoHash(), this);
								if(!pTracker)
									continue;

								if(CUdpTrackerClient* pUDPTracker = qobject_cast<CUdpTrackerClient*>(pTracker))
									pUDPTracker->SetupSocket(IPv4);

								pTracking->Trackers.insert(Url, pTracker);
								connect(pTracker, SIGNAL(PeersFound(QByteArray, TPeerList)), this, SLOT(OnPeersFound(QByteArray, TPeerList)));
							}
							else
							{
								Trackers.removeOne(pTracker);

								if(pTracker->HasError())
									continue;

								if(pTracker->IsBusy())
								{
									bSuccess = true; // busy with no error counts as success
									continue;
								}
							}
						
							if(!bAllTrackers && bSuccess) // if not anouncing to all trackers break here
								continue;
							bSuccess = true; // no error or new tracker counts for now as success

							if(!pTracker->ChkStarted())
								pTracker->Announce(CTrackerClient::eStarted,	uUploaded, uDownloaded, uLeft);
							else if(pFile->IsComplete() && !pTracker->ChkCompleted())
								pTracker->Announce(CTrackerClient::eCompleted,	uUploaded, uDownloaded, uLeft);
							else if(pTracker->IntervalPassed())
								pTracker->Announce(CTrackerClient::eNone,		uUploaded, uDownloaded, uLeft);
						}

						if(bSuccess && !bAllTiers) // success and not anouncing all tiers break here
							break;
					}
				}
			}

			// announce stop to trackers we dont longer handle
			foreach(CTrackerClient* pTracker, Trackers)
			{
				if(pTracker->IsBusy())
					continue;
				if(!pTracker->ChkStopped() && !pTracker->HasError())
					pTracker->Announce(CTrackerClient::eStopped, uUploaded, uDownloaded, uLeft);
				else
				{
					pTracking->Trackers.remove(pTracking->Trackers.key(pTracker));
					delete pTracker;
				}
			}
		}

		if(bTorrenting != pFile->GetProperty("Torrenting").toBool())
		{
			pFile->SetProperty("Torrenting", bTorrenting);
			// we have to disconnect all torrent connections
			if(!bTorrenting)
			{
				foreach(CTransfer* pTransfer, pFile->GetTransfers())
				{
					if(CTorrentPeer* pTorrentPeer = qobject_cast<CTorrentPeer*>(pTransfer))
						pTorrentPeer->Hold(false);
				}
			}
		}

		if(bTorrenting) // Randomize list for equal new connections chance
			ActiveFiles.insert(rand() % (ActiveFiles.size() + 1), pFile);
	}

	foreach(CFile* pFile, ActiveFiles)
		ManageConnections(pFile);

	// clear removed, stopped torrents
	foreach(CTorrent* pTorrent, Tracked)
	{
		STorrentTracking* pTracking = m_TorrentTracking.value(pTorrent);

		// ensure no new connections will enter, also drop all existing connections
		if(pTracking->LastInRange)
		{
			pTracking->LastInRange = 0;
			m_TorrentTrackingRev.remove(pTracking->InfoHash); 
		}

		// announce stop to all used trackers
		foreach(const QString& Url, pTracking->Trackers.uniqueKeys())
		{
			CTrackerClient* pTracker = pTracking->Trackers.value(Url);
			if(pTracker->IsBusy())
				continue;
			if(!pTracker->ChkStopped() && !pTracker->HasError())
				pTracker->Announce(CTrackerClient::eStopped);
			else
			{
				pTracking->Trackers.remove(Url);
				delete pTracker;
			}
		}

		// finaly clean up
		if(pTracking->Trackers.isEmpty())
		{
			delete pTracking;
			m_TorrentTracking.remove(pTorrent);
		}
	}
}

bool CmpTorrentDisconnect(CTorrentPeer* pL, CTorrentPeer* pR) // pL < pR
{
	if(pL->IsInteresting() != pR->IsInteresting())
		return !pR->IsInteresting(); // if R is not interesting its greater for removal

	// Note: the higher the timeout value the more recent is the node
	//	so we want here to sort the oldest nodes to the top, so those with the smalest value
	return pL->GetClient()->GetIdleTimeOut() > pR->GetClient()->GetIdleTimeOut();
}

bool CmpTorrentConnect(CTorrentPeer* pL, CTorrentPeer* pR) // pL < pR
{
	// Note: we prefer to connect to nodes that we havnt connected yet
	if(pL->IsChecked() != pR->IsChecked())
		return pL->IsChecked(); // if L is checked R is bigget
	
	if(pL->IsInteresting() != pR->IsInteresting())
		return pR->IsInteresting(); // if R is interesting its greater for connect

	if(pL->DownloadedBytes() != pR->DownloadedBytes())
		return pL->DownloadedBytes() < pR->DownloadedBytes(); // if we got more from R its greater for connect

	return pL->GetLastConnect() > pR->GetLastConnect(); // the node that was longer not connected (LastConnect is lower) is greater for connect
}

template<class T>
void InsertSorted(T New, QList<T>& List, bool(*Cmp)(T L, T R), int Limit = -1) // gratest first
{
	for(int i=0; i < List.size(); i++)
	{
		if(Cmp(List.at(i), New)) // List.at(i) < New
		{
			List.insert(i, New);
			if(Limit != -1 && List.size() >= Limit)
				List.removeLast();
			return;
		}
	}
	if(Limit == -1 || List.size() < Limit)
		List.append(New);
}

void CTorrentManager::ManageConnections(CFile* pFile)
{
	int MaxPeers = theCore->Cfg()->GetInt("BitTorrent/MaxPeers");
	if(pFile->IsComplete(true))
		MaxPeers /= 2;

	CFileStats::STemp Temp = pFile->GetStats()->GetTempCount(eBitTorrent);
	int CurPeers = Temp.Connected;
	int Unchecked = Max(Temp.All - Temp.Checked, 0);
	bool bModLimit = Temp.Checked < Temp.All * 7 / 10; // if we checked less than 70% of all sources

	int LocalToMany = Temp.Connected - (bModLimit ? MaxPeers * 7 / 10 : MaxPeers);
	int GlobalToMany = theCore->m_Network->GetCount() - (bModLimit ? theCore->m_Network->GetLimit() * 7 / 10 : theCore->m_Network->GetLimit());
	int ToMany = Max(LocalToMany, GlobalToMany);
	if(ToMany > 0) // we have to drop some
	{
		QList<CTorrentPeer*> DisconnectCandidates;
		foreach(CTransfer* pTransfer, pFile->GetTransfers())
		{
			CTorrentPeer* pTorrentPeer = qobject_cast<CTorrentPeer*>(pTransfer);
			if(!pTorrentPeer || !pTorrentPeer->IsConnected())
				continue;

			// if we are complete drop other seeds right away
			if(pFile->IsComplete(true) && pTorrentPeer->IsSeed())
			{
				pTorrentPeer->GetClient()->Disconnect();
				continue;
			}

			// do not disconnect peers we are exchanging data right now
			if(pTorrentPeer->IsActiveDownload() || pTorrentPeer->IsActiveUpload())
				continue;
			// disconnect cleints only after thay have been idle for some time
			if(!pTorrentPeer->GetClient()->IsIdleTimeOut())
				continue;
			
			InsertSorted(pTorrentPeer, DisconnectCandidates, CmpTorrentDisconnect, ToMany);
		}
		foreach(CTorrentPeer* pTorrentPeer, DisconnectCandidates)
		{
			pTorrentPeer->GetClient()->Disconnect();
		}
	}

	int LocalToFew = MaxPeers - Temp.Connected;
	int GlobalToFew = theCore->m_Network->GetLimit() - theCore->m_Network->GetCount();
	int ToFew = Min(LocalToFew, GlobalToFew);
	if(ToFew > 0) // we can add some
	{
		//int MaxNew = theCore->Cfg()->GetInt("Bandwidth/MaxNewPer5Sec") / 5; // we tick every second
		int MaxNew = theCore->Cfg()->GetInt("Bandwidth/MaxNewPer5Sec"); // we tick every second
		ToFew = Min(ToFew, Max(1, MaxNew));
		QList<CTorrentPeer*> ConnectCandidates;
		foreach(CTransfer* pTransfer, pFile->GetTransfers())
		{
			CTorrentPeer* pTorrentPeer = qobject_cast<CTorrentPeer*>(pTransfer);
			if(!pTorrentPeer || pTorrentPeer->IsConnected() || pTorrentPeer->GetPort() == 0)
				continue;

			// do not connect to seeds if we are a seed ourselv
			if(pTorrentPeer->IsSeed() && pFile->IsComplete(true))
				continue;
			
			InsertSorted(pTorrentPeer, ConnectCandidates, CmpTorrentConnect, ToFew);
		}
		foreach(CTorrentPeer* pTorrentPeer, ConnectCandidates)
		{
			if(theCore->m_Network->CanConnect())
			{
				if(pTorrentPeer->Connect())
					theCore->m_Network->CountConnect();
				else // if connect failed it wil always fail, drop the peer
					pTorrentPeer->SetError("NoConnect");
			}
		}
	}
}

void CTorrentManager::RestartDHT()
{
	foreach(STorrentTracking* pTracking, m_TorrentTracking)
		pTracking->NextDHTAnounce = 0; // do that on timer
	m_LastSave = 0;
}

void CTorrentManager::AnnounceNow(CFile* pFile)
{
	if(!pFile->IsStarted())
	{
		LogLine(LOG_ERROR, tr("File %1 must be started in order to announce").arg(pFile->GetFileName()));
		return;
	}

	foreach(CTorrent* pTorrent, pFile->GetTorrents())
	{
		if(!pTorrent)
			continue;

		STorrentTracking* pTracking = m_TorrentTracking.value(pTorrent);
		if(!pTracking)
			continue;

		pTracking->NextDHTAnounce = 0; // do that on timer

		uint64 uUploaded = pFile->DownloadedBytes();
		uint64 uDownloaded = pFile->UploadedBytes();
		uint64 uLeft = pFile->IsComplete() ? 0 : pFile->GetFileSize() - pFile->DownloadedBytes();

		foreach(const QString& Url, pTracking->Trackers.uniqueKeys())
		{
			CTrackerClient* pTracker = pTracking->Trackers.value(Url);
			if(pTracker->IsBusy())
				continue;
			//if(pTracker->HasError())
			//	continue;

			if(!pTracker->ChkStarted())
				pTracker->Announce(CTrackerClient::eStarted,	uUploaded, uDownloaded, uLeft);
			else if(pFile->IsComplete() && !pTracker->ChkCompleted())
				pTracker->Announce(CTrackerClient::eCompleted,	uUploaded, uDownloaded, uLeft);
			else
				pTracker->Announce(CTrackerClient::eNone,		uUploaded, uDownloaded, uLeft);
		}
	}
}

bool CTorrentManager::IsFirewalled(CAddress::EAF eAF, bool bUTP) const
{
	return m_FWWatch.IsFirewalled(eAF, bUTP ? CFWWatch::eUTP : CFWWatch::eTCP);
}

CAddress CTorrentManager::GetAddress(CAddress::EAF eAF) const
{
	if(m_pDHT && eAF == CAddress::IPv4)
	{
		CAddress Address = m_pDHT->GetAddress();
		if(!Address.IsNull())
			return Address;
	}
	CAddress Address = m_Server->GetAddress(eAF);
	if(eAF == CAddress::IPv6 && Address.IsNull())
		Address = theCore->m_Network->GetAddress(CAddress::IPv6);
	return Address;
}

int CTorrentManager::GetIPv6Support() const
{
	if(!m_Server->HasIPv6())
		return 0; // No Support
	if(!GetAddress(CAddress::IPv6).IsNull())
		return 1; // Confirmed Support
	return 2; // we dont know
}

void CTorrentManager::AddDHTNode(const CAddress& Address, uint16 Port)
{
	if(m_pDHT && Address.Type() == CAddress::IPv4)
		m_pDHT->AddNode(SPeer(Address, Port));
}

QString CTorrentManager::GetDHTStatus(CTorrent* pTorrent, uint64* pNext) const
{
	if(!m_bEnabled)
		return "disabled";
	if(STorrentTracking* pTracking = m_TorrentTracking.value(pTorrent))
	{
		uint64 uNow = GetCurTick();
		if(pNext && pTracking->NextDHTAnounce > uNow)
			*pNext = pTracking->NextDHTAnounce - uNow;
		return "ok";
	}
	return "";
}

void CTorrentManager::OnConnection(CStreamSocket* pSocket)
{
	if(!m_bEnabled 
	 || (pSocket->GetAddress().Type() == CAddress::IPv6 && GetIPv6Support() == 0) 
	 || !theCore->m_PeerWatch->CheckPeer(pSocket->GetAddress(), 0, true)
	 || pSocket->GetState() != CStreamSocket::eIncoming)
	{
		m_Server->FreeSocket(pSocket);
		return;
	}
	CTorrentClient* pClient = new CTorrentClient((CTorrentSocket*)pSocket, this);
	AddConnection(pClient, true);
}

void CTorrentManager::OnPeersFound(QByteArray InfoHash, TPeerList PeerList)
{
	bool bDHT = (sender() == m_pDHT);

	CTorrent* pTorrent = m_TorrentTrackingRev.value(InfoHash);
	if(!pTorrent)
		return;

	foreach(const SPeer& FoundPeer, PeerList)
	{
		STorrentPeer Peer;
		Peer.SetIP(FoundPeer.Address);
		Peer.Port = FoundPeer.Port;
		if(FoundPeer.Flags != 0xFF)
			Peer.ConOpts.Bits = FoundPeer.Flags;
		AddToFile(pTorrent, Peer, bDHT ? eDHT : eTracker);
	}
}

void CTorrentManager::OnEndLookup(QByteArray InfoHash)
{
	bool bConnectedDHT = m_DHTStatus["Nodes"].toInt() > 10; // BT-DoDo: Improve This;
	
	CTorrent* pTorrent = m_TorrentTrackingRev.value(InfoHash);
	if(STorrentTracking* pTracking = m_TorrentTracking.value(pTorrent))
	{
		if(!bConnectedDHT)
			pTracking->NextDHTAnounce = 0; // do that on timer
	}
}

void CTorrentManager::AddConnection(CTorrentClient* pClient, bool bPending)
{
	m_Connections.append(pClient); 
	if(bPending) 
		m_Pending.append(pClient);
	else{
		ASSERT(pClient->GetTorrentPeer() != NULL);
	}
}

void CTorrentManager::RemoveConnection(CTorrentClient* pClient)	
{
	m_Connections.removeOne(pClient); 
	m_Pending.removeOne(pClient);
}

int CTorrentManager::GetConnectionCount()
{
	//return m_Connections.size();
	return m_Server->GetSocketCount();
}

bool CTorrentManager::MakeRendezvous(CTorrentClient* pClient)
{
	CTorrentPeer* pPeer = pClient->GetTorrentPeer();
	if(!pPeer)
		return false;
	ASSERT(pPeer);
	CFile* pFile = pPeer->GetFile();
	ASSERT(pFile);

	QList<CTorrentClient*> Peers;
	foreach (CTransfer* pTransfer, pFile->GetTransfers()) 
	{
		if(CTorrentPeer* pCurPeer = qobject_cast<CTorrentPeer*>(pTransfer))
		{
			CTorrentClient* pCurClient = pCurPeer->GetClient();

			if(!pCurClient || pCurClient == pClient)
				continue;

			if(pCurClient->GetSocket()->GetAddress().Type() != pClient->GetSocket()->GetAddress().Type())
				continue;

			if(pCurClient->SupportsHolepunch())
				Peers.append(pCurClient);
		}
	}
	if(Peers.isEmpty())
		return false;

	const STorrentPeer& Peer = pClient->GetPeer();
	CTorrentClient* pSelClient = Peers.at(qrand()%Peers.size());
	pSelClient->SendHolepunch(CTorrentClient::eHpRendezvous, Peer.GetIP(), Peer.Port);
	return true;
}

bool CTorrentManager::DispatchClient(CTorrentClient* pClient)
{
	if(!m_Connections.contains(pClient))
		return false; // this one has ben

	CTorrent* pTorrent = m_TorrentTrackingRev.value(pClient->GetInfoHash());
	if(!pTorrent)
		return false;

	CFile* pFile = pTorrent->GetFile();
	if(!pFile->IsStarted())
		return false;
	
	if(GetClientID() == pClient->GetClientID())
		return false;

	//bool byID = false;
	CTorrentPeer* pFoundPeer = NULL;
    foreach (CTransfer* pTransfer, pFile->GetTransfers())
	{
		if(CTorrentPeer* pPeer = qobject_cast<CTorrentPeer*>(pTransfer))
		{
			if(pPeer == pClient->GetTorrentPeer())
				continue;

			if(pPeer->GetPeer() == pClient->GetPeer())
				pFoundPeer = pPeer;
			if(pPeer->GetClientID() == pClient->GetClientID())
			{
				pFoundPeer = pPeer;
				//byID = true;
				break;
			}
		}
	}

	if(!pClient->GetTorrentPeer())
	{
		if(pFoundPeer && pFoundPeer->IsConnected()) // this checks only if there is a client
		{
//#ifdef _DEBUG
//		LogLine(LOG_DEBUG, tr("A Torrent peer connected to us while we ware still connected to it %1").arg(byID ? "by ID" : "by IP"));
//#endif
			if(pFoundPeer->GetClient()->IsConnected())
				return false; // if we really are connected deny the incomming connection
			pFoundPeer->DetacheClient();
		}

		if(!pFoundPeer)
		{
			pFoundPeer = new CTorrentPeer(pTorrent);
			pFoundPeer->SetFoundBy(eSelf);
			pFile->AddTransfer(pFoundPeer);
		}
		m_Pending.removeOne(pClient);

		pFoundPeer->AttacheClient(pClient);
	}
	else if(pFoundPeer)
	{
		// ToDo: Merge Peer Infos
		//ASSERT(byID);
		if(!pFoundPeer->IsConnected())
			pFile->RemoveTransfer(pFoundPeer);
		else
			return false;
	}
	return true;
}

CTorrentClient* CTorrentManager::FindClient(const CAddress& Address, uint16 uPort)
{
	foreach (CTorrentClient* pCurClient, m_Connections)
	{
		if(uPort == pCurClient->GetPeer().Port && pCurClient->GetPeer().CompareTo(Address))
			return pCurClient;
	}
	return NULL;
}

void CTorrentManager::AddToFile(CTorrent* pTorrent, const STorrentPeer& Peer, EFoundBy FoundBy)
{
	CFile* pFile = pTorrent->GetFile();
	if(!pFile->IsStarted())
		return; 

	if(!theCore->m_PeerWatch->CheckPeer(Peer.GetIP(), Peer.Port))
		return;

	if(Peer.ConOpts.Fields.IsSeed && pFile->IsComplete(true))
		return; // we are seed, he is seed, don't connect

	foreach (CTransfer* pTransfer, pFile->GetTransfers()) 
	{
		if(CTorrentPeer* pPeer = qobject_cast<CTorrentPeer*>(pTransfer))
		{
			if(!Peer.ID.IsEmpty() && pPeer->GetClientID() == Peer.ID)
				return;
			if(pPeer->GetPeer() == Peer)
			{
				//if(FoundBy != ePEX) // do not reset on pex
				//	pPeer->ResetNextConnect(); // make cure we can connect imminetly and dont have to wait for the next retry
				return;
			}
		}
	}

	CTorrentPeer* pNewPeer = new CTorrentPeer(pTorrent, Peer);
	pNewPeer->SetFoundBy(FoundBy);
	pFile->AddTransfer(pNewPeer);
}

QString CTorrentManager::GetTrackerStatus(CTorrent* pTorrent, const QString& Url, uint64* pNext)
{
	if(!m_bEnabled)
		return "disabled";
	if(STorrentTracking* pTracking = m_TorrentTracking.value(pTorrent))
	{
		foreach(CTrackerClient* pTracker, pTracking->Trackers)
		{
			if(pTracker->GetUrl() == Url)
			{
				if(pNext)
					*pNext = pTracker->NextAnnounce() * 1000;
				return pTracker->HasError() ? ("Error: " + pTracker->GetError()) : "ok";
			}
		}
	}
	return "";
}

void CTorrentManager::RegisterInfoHash(const QByteArray& InfoHash)
{
	m_Server->AddInfoHash(InfoHash);
}

uint64 CTorrentManager::GrabTorrent(const QByteArray& FileData, const QString& FileName)
{
	uint64 GrabberID = theCore->m_FileGrabber->GrabUris(QStringList("file://" + FileName));
	ASSERT(GrabberID);

	CFile* pFile = new CFile();
	pFile->SetPending();
	theCore->m_FileGrabber->AddFile(pFile, GrabberID); // this must not fail
	if(pFile->AddTorrentFromFile(FileData))
		pFile->Start();
	else
	{
		LogLine(LOG_ERROR, tr("The torrent file %1 cannot not be parsed.").arg(FileName));

		pFile->Remove(true);

		theCore->m_FileGrabber->Clear(true, GrabberID);
		return 0;
	}
	return GrabberID;
}

void CTorrentManager::OnBytesWritten(qint64 Bytes)
{
	m_TransferStats.AddUpload(Bytes);
	theCore->m_Network->OnBytesWritten(Bytes);
}

void CTorrentManager::OnBytesReceived(qint64 Bytes)
{
	m_TransferStats.AddDownload(Bytes);
	theCore->m_Network->OnBytesReceived(Bytes);
}

//////////////////////////////////////////////////

void CTorrentManager::OnRequestCompleted()
{
	CHttpSocket* pRequest = (CHttpSocket*)sender();
	ASSERT(pRequest->GetState() == CHttpSocket::eHandling);
	QString Path = pRequest->GetPath();
	TArguments Cookies = GetArguments(pRequest->GetHeader("Cookie"));
	TArguments Arguments = GetArguments(pRequest->GetQuery().mid(1),'&');

	pRequest->SetHeader("Access-Control-Allow-Origin", "*");

	switch(pRequest->GetType())
	{
		case CHttpSocket::eDELETE:
			pRequest->RespondWithError(501);
		case CHttpSocket::eHEAD:
		case CHttpSocket::eOPTIONS:
			pRequest->SendResponse();
			return;
	}

	if(!CWebRoot::TestLogin(pRequest))
	{
		pRequest->SetHeader("Location", QString("/Login" + Path).toUtf8());
		pRequest->RespondWithError(303);
	}
	else if(Path.left(9).compare("/Torrent/") == 0)
	{
		if(Arguments.contains("ID"))
		{
			if(CFile* pFile = CFileList::GetFile(Arguments["ID"].toULongLong()))
			{
				if(CTorrent* pTorrent = pFile->GetTopTorrent())
				{
					if(CTorrentInfo* pInfo = pTorrent->GetInfo())
					{
						pRequest->SetHeader("Content-Disposition", "attachment; filename=" + pFile->GetFileName() + ".torrent");
						pRequest->SetHeader("Content-Type", "application/octet-stream");
						pRequest->write(pInfo->SaveTorrentFile());
					}
					else
						pRequest->RespondWithError(405);
				}
				else
					pRequest->RespondWithError(405);
			}
			else
				pRequest->RespondWithError(404);
		}
		else
		{
			if(pRequest->IsPost())
			{
				QString TorrenFileName = pRequest->GetPostValue("Torrent");
				QIODevice* pTorrent = pRequest->GetPostFile("Torrent");

				uint64 GrabberID = pTorrent ? GrabTorrent(pTorrent->readAll(), TorrenFileName) : 0;

				if(Arguments["res"] == "json")
				{
					QVariantMap Return;
					Return["GrabberID"] = GrabberID;
					CIPCSocket::EEncoding Encoding = CIPCSocket::eJson;
					pRequest->write(CIPCSocket::Variant2String(Return, Encoding));
				}
				else
				{
					TArguments Variables;
					Variables["Result"] = GrabberID != 0 ? tr("Torrent added successfully:") : tr("Torrent couldn't be read properly:");
					Variables["File"] = TorrenFileName;
					pRequest->write(FillTemplate(GetTemplate(":/Templates/Torrent", "Result"),Variables).toUtf8());
				}
			}
			else
				pRequest->write(GetTemplate(":/Templates/Torrent", "Upload").toUtf8());
		}
	}
	else
		pRequest->RespondWithError(404);

	pRequest->SendResponse();
}

void CTorrentManager::OnFilePosted(QString Name, QString File, QString Type)
{
	CHttpSocket* pRequest = (CHttpSocket*)sender();
	pRequest->SetPostBuffer(); // ToDo: size limit
}

void CTorrentManager::HandleRequest(CHttpSocket* pRequest)
{
	connect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
	connect(pRequest, SIGNAL(FilePosted(QString, QString, QString)), this, SLOT(OnFilePosted(QString, QString, QString)));
}

void CTorrentManager::ReleaseRequest(CHttpSocket* pRequest)
{
	disconnect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
	disconnect(pRequest, SIGNAL(FilePosted(QString, QString, QString)), this, SLOT(OnFilePosted(QString, QString, QString)));
}
