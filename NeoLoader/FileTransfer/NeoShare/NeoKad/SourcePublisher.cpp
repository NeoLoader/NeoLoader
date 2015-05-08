#include "GlobalHeader.h"
#include "SourcePublisher.h"
#include "../../../NeoCore.h"
#include "../../../FileTransfer/NeoShare/NeoKad.h"
#include "../../../FileTransfer/NeoShare/NeoManager.h"
#include "../../../../Framework/Cryptography/HashFunction.h"
#include "../../../FileList/Hashing/FileHash.h"
#include "../../../FileTransfer/BitTorrent/Torrent.h"
#include "../../../FileTransfer/BitTorrent/TorrentManager.h"
#include "../../../FileTransfer/BitTorrent/TorrentPeer.h"
#include "../../../FileTransfer/BitTorrent/TorrentServer.h"
#include "../../../FileTransfer/ed2kMule/MuleManager.h"
#include "../../../FileTransfer/ed2kMule/MuleKad.h"
#include "../../../FileTransfer/ed2kMule/MuleSource.h"
#include "../../../FileTransfer/ed2kMule/MuleServer.h"
#include "../../../FileTransfer/NeoShare/NeoManager.h"
#include "../../../FileTransfer/NeoShare/NeoEntity.h"
#include "../../../FileTransfer/NeoShare/NeoRoute.h"

CSourcePublisher::CSourcePublisher(CKadAbstract* pItf)
: CFilePublisher(pItf) 
{
}

QVariant CSourcePublisher::PublishEntrys(uint64 FileID, CFileHashPtr pHash)
{
	CKadAbstract* pItf = Itf();

	QVariantMap Request = theCore->m_NeoManager->GetKad()->GetLookupCfg(CNeoKad::ePublish);
	Request["TargetID"] = pItf->MkTargetID(pHash->GetHash());
	Request["CodeID"] = theCore->m_NeoManager->GetKad()->GetCodeID(CodeID());


	QVariantList Execute;

	QVariantMap Call;
	Call["Function"] = "announceSource";
	//Call["ID"] =; 
	QVariantMap Parameters;
	Parameters["PID"] = pItf->MkPubID(FileID); // PubID
	Parameters["HV"] = pHash->GetHash(); // HashValue
	Parameters["HF"] = CFileHash::HashType2Str(pHash->GetType()); // HashType

	//Parameters["Expiration"] = // K-ToDo-Now: set a proeprly short time, base it on scpirience in how long the client is online
	Parameters["TTL"] = (uint64)(GetTime() + DAY2S(1)); // K-ToDo-Now: set a proeprly short time, base it on expirience in how long the client is online

	QVariantList Entities;
	foreach(CNeoRoute* pRoute, theCore->m_NeoManager->GetStaticRoutes())
	{
		QVariantMap Entity;
		Entity["EID"] = pRoute->GetEntityID();
		Entity["TID"] = pRoute->GetTargetID();
		//Entity["IH"] = false; // IsHub
		Entities.append(Entity);
	}
	Parameters["NE"] = Entities;

	switch(pHash->GetType())
	{
		case HashNeo:
		case HashXNeo:
			break;
		case HashEd2k:
		{
			Parameters["UH"] = theCore->m_MuleManager->GetUserHash().ToArray(); // UserHash

			CAddress IPv4 = theCore->m_MuleManager->GetAddress(CAddress::IPv4);
			if(!IPv4.IsNull())
				Parameters["IP4"] = IPv4.ToIPv4(); // IPv4
			CAddress IPv6 = theCore->m_MuleManager->GetAddress(CAddress::IPv6);
			if(!IPv6.IsNull() && !theCore->m_MuleManager->IsFirewalled(CAddress::IPv6))
				Parameters["IP6"] = QByteArray((char*)IPv6.Data(), 16); // IPv6 - we only publish unfirewalled IPv6

			Parameters["TP"] = theCore->m_MuleManager->GetServer()->GetPort(); // TCPPort
			Parameters["UP"] = theCore->m_MuleManager->GetServer()->GetUTPPort(); // UDPPort
			Parameters["KP"] = theCore->m_MuleManager->GetKad()->GetKadPort(); // KadPort
			UMuleConOpt ConOpts;
			ConOpts.Fields.SupportsCryptLayer = theCore->m_MuleManager->SupportsCryptLayer();
			ConOpts.Fields.RequestsCryptLayer = theCore->m_MuleManager->RequestsCryptLayer();
			ConOpts.Fields.RequiresCryptLayer = theCore->m_MuleManager->RequiresCryptLayer();
			ConOpts.Fields.DirectUDPCallback = theCore->m_MuleManager->DirectUDPCallback();
			ConOpts.Fields.Reserved = 0;
			ConOpts.Fields.SupportsNatTraversal = theCore->m_MuleManager->SupportsNatTraversal();
			Parameters["CO"] = ConOpts.Bits; // ConOpts

			if(theCore->m_MuleManager->IsFirewalled(CAddress::IPv4))
			{
				if(CMuleClient* pClient = theCore->m_MuleManager->GetKad()->GetBuddy())
				{
					Parameters["BID"] = pClient->GetBuddyID(); // BuddyID
					Parameters["BIP"] = pClient->GetMule().IPv4.ToIPv4(); // BuddyIPv4
					Parameters["BP"] = pClient->GetPort(); // BuddyPort
				}
			}
			break;
		}
		case HashTorrent:
		{
			Parameters["PID"] = theCore->m_TorrentManager->GetClientID().ToArray(); // PeerID

			CAddress IPv4 = theCore->m_TorrentManager->GetServer()->GetAddress(CAddress::IPv4);
			if(!IPv4.IsNull())
				Parameters["IP4"] = IPv4.ToIPv4(); // IPv4
			CAddress IPv6 = theCore->m_TorrentManager->GetServer()->GetAddress(CAddress::IPv6);
			if(!IPv6.IsNull())
				Parameters["IP6"] = QByteArray((char*)IPv6.Data(), 16); // IPv6
			Parameters["TP"] = theCore->m_TorrentManager->GetServer()->GetPort(); // TCPPort
			STorrentPeer::UPeerConOpt ConOpt;
			ConOpt.Fields.IsSeed = pItf->IsComplete(FileID);
			ConOpt.Fields.SupportsEncryption = theCore->m_TorrentManager->SupportsCryptLayer();
			if(theCore->Cfg()->GetBool("BitTorrent/uTP"))
			{
				ConOpt.Fields.SupportsUTP = 1;
				ConOpt.Fields.SupportsHolepunch = 1;
			}
			ConOpt.Fields.Reserved = 0;
			Parameters["CO"] = ConOpt.Bits; // ConOpts
			break;
		}
	}
	Call["Parameters"] = Parameters;

	Execute.append(Call);
	Request["Execute"] = Execute;

	Request["GUIName"] = "announceSource: " + pItf->GetFileInfo(FileID).FileName; // for GUI only

	return Request;
}

bool CSourcePublisher::EntrysFound(uint64 FileID, const QVariantList& Results, bool bDone)
{
	CKadAbstract* pItf = Itf();

	QMultiMap<QString, SNeoEntity> Neos;
	QList<SMuleSource> Mules;
	QMultiMap<QByteArray, STorrentPeer> Peers;

	foreach(const QVariant& vResult, Results)
	{
		QVariantMap Result = vResult.toMap();
		if(Result.isEmpty())
			continue;

		//Result["ID"]
		QString Function = Result["Function"].toString();
		QVariantMap Return = Result["Return"].toMap();
		// K-ToDo: check if hash is really ok

		if(Function == "collectSources")
		{
			QString TypeStr = Return["HF"].toString();
			EFileHashType HashType = CFileHash::Str2HashType(TypeStr);
			QByteArray Hash = Return["HV"].toByteArray();

			if(!HashType)
				continue;

			if(theCore->Cfg()->GetBool("NeoShare/Enable"))
			{
				QString HashStr = TypeStr + ":" + CFileHash::EncodeHash(HashType, Hash);
				foreach(const QVariant& vEntity, Return["NE"].toList())
				{
					QVariantMap Entity = vEntity.toMap();
					SNeoEntity Neo;

					Neo.TargetID = Entity["TID"].toByteArray();
					Neo.EntityID = Entity["EID"].toByteArray();
					Neo.IsHub = Entity["IH"].toBool();

					Neos.insert(HashStr, Neo);
				}
			}

			switch(HashType)
			{
				case HashNeo:
				case HashXNeo:
					break;
				case HashEd2k:
				{
					if(!theCore->Cfg()->GetBool("Ed2kMule/Enable"))
						continue;

					SMuleSource Mule;
		
					Mule.UserHash = Return["UH"].toByteArray(); // UserHash

					if(Return.contains("IP4")) // IPv4
						Mule.AddIP(CAddress(Return["IP4"].toUInt())); // IPv4
					if(Return.contains("IP6")) // IPv6
						Mule.AddIP(CAddress(Return["IP6"].toByteArray().data())); // IPv6
					Mule.SelectIP();

					Mule.TCPPort = Return["TP"].toUInt(); // TCPPort
					Mule.UDPPort = Return["UP"].toUInt(); // UDPPort
					Mule.KadPort = Return["KP"].toUInt(); // KadPort
					Mule.ConOpts.Bits = Return["CO"].toUInt(); // ConOpts
					
					if(Return.contains("BID"))
					{
						Mule.ClientID = 1; // if he has a buddy he must eb firewalled

						Mule.BuddyID = Return["BID"].toByteArray(); // BuddyID
						Mule.BuddyAddress = CAddress(Return["BIP"].toUInt()); // BuddyIPv4
						Mule.BuddyPort = Return["BP"].toUInt(); // BuddyPort
					}
					else // if he published without a buddy he is a HighID
					{
						if(Mule.IPv4.IsNull()) // is this an IPv6 only source
							Mule.ClientID = 0xFFFFFFFF;
						else
							Mule.ClientID = Mule.IPv4.ToIPv4(); 
					}

					Mules.append(Mule);
					break;
				}
				case HashTorrent:
				{
					if(!theCore->Cfg()->GetBool("BitTorrent/Enable"))
						continue;

					STorrentPeer Peer;
					Peer.ID = Return["PID"].toByteArray(); // PeerID
					if(Return.contains("IP4")) // IPv4
						Peer.AddIP(CAddress(Return["IP4"].toUInt())); // IPv4
					if(Return.contains("IP6")) // IPv6
						Peer.AddIP(CAddress(Return["IP6"].toByteArray().data())); // IPv6
					Peer.Port = Return["TP"].toUInt(); // TCPPort
					Peer.SelectIP();
					Peer.ConOpts.Bits = Return["CO"].toUInt(); // ConOpts

					Peers.insert(Hash, Peer); // peers must be coded with thair infohash
					break;
				}
			}
		}
	}

	pItf->AddSources(FileID, Neos, Mules, Peers);
	return true;
}