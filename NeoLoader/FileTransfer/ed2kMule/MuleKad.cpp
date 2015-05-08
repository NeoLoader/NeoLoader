#include "GlobalHeader.h"
#include "MuleKad.h"
#include "MuleManager.h"
#include "MuleClient.h"
#include "MuleTags.h"
#include "MuleServer.h"
#include "../../FileList/FileManager.h"
#include "../../FileList/File.h"
#include "../../FileList/FileStats.h"
#include "../../NeoCore.h"
#include "../../Interface/InterfaceManager.h"
#include "../../FileSearch/Search.h"
#include "../../FileSearch/SearchManager.h"
#include "../../FileList/Hashing/FileHashTree.h"
#include "../../FileList/Hashing/FileHashSet.h"
#include "../../../Framework/Scope.h"
#include "../../../Framework/HttpServer/HttpHelper.h"
#include "../PeerWatch.h"
#include "../BitTorrent/Torrent.h"
#include "../../../Framework/RequestManager.h"
#include "../../FileList/FileDetails.h"

CMuleKad::CMuleKad(CMuleServer* pServer, QObject* qObject)
: QObjectEx(qObject)
{
	m_KadPort = 0;
	//m_UDPPort = 0;
	m_Firewalled = true;
	m_KadFirewalled = true;
	m_NextBuddyPing = 0;
	m_NextConnectionAttempt = 0;
	m_KadStatus = eDisconnected;

	m_MyBuddy = NULL;

	m_uLastLog = 0;
	m_LookupCount = 0;

	m_uNextKadFirewallRecheck = 0;

	connect(pServer, SIGNAL(ProcessKadPacket(QByteArray, quint8, CAddress, quint16, quint32, bool)), this, SLOT(ProcessKadPacket(QByteArray, quint8, CAddress, quint16, quint32, bool)));
	connect(this, SIGNAL(SendKadPacket(QByteArray, quint8, CAddress, quint16, QByteArray, quint32)), pServer, SLOT(SendKadPacket(QByteArray, quint8, CAddress, quint16, QByteArray, quint32)));
}

void CMuleKad::Process(UINT Tick)
{
	if(m_MyBuddy && IsFirewalled() && m_NextBuddyPing < GetCurTick())
	{
		m_NextBuddyPing = GetCurTick() + MIN2MS(10);
		m_MyBuddy->SendBuddyPing();
	}

	QVariantMap Request;
	Request["Firewalled"] = theCore->m_MuleManager->IsFirewalled(CAddress::IPv4, false, true);
	Request["PublicIP"] = theCore->m_MuleManager->GetAddress(CAddress::IPv4, true).ToQString();
	if(theCore->m_MuleManager->IsFirewalled(CAddress::IPv4))
	{
		if(m_MyBuddy)
		{
			Request["BuddyIP"] = m_MyBuddy->GetMule().IPv4.ToQString();
			Request["BuddyPort"] = m_MyBuddy->GetKadPort();
		}
	}

	// Note: we always advertize IPv6 addresses
	CAddress IPv6 = theCore->m_MuleManager->GetAddress(CAddress::IPv6, true);
	if(!IPv6.IsNull())
		Request["IPv6"] = IPv6.ToQString();

	if(Tick & EPerSec)
	{
		if(theCore->Cfg()->GetBool("Log/Merge"))
			SyncLog();
	}

	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "SyncState", Request).toMap();

	m_KadID = Response["KadID"].toByteArray();

	if(Response["Result"] == "Connected")
	{
		if(m_KadStatus != eConnected)
		{
			LogLine(LOG_SUCCESS, tr("MuleKad Connected, ID: %1").arg(QString(m_KadID.toHex()).toUpper()));
			m_KadStatus = eConnected;
		}
	}
	else if(Response["Result"] == "Connecting")
		m_KadStatus = eConnecting;
	else //if(Response["Result"] == "Disconnected")
		m_KadStatus = eDisconnected;

	if(m_KadStatus == eDisconnected)
	{
		if(GetCurTick() > m_NextConnectionAttempt)
		{
			LogLine(LOG_INFO, tr("Connecting emule kademlia"));
			m_NextConnectionAttempt = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/IdleTimeout"));
			StartKad();
		}
		return;
	}
	
	m_Address = CAddress(Response["PublicIP"].toString());
	m_KadPort = Response["KadPort"].toUInt(); // external kad port
	//m_UDPPort = Response["UDPPort"].toUInt(); // socket port
	m_Firewalled = Response["Firewalled"].toBool();
	m_KadFirewalled = Response["KadFirewalled"].toBool();
	
	if (m_KadFirewalled && m_uNextKadFirewallRecheck <= GetCurTick())
	{
		m_uNextKadFirewallRecheck = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/CheckFWInterval"));
		CheckFWState();
	}

	foreach(const QVariant& vQueuedFWCheck, Response["QueuedFWChecks"].toList())
	{
		QVariantMap QueuedFWCheck = vQueuedFWCheck.toMap();

		SMuleSource Mule;
		Mule.SetIP(CAddress(QueuedFWCheck["Address"].toString()));
		Mule.TCPPort = QueuedFWCheck["TCPPort"].toUInt();
		Mule.ConOpts.Bits = QueuedFWCheck["ConOpts"].toUInt(); 
		if(Mule.ConOpts.Fields.RequestsCryptLayer) // some cleints seam to mess this up
			Mule.ConOpts.Fields.SupportsCryptLayer = true;
		QByteArray UserHash = QueuedFWCheck["UserHash"].toByteArray();
		
		if(!Mule.SelectIP() || !theCore->m_PeerWatch->CheckPeer(Mule.GetIP(), Mule.TCPPort))
			continue;

		bool bAdded = false;
		CMuleClient* pClient = theCore->m_MuleManager->GetClient(Mule, &bAdded);
		if(!bAdded)
		{
			if(QueuedFWCheck["TestUDP"].toBool() == true)
				SetUDPFWCheckResult(pClient, true);
			continue;
		}

		if(QueuedFWCheck["TestUDP"].toBool() == true)
		{
			uint32 UDPKey;
			if(QueuedFWCheck.contains("UDPKey"))
				UDPKey = QueuedFWCheck["UDPKey"].toUInt();
			else
				UDPKey = theCore->m_MuleManager->GetServer()->GetUDPKey(Mule.GetIP());
			m_QueuedFWChecks.insert(pClient, SFWCheck(QueuedFWCheck["IntPort"].toUInt(), QueuedFWCheck["ExtPort"].toUInt(), UDPKey));
		}
		else
			m_QueuedFWChecks.insert(pClient, SFWCheck());

		connect(pClient, SIGNAL(HelloRecived()), this, SLOT(OnHelloRecived()));
		connect(pClient, SIGNAL(SocketClosed()), this, SLOT(OnSocketClosed()));

		pClient->SetUserHash(UserHash);
		pClient->Connect();
	}

	QList<CAddress> PendingFWChecks;
	foreach(const QVariant& vPendingFWCheck, Response["PendingFWChecks"].toList())
	{
		QVariantMap PendingFWCheck = vPendingFWCheck.toMap();
		PendingFWChecks.append(CAddress(PendingFWCheck["Address"].toString()));
	}
	theCore->m_MuleManager->GetServer()->SetExpected(PendingFWChecks);

	foreach(const QVariant& vBufferedCallback, Response["BufferedCallbacks"].toList())
	{
		QVariantMap BufferedCallback = vBufferedCallback.toMap();

		CAddress Address = CAddress(BufferedCallback["Address"].toString());
		uint16 uPort = BufferedCallback["TCPPort"].toUInt();
		QByteArray BuddyID = BufferedCallback["BuddyID"].toByteArray();
		QByteArray FileID = BufferedCallback["FileID"].toByteArray();

		if(m_MyBuddy && m_MyBuddy->IsFirewalled(CAddress::IPv4))
		{
			if(m_MyBuddy->GetBuddyID() == BuddyID)
				m_MyBuddy->RelayKadCallback(Address, uPort, BuddyID, FileID);
		}
	}

	// Note: This comes in on the normal UDP Socket, as we provide thesocket now we dont haveto pull it
	/*foreach(const QVariant& vBufferedPackets, Response["BufferedPackets"].toList())
	{
		QVariantMap BufferedPackets = vBufferedPackets.toMap();

		CAddress Address = CAddress(BufferedPackets["Address"].toString());
		uint16 uPort = BufferedPackets["UDPPort"].toUInt();
		QByteArray Data = BufferedPackets["Data"].toByteArray();
			
		CBuffer Packet((byte*)Data.data(), Data.size(), true);
		RelayUDPPacket(Address, uPort, Packet);
	}

	foreach(const QVariant& vPendingCallback, Response["PendingCallbacks"].toList())
	{
		QVariantMap PendingCallback = vPendingCallback.toMap();

		SMuleSource Mule;
		Mule.SetIP(CAddress(PendingCallback["Address"].toString()));
		Mule.TCPPort = PendingCallback["TCPPort"].toUInt();
		Mule.UserHash = PendingCallback["UserHash"].toByteArray();
		Mule.ConOpts.Bits = PendingCallback["ConOpts"].toUInt();
		if(Mule.ConOpts.Fields.RequestsCryptLayer) // some cleints seam to mess this up
			Mule.ConOpts.Fields.SupportsCryptLayer = true;
		theCore->m_MuleManager->CallbackRequested(Mule);
	}*/

	foreach(const QVariant& vPendingBuddys, Response["PendingBuddys"].toList())
	{
		if(m_MyBuddy) // if we have a buddy we are not interested in new ones
			break;

		QVariantMap PendingBuddy = vPendingBuddys.toMap();

		SMuleSource Mule;
		Mule.SetIP(CAddress(PendingBuddy["Address"].toString()));
		Mule.TCPPort = PendingBuddy["TCPPort"].toUInt();
		Mule.KadPort = PendingBuddy["KadPort"].toUInt();
		Mule.ConOpts.Bits = PendingBuddy["ConOpts"].toUInt();
		if(Mule.ConOpts.Fields.RequestsCryptLayer) // some cleints seam to mess this up
			Mule.ConOpts.Fields.SupportsCryptLayer = true;
		Mule.UserHash = PendingBuddy["UserHash"].toByteArray();
		Mule.BuddyID = PendingBuddy["BuddyID"].toByteArray();

		if(!theCore->m_PeerWatch->CheckPeer(Mule.GetIP(), Mule.TCPPort))
			continue;

		bool bAdded = false;
		CMuleClient* pClient = theCore->m_MuleManager->GetClient(Mule, &bAdded);
		if(!bAdded)
			continue; // already known clients are not viable budies

		connect(pClient, SIGNAL(HelloRecived()), this, SLOT(OnHelloRecived()));
		connect(pClient, SIGNAL(SocketClosed()), this, SLOT(OnSocketClosed()));
		if(m_PendingBuddys.contains(pClient))
			continue; // already listes
		m_PendingBuddys.append(pClient);

		if(PendingBuddy["Incoming"].toBool()) // is this a lowID client that wants us to become his buddy ans will soon connect us?
			pClient->ExpectConnection();
		else //this is a high ID client that agreed to become our buddy
			pClient->Connect();
	}

	m_KadStats = Response["Stats"].toMap();

	if(Tick & EPerSec)
	{
		if(IsConnected())
		{
			SyncFiles();
			SyncNotes();

			foreach(CAbstractSearch* pSearch, m_RunningSearches)
				SyncSearch(pSearch);
		}
	}
}

void CMuleKad::SyncFiles()
{
	QList<CFile*> Lookuped = m_SourceLookup.keys();

	QList<CFile*> FileList;

	QVariantList Files;
	foreach(CFile* pFile, CFileList::GetAllFiles())
	{
		CFileHash* pHash = pFile->GetHash(HashEd2k);
		if(pFile->IsRemoved() || !pHash || !pHash->IsValid())
			continue;

		if(pFile->IsDuplicate())
			continue;

		if(!pFile->IsStarted() || !pFile->IsEd2kShared())
			continue;

		///////////////////////////////////////////////////////////////
		// Publish File
		if(!(pFile->IsPending() || pFile->MetaDataMissing()))
		{
			FileList.append(pFile);

			QVariantMap File;
			File["FileID"] = pHash->GetHash();

			File["Shared"] = true; // EM-ToDo-Now: share only if at elast one part is complete

			File["FileSize"] = pFile->GetFileSize();
			File["FileName"] = pFile->GetFileName();

			if(CFileHash* pAICHHash = pFile->GetHash(HashMule))
				File["AICHHash"] = pAICHHash->GetHash();

			QVariantMap Tags;
			foreach(const QString& Name, QString("Title|Artist|Album|Length|Bitrate|Codec").split("|"))
			{
				QVariant Value = pFile->GetDetails()->GetProperty(Name);
				if (Value.isValid())
					Tags[Name] = Value;
			}

			File["CompleteSources"] = (uint16)pFile->GetStats()->GetAvailStats();
			File["Tags"] = Tags;

			File["Rating"] = pFile->GetProperty("Rating");
			File["Description"] = pFile->GetProperty("Description");

			Files.append(File);
		}

		///////////////////////////////////////////////////////////////
		// Find Sources
		if(!pFile->IsComplete())
		{
			Lookuped.removeOne(pFile);

			if(m_SourceLookup[pFile].uPendingLookup)
			{
				QVariantMap Request;
				Request["SearchID"] = m_SourceLookup[pFile].uPendingLookup;
				QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "GetFoundSources", Request).toMap();

				foreach(const QVariant& vSource, Response["Sources"].toList())
				{
					QVariantMap Source = vSource.toMap();

					SMuleSource Mule;

					Mule.UserHash = Source["UserHash"].toByteArray();

					Mule.SetIP(CAddress(Source["Address"].toString()));
					if(Source.contains("IPv6"))
					{
						Mule.AddIP(CAddress(Source["IPv6"].toString()));
						Mule.Prot = CAddress::IPv6;
						Mule.OpenIPv6 = true;
					}
					Mule.TCPPort = Source["TCPPort"].toUInt();
					Mule.KadPort = Source["KadPort"].toUInt();
					Mule.ConOpts.Bits = Source["ConOpts"].toUInt();
					if(Mule.ConOpts.Fields.RequestsCryptLayer) // some cleints seam to mess this up
						Mule.ConOpts.Fields.SupportsCryptLayer = true;

					if(Source["Type"].toString() == "Open")
						Mule.ClientID = Mule.IPv4.ToIPv4();
					else
					{
						Mule.ClientID = 1; // kad low ID

						Mule.BuddyID = Source["BuddyID"].toByteArray();
						Mule.BuddyAddress = CAddress(Source["BuddyIP"].toString());
						Mule.BuddyPort = Source["BuddyPort"].toUInt();
					}

					theCore->m_MuleManager->AddToFile(pFile, Mule, eKad);
				}
					
				if(Response["Finished"].toBool()) // lookup finished, shedule next
				{
					m_SourceLookup[pFile].uPendingLookup = 0;
					m_LookupCount--;
					m_SourceLookup[pFile].uNextLookup = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/KadLookupInterval"));
				}
			}
			else if(m_SourceLookup[pFile].uNextLookup < GetCurTick()
			&& pFile->GetStats()->GetTransferCount(eEd2kMule) < theCore->Cfg()->GetInt("Ed2kMule/MaxSources")
			&& m_LookupCount < theCore->Cfg()->GetInt("Ed2kMule/KadMaxLookup"))
			{
				FindSources(pFile);
			}
		}
	}

	// clear removed files
	foreach(CFile* pFile, Lookuped)
	{
		if(m_SourceLookup[pFile].uPendingLookup != 0)
			m_LookupCount--;
		m_SourceLookup.remove(pFile);
	}

	QVariantMap FileRequest;
	FileRequest["Files"] = Files;
	QVariantMap FileResponse = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "SyncFiles", FileRequest).toMap();
	foreach(const QVariant& vFile, FileResponse["Files"].toList())
	{
		QVariantMap File = vFile.toMap();
		CFile* pFile = FileList.takeFirst();
		CFileHash* pHash = pFile->GetHash(HashEd2k);
		if(!pHash || pHash->GetHash() != File["FileID"])
		{
			ASSERT(0);
			break;
		}

		pFile->SetProperty("MuleLastPubSource", QDateTime::fromTime_t(File["LastPublishedSource"].toULongLong()));
		pFile->SetProperty("MuleLastPubNote", QDateTime::fromTime_t(File["LastPublishedNote"].toULongLong()));
	}
}

void CMuleKad::SyncLog()
{
	QVariantMap Request;
	Request["LastID"] = m_uLastLog;
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "GetLog", Request).toMap();
	QVariantList Lines = Response["Lines"].toList();
	if(Lines.isEmpty())
		return;
	QVariantMap LogEntry;
	for(int i = 0; i < Lines.size(); i ++)
	{
		LogEntry = Lines.at(i).toMap();
		AddLogLine((time_t)LogEntry["Stamp"].toULongLong(), LogEntry["Flag"].toUInt() | LOG_XMOD('k') | LOG_DEBUG, LogEntry["Line"]);
	}
	m_uLastLog = LogEntry["ID"].toULongLong();
}

void CMuleKad::RecivedBuddyPing()
{
	m_NextBuddyPing = GetCurTick() + MIN2MS(10);
}

void CMuleKad::StartKad()
{
	m_Address = CAddress();
	m_KadPort = 0;
	m_Firewalled = true;
	m_KadFirewalled = true;
	m_KadStatus = eConnecting;

	QVariantMap Request;
	Request["TCPPort"] = theCore->m_MuleManager->GetServer()->GetPort();
	Request["UDPPort"] = theCore->m_MuleManager->GetServer()->GetUTPPort();
	Request["UserHash"] = theCore->m_MuleManager->GetUserHash().ToArray();

	Request["EnableLanMode"] = theCore->Cfg()->GetBool("Ed2kMule/LanMode");
	Request["SupportsCryptLayer"] = theCore->m_MuleManager->SupportsCryptLayer();
	Request["RequestsCryptLayer"] = theCore->m_MuleManager->RequestsCryptLayer();
	Request["RequiresCryptLayer"] = theCore->m_MuleManager->RequiresCryptLayer();
	Request["SupportsNatTraversal"] = theCore->m_MuleManager->SupportsNatTraversal();

	QVariantMap Result = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "Connect", Request).toMap();

	theCore->m_MuleManager->GetServer()->SetKadCrypto(Result["KadID"].toByteArray(), Result["UDPKey"].toInt());
}

void CMuleKad::StopKad()
{
	m_Address = CAddress();
	m_KadPort = 0;
	m_Firewalled = true;
	m_KadFirewalled = true;
	m_KadStatus = eDisconnected;

	QVariantMap Request;
	theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "Disconnect", Request);
}

void CMuleKad::AddNode(const CAddress& Address, uint16 uKadPort)
{
	QVariantMap Request;
	Request["Address"] = Address.ToQString();
	Request["KadPort"] = uKadPort;

	theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "AddNode", Request);
}

bool CMuleKad::RequestKadCallback(CMuleClient* pClient, CFile* pFile)
{
	ASSERT(pClient->GetBuddyID().size() == 16);
	
	CFileHash* pHash = pFile->GetHash(HashEd2k);
	ASSERT(pHash);

	QVariantMap Request;
	Request["BuddyID"] = pClient->GetBuddyID();
	Request["FileID"] = pHash->GetHash();
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "RequestKadCallback", Request).toMap();
	return Response["Result"].toString() == "Done"; // or "Wait"
}

void CMuleKad::RelayUDPPacket(const CAddress& Address, uint16 uUDPPort, const CBuffer& Packet)
{
	if(m_MyBuddy && m_MyBuddy->IsFirewalled(CAddress::IPv4))
	{
		ASSERT(m_MyBuddy);
		QByteArray BuddyID = Packet.ReadQData(16);
		if(m_MyBuddy->GetBuddyID() == BuddyID)
		{
			LogLine(LOG_DEBUG, tr("RelayReaskCallback"));

			CBuffer AuxPacket;
			AuxPacket.WriteValue<uint8>(OP_REASKCALLBACKTCP);
			if(Address.Type() == CAddress::IPv4)
				AuxPacket.WriteValue<uint32>(_ntohl(Address.ToIPv4()));
			else if(Address.Type() == CAddress::IPv6)
			{
				AuxPacket.WriteValue<uint32>(-1);
				AuxPacket.WriteData(Address.Data(), 16);
			}
			AuxPacket.WriteValue<uint16>(uUDPPort);
			AuxPacket.WriteData(Packet.GetData(0), Packet.GetSizeLeft());
			m_MyBuddy->SendPacket(AuxPacket, OP_EMULEPROT);
		}
	}
}

bool CMuleKad::IsEnabled() const
{
	return theCore->m_Interfaces->IsInterfaceEstablished("MuleKad");
}

void CMuleKad::OnHelloRecived()
{
	CMuleClient* pClient = (CMuleClient*)sender();

	if(m_PendingBuddys.contains(pClient))
	{
		ASSERT(m_MyBuddy == NULL);
		if(IsFirewalled() == pClient->IsFirewalled(CAddress::IPv4))
			return;

		m_PendingBuddys.clear();
		m_MyBuddy = pClient;
		pClient->DisableTimedOut();
		m_NextBuddyPing = GetCurTick();
	}
	else if(m_QueuedFWChecks.contains(pClient))
	{
		SFWCheck& Check = m_QueuedFWChecks[pClient];
		if(Check.bTestUDP)
		{
			if(pClient->KadVersion() <= KADEMLIA_VERSION5_48a || pClient->GetKadPort() == 0)
				SetUDPFWCheckResult(pClient, true);
			else
				pClient->RequestFWCheckUDP(Check.uIntPort, Check.uExtPort, Check.uUDPKey);
		}
		else
		{
			if (pClient->KadVersion() >= KADEMLIA_VERSION7_49a)
				pClient->SendFWCheckACK();
			else
				SendFWCheckACK(pClient);
		}
	}

	disconnect(pClient, SIGNAL(HelloRecived()), this, SLOT(OnHelloRecived()));
}

void CMuleKad::OnSocketClosed()
{
	CMuleClient* pClient = (CMuleClient*)sender();

	if(m_QueuedFWChecks.contains(pClient))
	{
		SFWCheck& Check = m_QueuedFWChecks[pClient];
		if(Check.bTestUDP)
			SetUDPFWCheckResult(pClient, pClient->HasError());
	}

	disconnect(pClient, SIGNAL(SocketClosed()), this, SLOT(OnSocketClosed()));
}

bool CMuleKad::FindSources(CFile* pFile)
{
	if(!pFile->IsEd2kShared())
		return true; // finished without error
	if(!IsConnected())
		return false;

	CFileHash* pHash = pFile->GetHash(HashEd2k);
	if(!pHash)
		return false;

	if(!m_SourceLookup[pFile].uPendingLookup)
	{
		QVariantMap Request;
		Request["FileID"] = pHash->GetHash();
		Request["FileSize"] = pFile->GetFileSize();
		Request["FileName"] = pFile->GetFileName();
		QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "FindSources", Request).toMap();
		if(m_SourceLookup[pFile].uPendingLookup = Response["SearchID"].toUInt())
			m_LookupCount++;
	}
	return true;
}

bool CMuleKad::ResetPub(CFile* pFile)
{
	if(!pFile->IsEd2kShared())
		return true; // finished without error
	if(!IsConnected())
		return false;

	CFileHash* pHash = pFile->GetHash(HashEd2k);
	if(!pHash)
		return false;

	QVariantMap Request;
	Request["FileID"] = pHash->GetHash();
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "ResetPub", Request).toMap();
	return true;
}

void CMuleKad::CheckFWState()
{
	QVariantMap Request;
	theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "CheckFWState", Request);
}

void CMuleKad::SetUDPFWCheckResult(CMuleClient* pClient, bool bCanceled)
{
	QVariantMap Request;
	Request["State"] = bCanceled ? "Cancelled" : "Failed"; // "Succeeded"
	Request["Address"] = pClient->GetMule().IPv4.ToQString();
	theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "SetUDPFWCheckResult", Request);
}

void CMuleKad::FWCheckUDPRequested(CMuleClient* pClient, uint16 IntPort, uint16 ExtPort, uint32 UDPKey)
{
	QVariantMap Request;
	Request["Address"] = pClient->GetMule().IPv4.ToQString();
	Request["IntPort"] = IntPort; 
	Request["ExtPort"] = ExtPort; 
	Request["UDPKey"] = UDPKey;
	Request["Biased"] = pClient->GetAllSources().isEmpty() == false;
	theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "FWCheckUDPRequested", Request);
}

void CMuleKad::FWCheckACKRecived(CMuleClient* pClient)
{
	QVariantMap Request;
	Request["Address"] = pClient->GetMule().IPv4.ToQString();
	theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "FWCheckACKRecived", Request);
}

void CMuleKad::SendFWCheckACK(CMuleClient* pClient)
{
	QVariantMap Request;
	Request["Address"] = pClient->GetMule().IPv4.ToQString();
	Request["KadPort"] = pClient->GetKadPort(); 
	theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "SendFWCheckACK", Request);
}

void CMuleKad::RemoveClient(CMuleClient* pClient)
{
	m_PendingBuddys.removeOne(pClient);
	m_QueuedFWChecks.remove(pClient);
	if(pClient == m_MyBuddy)
		m_MyBuddy = NULL;
}

CMuleClient* CMuleKad::GetBuddy(bool Booth)
{
	if(!m_MyBuddy)
		return NULL;
	if(m_MyBuddy->IsFirewalled(CAddress::IPv4) && !Booth) 
		return NULL;
	return m_MyBuddy; 
}

void CMuleKad::StartSearch(CAbstractSearch* pSearch)
{
	ASSERT(!pSearch->IsRunning());

	if(pSearch->GetExpression().isEmpty())
		return;

	QVariantMap Request;
	Request["Expression"] = pSearch->GetExpression(); // depricated

	CScoped<SSearchTree> pSearchTree = CSearchManager::MakeSearchTree(pSearch->GetExpression());
	if(!pSearchTree)
	{
		pSearch->SetError("Invalie Search Query");
		return;
	}
	Request["SearchTree"] = pSearchTree->ToVariant();

	foreach(const QString& Name, pSearch->GetAllCriterias())
	{
		if(Name == "Ed2kFileType")
			Request["FileType"] = pSearch->GetCriteria(Name);
		else if(Name == "FileExt" || Name == "MinSize" || Name == "MaxSize" || Name == "Availability")
			Request[Name] = pSearch->GetCriteria(Name);
		// else unsupported
	}
	
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "FindFiles", Request).toMap();
	
	if(Response.contains("Error"))
		pSearch->SetError(Response["Error"].toString());
	else if(uint32 SearchID = Response["SearchID"].toUInt())
	{
		pSearch->SetStarted(SearchID);
		m_RunningSearches.insert(pSearch);
	}
	else
		pSearch->SetError("Internal Error");
}

void CMuleKad::SyncSearch(CAbstractSearch* pSearch)
{
	ASSERT(pSearch->IsRunning());

	QVariantMap Request;
	Request["SearchID"] = pSearch->GetSearchID();
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "GetFoundFiles", Request).toMap();

	foreach(const QVariant& vFile, Response["Files"].toList())
	{
		QVariantMap File = vFile.toMap();
		QVariantMap Tags = File["Tags"].toMap();

		uint64 uFileSize = File["FileSize"].toULongLong();
		CScoped<CFileHashSet> pHashSet = new CFileHashSet(HashEd2k, uFileSize);
		if(uFileSize == 0 || !pHashSet->SetHash(File["FileID"].toByteArray()))
			continue;

		CFile* pFile = new CFile();
		pFile->AddHash(CFileHashPtr(pHashSet.Detache()));
		pFile->AddEmpty(HashEd2k, File["FileName"].toString(), uFileSize);

		QVariantMap Details;
		Details["Name"] = File["FileName"].toString();

		for(QVariantMap::iterator I = Tags.begin(); I != Tags.end(); ++I)
		{
			const QString& Name = I.key();
			const QVariant& Value = I.value();
			if(Name == "AICHHash")
			{
				CFileHashPtr pHashTree = CFileHashPtr(new CFileHashTree(HashMule, pFile->GetFileSize()));
				if(pHashTree->SetHash(Value.toByteArray()))
					pFile->AddHash(pHashTree);
			}
			else if(Name == "Artist")	Details["Artist"] = Value.toString();
			else if(Name == "Album")	Details["Album"] = Value.toString();
			else if(Name == "Title")	Details["Title"] = Value.toString();
			else if(Name == "Length")	Details["Length"] = Value.toInt();
			else if(Name == "Bitrate")	Details["Bitrate"] = Value.toInt();
			else if(Name == "Codec")	Details["Codec"] = Value.toString();
		}

		Details["Availability"] = File["Availability"]; //.toUInt();
		Details["MuleDifferentNames"] = File["DifferentNames"]; //.toUInt();
		Details["MulePublishersKnown"] = File["PublishersKnown"]; //.toUInt();
		Details["MuleTrustValue"] = File["TrustValue"]; //.toUInt();

		pFile->GetDetails()->Add("kad://search_" + GetRand64Str(), Details);
		pSearch->AddFoundFile(pFile);
	}

	if(Response["Finished"].toBool())
	{
		pSearch->SetStopped();
		m_RunningSearches.remove(pSearch);
	}
}

void CMuleKad::StopSearch(CAbstractSearch* pSearch)
{
	ASSERT(pSearch->IsRunning());

	QVariantMap Request;
	Request["SearchID"] = pSearch->GetSearchID();
	theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "StopSearch", Request);

	pSearch->SetStopped();
	m_RunningSearches.remove(pSearch);
}

uint32 CMuleKad::FindNotes(const QByteArray& Hash, uint64 Size, const QString& Name)
{
	QVariantMap Request;
	Request["FileID"] = Hash;
	Request["FileSize"] = Size;
	Request["FileName"] = Name;
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "FindNotes", Request).toMap();
	return Response["SearchID"].toUInt();
}

bool CMuleKad::GetFoundNotes(uint32 SearchID, QVariantList& Notes)
{
	QVariantMap Request;
	Request["SearchID"] = SearchID;
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("MuleKad", "GetFoundNotes", Request).toMap();

	Notes = Response["Notes"].toList();

	return Response["Finished"].toBool();
}

bool CMuleKad::FindNotes(CFile* pFile)
{
	uint64 FileID = pFile->GetFileID();
	if(m_NoteLookup.contains(FileID))
	{
		LogLine(LOG_ERROR, tr("Note lookup for %1 isalready in progress").arg(pFile->GetFileName()));
		return false;
	}

	CFileHash* pHash = pFile->GetHash(HashEd2k);
	if(!pHash)
		return false;

	uint32 SearchID = FindNotes(pHash->GetHash(), pFile->GetFileSize(), pFile->GetFileName());
	if(!SearchID)
	{
		LogLine(LOG_ERROR | LOG_DEBUG, tr("Failed to start Note lookup for %1").arg(pFile->GetFileName()));
		return false;
	}

	m_NoteLookup[FileID] = SearchID;
	return true;
}

bool CMuleKad::IsFindingNotes(CFile* pFile)
{
	return m_NoteLookup.contains(pFile->GetFileID());
}

void CMuleKad::SyncNotes()
{
	foreach(uint64 FileID, m_NoteLookup.keys())
	{
		bool bDone;
		QVariantList Notes;
		if(bDone = GetFoundNotes(m_NoteLookup[FileID], Notes))
			m_NoteLookup.remove(FileID); // note search finished

		if(CFile* pFile = CFileList::GetFile(FileID))
		{
			foreach(const QVariant& vNote, Notes)
			{
				QVariantMap Note;
				Note["Name"] = Note["FileName"].toString();
				Note["Rating"] = Note["Rating"].toUInt();
				Note["Description"] = Note["Description"].toString();
				pFile->GetDetails()->Add("kad://" + Note["SourceID"].toByteArray().toHex(), Note);
			}

			if(bDone)
				pFile->SetProperty("MuleLastFindNotes", (uint64)GetTime());
		}
	}
}

QString	CMuleKad::GetStatus(CFile* pFile, uint64* pNext) const
{
	QString Status;
	if(!IsEnabled())
		Status = "Disabled";
	else if(IsConnected())
	{
		Status = "Connected";

		if(pFile)
		{
			QDateTime LastPub = pFile->GetProperty("MuleLastPubSource").toDateTime();
			time_t uLastPub = LastPub.isValid() ? LastPub.toTime_t() : 0;
			if(uLastPub == 0)
				Status += ", Published";

			if(pNext && m_SourceLookup.contains(pFile))
			{
				const SSrcLookup& Lookup = m_SourceLookup[pFile];
				uint64 uNow = GetCurTick();
				if(Lookup.uNextLookup > uNow)
					*pNext = Lookup.uNextLookup - uNow;
			}
		}
	}
	else if(IsDisconnected())
		Status = "Disconnected";
	else
		Status = "Connecting";
	return Status;
}

void CMuleKad::OnNotificationRecived(const QString& Command, const QVariant& Parameters)
{
	//CIPCClient*	pClient = (CIPCClient*)sender();
	if(Command == "SendUDP")
	{
		QVariantMap Request	= Parameters.toMap();

		quint16 SendPort= Request["SendPort"].toUInt();

		quint8 Prot			= Request["Prot"].toUInt();
		QByteArray Data		= Request["Data"].toByteArray();
		
		CAddress Address= CAddress(Request["Address"].toString());
		quint16 KadPort		= Request["KadPort"].toUInt();
		QByteArray NodeID	= Request["KadID"].toByteArray();
		quint32 UDPKey		= Request["UDPKey"].toUInt();

		emit SendKadPacket(Data, Prot, Address, KadPort, NodeID, UDPKey);
	}
}

void CMuleKad::ProcessKadPacket(QByteArray Packet, quint8 Prot, CAddress Address, quint16 uKadPort, quint32 UDPKey, bool bValidKey)
{
	QVariantMap Request;
	Request["RecvPort"] = theCore->m_MuleManager->GetServer()->GetUTPPort();
	Request["Prot"] = Prot;
	Request["Data"] = Packet;
	Request["Address"] = Address.ToQString();
	Request["KadPort"] = uKadPort;
	if(UDPKey != 0)
		Request["UDPKey"] = UDPKey;
	if(bValidKey)
		Request["ValidKey"] = true;

	theCore->m_Interfaces->SendNotification("MuleKad", "ProcessUDP", Request);
}