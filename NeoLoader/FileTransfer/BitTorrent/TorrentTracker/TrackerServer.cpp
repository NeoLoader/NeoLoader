#include "GlobalHeader.h"
#include "../../../../qbencode/lib/bencode.h"
#include "TrackerServer.h"
#include "../../../NeoCore.h"
#include "../../../../Framework/HttpServer/HttpSocket.h"
#include "../../../FileList/FileManager.h"
#include "../../../../Framework/Buffer.h"
#include "../../../FileList/File.h"
#include "../TorrentPeer.h"

CTrackerServer::CTrackerServer(QObject* qObject)
: QObjectEx(qObject)
{
	theCore->m_HttpServer->RegisterHandler(this,"/Torrent/announce");
}

void CTrackerServer::OnRequestCompleted()
{
	CHttpSocket* pRequest = (CHttpSocket*)sender();
	ASSERT(pRequest->GetState() == CHttpSocket::eHandling);
	
	switch(pRequest->GetType())
	{
		case CHttpSocket::eDELETE:
			pRequest->RespondWithError(501);
		case CHttpSocket::eHEAD:
		case CHttpSocket::eOPTIONS:
			pRequest->SendResponse();
			return;
	}

	if(theCore->Cfg()->GetInt("BitTorrent/EnableTracker"))
	{
		TArguments Arguments = GetArguments(pRequest->GetQuery().mid(1),'&');
		
		QString Error;

		QByteArray InfoHash = QByteArray::fromPercentEncoding(Arguments["info_hash"].toLatin1());
		if(InfoHash.size() != 20)
			Error = "invalid infohash";
		else if(theCore->Cfg()->GetInt("BitTorrent/EnableTracker") == 2)
		{
			CFileHash Hash(HashTorrent);
			Hash.SetHash(InfoHash);
			if(theCore->m_FileManager->GetFileByHash(&Hash) == NULL)
				Error = "unauthorized infohash";
		}

		if(Arguments["compact"].toUInt() != 1)
			Error = "only compact format is supported";

		QVariantMap Dict;
		Dict["interval"] = theCore->Cfg()->GetInt("BitTorrent/AnnounceInterval");

		if(!Error.isEmpty())
			Dict["failure reason"] = Error;
		else
		{
			STorrentPeerX CurPeer;
			CurPeer.ID = QByteArray::fromPercentEncoding(Arguments["peer_id"].toLatin1());
			CurPeer.SetIP(CAddress(pRequest->GetAddress().toString()));
			CurPeer.Port = Arguments["port"].toUInt();
			//Arguments["supportcrypto"]
			CurPeer.LastSeen = GetTime();

			QList<STorrentPeerX>& PeerList = m_PeerList[InfoHash];
			if(Arguments["event"] == "stopped")
				PeerList.removeAll(CurPeer);
			else
			{
				int Index = PeerList.indexOf(CurPeer);
				if(Index != -1)
					((STorrentPeerX&)PeerList.at(Index)) = CurPeer;
				else
					PeerList.append(CurPeer);

				int Wanted = Arguments["numwant"].toUInt();

				CBuffer Peers;
				CBuffer Peers6;
				for(int i=0; i < Min(Wanted, PeerList.size()); i++)
				{
					STorrentPeerX Peer = PeerList.at(i);
					if(GetTime() - Peer.LastSeen > MIN2S(60))
					{
						PeerList.removeAt(i--);
						continue;
					}

					if(Peer.HasV6())
					{
						Peers6.WriteData(Peer.IPv6.Data(), 16);
						Peers6.WriteValue<uint16>(Peer.Port, true);
						//Flags6.WriteValue<uint8>(Peer.Flags, true);
					}
					else if(Peer.HasV4())
					{
						Peers.WriteValue<uint32>(Peer.IPv4.ToIPv4(), true);
						Peers.WriteValue<uint16>(Peer.Port, true);
						//Flags.WriteValue<uint8>(Peer.Flags, true);
					}
				}

				Dict["peers"] = Peers.ToByteArray();
				Dict["peers6"] = Peers6.ToByteArray();
			}
		}

		pRequest->write(Bencoder::encode(Dict).buffer());
	}
	else
		pRequest->RespondWithError(404);

	pRequest->SendResponse();
}

void CTrackerServer::HandleRequest(CHttpSocket* pRequest)
{
	connect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
}

void CTrackerServer::ReleaseRequest(CHttpSocket* pRequest)
{
	disconnect(pRequest, SIGNAL(readChannelFinished()), this, SLOT(OnRequestCompleted()));
}
