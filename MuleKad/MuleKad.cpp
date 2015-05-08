#include "GlobalHeader.h"
#include "MuleKad.h"
#include "../Framework/IPC/IPCSocket.h"
#include "../Framework/Cryptography/AbstractKey.h"
#include "../Framework/Settings.h"
#include "../Framework/Xml.h"
#include "Kad/Types.h"
#include "Kad/KadHandler.h"
#include "Kad/UDPSocket.h"
#include "Kad/routing/Contact.h"
#include "Kad/routing/RoutingZone.h"
#include "Kad/kademlia/Search.h"
#include "Kad/kademlia/SearchManager.h"
#include "Kad/kademlia/Indexed.h"
#include "Kad/FileTags.h"
#include "Kad/kademlia/UDPFirewallTester.h"
#include <QApplication>
#include "../Framework/RequestManager.h"


CMuleKad::CMuleKad(QObject *parent)
 : CLoggerTmpl<QObject>("MuleKad", parent)
{
	m_uTimerID = startTimer(10);

	m_pBootstrap = NULL;
	m_pRequestManager = new CRequestManager(MIN2S(60), this);

	m_bEmbedded = false;
	m_uLastContact = 0;

	CSettings::InitSettingsEnvironment("Neo", "MuleKad", "neoloader.com");
	QMap<QString, CSettings::SSetting> Settings;

	Settings.insert("Kademlia/UDPPort", CSettings::SSetting(GetRandomInt(4800, 4900)));
	Settings.insert("Kademlia/KadID", CSettings::SSetting(""));
	Settings.insert("Kademlia/UDPKey", CSettings::SSetting(0));
	Settings.insert("Kademlia/Debug", CSettings::SSetting(0));
	Settings.insert("Kademlia/Bootstrap", CSettings::SSetting("http://upd.emule-security.org/nodes.dat")); // http://upd.emule-security.org/nodes.dat

	Settings.insert("Log/Store", CSettings::SSetting(false));
	Settings.insert("Log/Limit", CSettings::SSetting(256,128,1024));

	Settings.insert("Gui/LogLength", CSettings::SSetting(256,128,1024));
	Settings.insert("Gui/Window", CSettings::SSetting(""));
	Settings.insert("Gui/Main", CSettings::SSetting(""));
	Settings.insert("Gui/RoutingTable", CSettings::SSetting(""));
	Settings.insert("Gui/LookupList", CSettings::SSetting(""));


	m_Settings = new CSettings("MuleKad", Settings, this);

	m_UDPKey = m_Settings->GetInt("Kademlia/UDPKey");
	if(!m_UDPKey)
	{
		m_UDPKey = GetRand64();
		m_Settings->SetSetting("Kademlia/UDPKey",m_UDPKey);
	}

	SetLogLimit(m_Settings->GetInt("Log/Limit"));
	if(m_Settings->GetBool("Log/Store") )
		SetLogPath(m_Settings->GetSettingsDir() + "/Logs");

	QStringList Args = QApplication::instance()->arguments();

	QString Name;
	int NameIndex = Args.indexOf("-name");
	if(NameIndex != -1 && NameIndex + 1 < Args.size())
		Name = Args.at(NameIndex + 1);

	m_pInterface = new CIPCServer(this);
	if(Name.isEmpty())
		m_pInterface->LocalListen("MuleKad");
	else
		m_pInterface->LocalListen(Name);

	int ListenIndex = Args.indexOf("-listen");
	if(ListenIndex != -1 && ListenIndex + 1 < Args.size())
		m_pInterface->RemoteListen(Args.at(ListenIndex + 1).toULong());

	connect(m_pInterface, SIGNAL(RequestRecived(const QString&, const QVariant&, QVariant&)), this, SLOT(OnRequestRecived(const QString&, const QVariant&, QVariant&)));


#ifdef WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);

	if (0 != WSAStartup(wVersionRequested, &wsaData))
		LogLine(LOG_ERROR, L"Couldn't initialise windows sockets");
#endif

	g_bLogKad = m_Settings->GetInt("Kademlia/Debug") == 1;

	m_KadID = m_Settings->GetBlob("Kademlia/KadID");
	if(m_KadID.size() != 16)
	{
		m_KadID = CAbstractKey(16,true).ToByteArray();
		m_Settings->SetBlob("Kademlia/KadID",m_KadID);
	}

	m_KadHandler = NULL;
}

CMuleKad::~CMuleKad()
{
	killTimer(m_uTimerID);

	delete m_KadHandler;
}

void CMuleKad::Process()
{
	if(m_bEmbedded)
	{
		if(m_pInterface->GetClientCount() == 0)
		{
			if(m_uLastContact == 0)
				m_uLastContact = GetCurTick();
			else if(GetCurTick() - m_uLastContact > SEC2MS(30)) // ToDo-Now: Customize
				QApplication::quit();
		}
		else if(m_uLastContact)
			m_uLastContact = 0;
	}

	if(m_KadHandler)
		m_KadHandler->Process();
}

void CMuleKad::OnRequestRecived(const QString& Command, const QVariant& Parameters, QVariant& Result)
{
	QVariantMap Request = Parameters.toMap();

	QVariantMap Response;

	if(Command == "GetLog")
	{
		QList<CLog::SLine> Log = GetLog();

		int Index = -1;
		if(uint64 uLast = Request["LastID"].toULongLong())
		{
			for(int i=0;i < Log.count(); i++)
			{
				if(uLast == Log.at(i).uID)
				{
					Index = i;
					break;
				}
			}
		}

		QVariantList Entries;
		for(int i=Index+1;i < Log.count(); i++)
		{
			QVariantMap LogEntry;
			LogEntry["ID"] = Log.at(i).uID;
			LogEntry["Flag"] = Log.at(i).uFlag;
			LogEntry["Stamp"] = (quint64)Log.at(i).uStamp;
			LogEntry["Line"] = Log.at(i).Line;
			Entries.append(LogEntry);
		}
		Response["Lines"] = Entries;
	}
	else if(Command == "Shutdown")
	{
		QTimer::singleShot(100,this,SLOT(Shutdown()));
	}
	else if(Command == "GetSettings")
	{
		if(Request.contains("Options"))
		{
			QVariantMap Options;
			foreach(const QString& Key, Request["Options"].toStringList())
				Options[Key] = m_Settings->value(Key);
			Response["Options"] = Options;
		}
		else
		{
			QVariantMap Properties;
			foreach(const QString& Groupe, m_Settings->ListGroupes())
			{
				QVariantMap Branche;
				foreach(const QString& Key, m_Settings->ListKeys(Groupe))
					Branche[Key] = m_Settings->GetSetting(Groupe + "/" + Key);
				Properties[Groupe] = Branche;
			}
			Response["Properties"] = Properties;
		}
	}
	else if(Command == "SetSettings")
	{
		if(Request.contains("Options"))
		{
			QVariantMap Options = Request["Options"].toMap();
			foreach(const QString& Key, Options.keys())
				m_Settings->setValue(Key, Options[Key]);
		}
		else
		{
			QVariantMap Properties = Request["Properties"].toMap();
			foreach(const QString& Groupe, Properties.uniqueKeys())
			{
				QVariantMap Branche = Properties[Groupe].toMap();
				foreach(const QString& Key, Branche.uniqueKeys())
					m_Settings->SetSetting(Groupe + "/" + Key, Branche[Key]);
			}
		}
		Response["Result"] = "ok";
	}
	else if(Command == "Connect")
	{
		if(!m_KadHandler)
		{
			uint16 TCPPort = Request["TCPPort"].toUInt();
			uint16 UDPPort = Request["UDPPort"].toUInt();
			QByteArray UserHash = Request["UserHash"].toByteArray();
			m_KadHandler = new CKadHandler(m_KadID, TCPPort, UserHash);
			if(UDPPort)
				m_KadHandler->SetupProxy(UDPPort, this);
			else
				m_KadHandler->SetupSocket(m_Settings->GetInt("Kademlia/UDPPort"), m_UDPKey);
			CheckAndBootstrap();
		}

		m_KadHandler->EnableLanMode(Request["EnableLanMode"].toBool());
		m_KadHandler->SetupCryptLayer(Request["SupportsCryptLayer"].toBool(), Request["RequestsCryptLayer"].toBool(), Request["RequiresCryptLayer"].toBool());
		m_KadHandler->SetupNatTraversal(Request["SupportsNatTraversal"].toBool());

		Response["KadID"] = m_KadID;
		Response["UDPKey"] = m_UDPKey;

		Response["Result"] = "ok";
	}
	else if(!m_KadHandler || !m_KadHandler->IsRunning())
	{
		Response["Result"] = "Disconnected";
	}
	else if(Command == "AddNode")
	{	
		m_KadHandler->Bootstrap(QHostAddress(Request["Address"].toString()), Request["KadPort"].toUInt());
	}
	else if (Command == "CheckFWState")
	{
		Kademlia::CKademlia::RecheckFirewalled();
	}
	else if(Command == "SyncState")
	{
		Response["KadID"] = m_KadID;
		Response["UDPKey"] = m_UDPKey;

		m_KadHandler->SetFirewalled(Request["Firewalled"].toBool());
		m_KadHandler->SetPublicIP(QHostAddress(Request["PublicIP"].toString()).toIPv4Address());
		if(Request.contains("BuddyIP"))
			m_KadHandler->SetBuddy(QHostAddress(Request["BuddyIP"].toString()).toIPv4Address(), Request["BuddyPort"].toUInt());
		else
			m_KadHandler->ClearBuddy();

		Kademlia::CUInt128 IPv6;
		QHostAddress MyIPv6(Request["IPv6"].toString());
		if(!MyIPv6.isNull())
			IPv6.SetValueBE((byte*)MyIPv6.toIPv6Address().c);
		m_KadHandler->SetIPv6(IPv6);

		Response["PublicIP"] = QHostAddress(m_KadHandler->GetKadIP()).toString();
		Response["KadPort"] = m_KadHandler->GetKadPort();
		Response["UDPPort"] = m_KadHandler->GetKadPort(true);
		Response["Firewalled"] = m_KadHandler->IsFirewalledTCP();
		Response["KadFirewalled"] = m_KadHandler->IsFirewalledUDP();
		Response["NodeCount"] = Kademlia::CKademlia::GetRoutingZone()->GetNumContacts();


		QVariantList QueuedFWChecks;
		list<SQueuedFWCheck>* pQueuedFWChecks = m_KadHandler->GetQueuedFWChecks();
		for(list<SQueuedFWCheck>::iterator I = pQueuedFWChecks->begin(); I != pQueuedFWChecks->end(); I++)
		{
			QVariantMap QueuedFWCheck;
			QueuedFWCheck["Address"] = QHostAddress(I->uIP).toString();
			QueuedFWCheck["TCPPort"] = I->uTCPPort;
			QueuedFWCheck["UserHash"] = QByteArray((char*)I->UserHash,16);
			QueuedFWCheck["ConOpts"] = I->uConOpts;
			if(I->bTestUDP)
			{
				QueuedFWCheck["TestUDP"] = true;
				QueuedFWCheck["IntPort"] = m_KadHandler->GetKadPort(true); 
				QueuedFWCheck["ExtPort"] = m_KadHandler->GetKadPort(false); 
				if(CUDPSocket::Instance())
					QueuedFWCheck["UDPKey"] = CUDPSocket::Instance()->GetUDPVerifyKey(ntohl(I->uIP));
			}
			QueuedFWChecks.append(QueuedFWCheck);
		}
		pQueuedFWChecks->clear();
		Response["QueuedFWChecks"] = QueuedFWChecks;

		QVariantList PendingFWChecks;
		list<SPendingFWCheck>* pPendingFWChecks = m_KadHandler->GetPendingFWChecks();
		for(list<SPendingFWCheck>::iterator I = pPendingFWChecks->begin(); I != pPendingFWChecks->end(); I++)
		{
			QVariantMap PendingFWCheck;
			PendingFWCheck["Address"] = QHostAddress(I->uIP).toString();
			PendingFWChecks.append(PendingFWCheck);
		}
		Response["PendingFWChecks"] = PendingFWChecks;

		QVariantList BufferedCallbacks;
		list<SBufferedCallback>* pBufferedCallbacks = m_KadHandler->GetBufferedCallbacks();
		for(list<SBufferedCallback>::iterator I = pBufferedCallbacks->begin(); I != pBufferedCallbacks->end(); I++)
		{
			QVariantMap BufferedCallback;
			BufferedCallback["Address"] = QHostAddress(I->uIP).toString();
			BufferedCallback["TCPPort"] = I->uTCPPort;
			BufferedCallback["BuddyID"] = QByteArray((char*)I->BuddyID,16);
			BufferedCallback["FileID"] = QByteArray((char*)I->FileID, 16);
			BufferedCallbacks.append(BufferedCallback);
		}
		pBufferedCallbacks->clear();
		Response["BufferedCallbacks"] = BufferedCallbacks;

		// Note: we need to synch this only if we have an own socket
		if(CUDPSocket::Instance())
		{
			QVariantList BufferedPackets;
			list<SBufferedPacket>* pBufferedPackets = m_KadHandler->GetBufferedPackets();
			for(list<SBufferedPacket>::iterator I = pBufferedPackets->begin(); I != pBufferedPackets->end(); I++)
			{
				QVariantMap BufferedPacket;
				BufferedPacket["Address"] = QHostAddress(I->uIP).toString();
				BufferedPacket["UDPPort"] = I->uUDPPort;
				BufferedPacket["Data"] = I->Data;
				BufferedPackets.append(BufferedPacket);
			}
			pBufferedPackets->clear();
			Response["BufferedPackets"] = BufferedPackets;

			QVariantList PendingCallbacks;
			list<SPendingCallback>* pPendingCallbacks = m_KadHandler->GetPendingCallbacks();
			for(list<SPendingCallback>::iterator I = pPendingCallbacks->begin(); I != pPendingCallbacks->end(); I++)
			{
				QVariantMap PendingCallback;
				PendingCallback["Address"] = QHostAddress(I->uIP).toString();
				PendingCallback["TCPPort"] = I->uTCPPort;
				PendingCallback["UserHash"] = QByteArray((char*)I->UserHash,16);
				PendingCallback["ConOpts"] = I->uConOpts;
				PendingCallbacks.append(PendingCallback);
			}
			pPendingCallbacks->clear();
			Response["PendingCallbacks"] = PendingCallbacks;
		}

		QVariantList PendingBuddys;
		list<SPendingBuddy>* pPendingBuddys = m_KadHandler->GetPendingBuddys();
		for(list<SPendingBuddy>::iterator I = pPendingBuddys->begin(); I != pPendingBuddys->end(); I++)
		{
			QVariantMap PendingBuddy;
			PendingBuddy["Address"] = QHostAddress(I->uIP).toString();
			PendingBuddy["TCPPort"] = I->uTCPPort;
			PendingBuddy["KadPort"] = I->uKadPort;
			PendingBuddy["ConOpts"] = I->uConOpts;
			PendingBuddy["UserHash"] = QByteArray((char*)I->UserHash,16);
			PendingBuddy["BuddyID"] = QByteArray((char*)I->BuddyID,16);
			PendingBuddy["Incoming"] = I->bIncoming;
			PendingBuddys.append(PendingBuddy);
		}
		pPendingBuddys->clear();
		Response["PendingBuddys"] = PendingBuddys;

		QVariantMap Stats;
		Stats["TotalUsers"] = Kademlia::CKademlia::GetKademliaUsers();
		Stats["TotalFiles"] = Kademlia::CKademlia::GetKademliaFiles();
		Stats["IndexedSource"] = Kademlia::CKademlia::GetIndexed()->GetIndexSource();
		Stats["IndexedKeyword"] = Kademlia::CKademlia::GetIndexed()->GetIndexKeyword();
		Stats["IndexedNotes"] = Kademlia::CKademlia::GetIndexed()->GetIndexNotes();
		Stats["IndexLoad"] = Kademlia::CKademlia::GetIndexed()->GetIndexLoad();
		Response["Stats"] = Stats;

		Response["Result"] = Kademlia::CKademlia::IsConnected() ? "Connected" : "Connecting";
	}
	else if(Command == "SetUDPFWCheckResult")
	{
		Kademlia::CUDPFirewallTester::SetUDPFWCheckResult(Request["State"] == "Succeeded", Request["State"] == "Cancelled", QHostAddress(Request["Address"].toString()).toIPv4Address(), 0);
	}
	else if(Command == "FWCheckUDPRequested")
	{
		m_KadHandler->FWCheckUDPRequested(QHostAddress(Request["Address"].toString()).toIPv4Address(), Request["IntPort"].toUInt(), Request["ExtPort"].toUInt(), Request["UDPKey"].toUInt(), Request["Biased"].toBool());
		Response["Result"] = "ok";
	}
	else if(Command == "FWCheckACKRecived")
	{
		m_KadHandler->FWCheckACKRecived(QHostAddress(Request["Address"].toString()).toIPv4Address());
		Response["Result"] = "ok";
	}
	else if(Command == "SendFWCheckACK")
	{
		m_KadHandler->SendFWCheckACK(QHostAddress(Request["Address"].toString()).toIPv4Address(), Request["KadPort"].toUInt());
		Response["Result"] = "ok";
	}
	else if(Command == "SyncFiles")
	{
		QVariantList Files;
		map<Kademlia::CUInt128, SFileInfo*>& FileList = m_KadHandler->GetFileMap();
		map<Kademlia::CUInt128, SFileInfo*>  TempList = FileList;
		foreach(const QVariant& vFile, Request["Files"].toList())
		{
			QVariantMap File = vFile.toMap();

			QByteArray FileID = File["FileID"].toByteArray();
			Kademlia::CUInt128 fileID((byte*)FileID.data());

			SFileInfo* FileInfo;
			map<Kademlia::CUInt128, SFileInfo*>::iterator I = TempList.find(fileID);
			if(I == TempList.end())
			{
				FileInfo = new SFileInfo;
				FileInfo->FileID = fileID;
				FileInfo->uSize = File["FileSize"].toULongLong();
				FileInfo->sName = File["FileName"].toString().toStdWString();
				m_KadHandler->AddFile(FileInfo);
			}
			else
			{
				FileInfo = FileList[fileID];
				TempList.erase(I);
			}

			FileInfo->bShared = File["Shared"].toBool();

			FileInfo->ClearTags();
			QVariantMap Tags = File["Tags"].toMap();
			foreach(const QString& Name, Tags.uniqueKeys())
			{
				if(Name == "AICHHash")		FileInfo->TagList.push_back(new CTagBsob(TAG_KADAICHHASHPUB,(byte*)Tags[Name].toByteArray().data(), (uint8)20));
				//else if(Name == "NeoHash")	FileInfo->TagList.push_back(new CTagBsob(STR(TAG_KADNEOHASHPUB),(byte*)Tags[Name].toByteArray().data(), (uint8)32));
				else if(Name == "InfoHash")	FileInfo->TagList.push_back(new CTagString(TAG_KADTORREN,QString(Tags[Name].toByteArray().toHex()).toStdWString()));
				else if(Name == "Artist")	FileInfo->TagList.push_back(new CTagString(TAG_MEDIA_ARTIST,Tags[Name].toString().toStdWString()));
				else if(Name == "Album")	FileInfo->TagList.push_back(new CTagString(TAG_MEDIA_ALBUM,Tags[Name].toString().toStdWString()));
				else if(Name == "Title")	FileInfo->TagList.push_back(new CTagString(TAG_MEDIA_TITLE,Tags[Name].toString().toStdWString()));
				else if(Name == "Length")	FileInfo->TagList.push_back(new CTagVarInt(TAG_MEDIA_LENGTH,Tags[Name].toInt()));
				else if(Name == "Bitrate")	FileInfo->TagList.push_back(new CTagVarInt(TAG_MEDIA_BITRATE,Tags[Name].toInt()));
				else if(Name == "Codec")	FileInfo->TagList.push_back(new CTagString(TAG_MEDIA_CODEC,Tags[Name].toString().toStdWString()));
			}

			FileInfo->uRating = File["Rating"].toUInt();
			FileInfo->sComment  = File["Description"].toString().toStdWString();

			FileInfo->uCompleteSourcesCount = File["CompleteSources"].toUInt();

			File.clear();
			File["FileID"] = FileID;
            File["LastPublishedSource"] = (uint64)FileInfo->lastPublishTimeKadSrc;
            File["LastPublishedNote"] = (uint64)FileInfo->lastPublishTimeKadNotes;
			Files.append(File);
		}

		// whats left in the temp list was not transmited and thos must have been removed, so remove it.
		for (map<Kademlia::CUInt128, SFileInfo*>::iterator I = TempList.begin(); I != TempList.end(); I++)
		{
			map<Kademlia::CUInt128, SFileInfo*>::iterator J = FileList.find(I->first);
			ASSERT(J != FileList.end());
			m_KadHandler->RemoveFile(J->second);
		}

		Response["Files"] = Files;
		Response["Result"] = "ok";
	}
	else if(Command == "RequestKadCallback")
	{
		Kademlia::CSearchManager::UpdateStats(); // workaround to ensure we wont start to many requets
		QByteArray BuddyID = Request["BuddyID"].toByteArray();
		QByteArray FileID = Request["FileID"].toByteArray();
		bool bRet = m_KadHandler->RequestKadCallback(BuddyID, FileID);
		Response["Result"] = bRet ? "Done" : "Wait";
	}
	else if(Command == "FindSources")
	{
		QByteArray FileID = Request["FileID"].toByteArray();
		uint64 uFileSize = Request["FileSize"].toULongLong();
		QString FileName = Request["FileName"].toString();
		Response["SearchID"] = m_KadHandler->FindSources(FileID, uFileSize, FileName.toStdWString());
	}
	else if(Command == "ResetPub")
	{
		QByteArray FileID = Request["FileID"].toByteArray();
		Kademlia::CUInt128 fileID((byte*)FileID.data());

		map<Kademlia::CUInt128, SFileInfo*>::iterator I = m_KadHandler->GetFileMap().find(fileID);
		if(I != m_KadHandler->GetFileMap().end())
		{
			I->second->lastPublishTimeKadSrc = 0;
			I->second->lastPublishTimeKadNotes = 0;
		}
	}
	else if(Command == "GetFoundSources")
	{
		uint32 SearchID = Request["SearchID"].toUInt();
		map<Kademlia::CUInt128, SSearch::SSource*> SourceMap;
		Response["Finished"] = m_KadHandler->GetFoundSources(SearchID, SourceMap);
		QVariantList Sources;
		for(map<Kademlia::CUInt128, SSearch::SSource*>::iterator I = SourceMap.begin(); I != SourceMap.end(); I++)
		{
			SSearch::SSource* pSource = I->second;

			QVariantMap Source;
			Source["Address"] = QHostAddress(pSource->uIP).toString();
			Source["TCPPort"] = pSource->uTCPPort;
			Source["KadPort"] = pSource->uKADPort;
			QByteArray UserHash(16,'\0');
			//pSource->UserHash.ToByteArray((byte*)UserHash.data());
			I->first.ToByteArray((byte*)UserHash.data());
			Source["UserHash"] = UserHash;
			Source["ConOpts"] = pSource->uCryptOptions;
			if(pSource->eType != SSearch::SSource::eOpen)
			{
				if(pSource->eType == SSearch::SSource::eFirewalled)		
					Source["Type"] = "Closed"; // a.k.a. firewalled
				else if(pSource->eType == SSearch::SSource::eUDPOpen)	
					Source["Type"] = "OpenUDP"; // a.k.a. firewalled but with direct callback support
				QByteArray BuddyID(16,'\0');
				pSource->BuddyID.ToByteArray((byte*)BuddyID.data());
				Source["BuddyID"] = BuddyID;
				Source["BuddyIP"] = QHostAddress(pSource->uBuddyIP).toString();
				Source["BuddyPort"] = pSource->uBuddyPort;
			}
			else
				Source["Type"] = "Open";
			if(pSource->IPv6 != 0)
			{
				byte uIP[16];
				pSource->IPv6.ToByteArray(uIP);
				Source["IPv6"] = QHostAddress(uIP).toString();
			}
			Sources.append(Source);

			delete pSource;
		}
		Response["Sources"] = Sources;
		Response["Result"] = "ok";
	}
	else if(Command == "FindFiles")
	{
		SSearchRoot SearchRoot;
								//Request["Expression"].toString().toStdWString();
		SearchRoot.pSearchTree	= new SSearchTree(); 
		SearchRoot.pSearchTree->FromVariant(Request["SearchTree"]);
		SearchRoot.typeText		= Request["FileType"].toString().toStdWString();
		SearchRoot.extension	= Request["FileExt"].toString().toStdWString();
		SearchRoot.minSize		= Request["MinSize"].toULongLong();
		SearchRoot.maxSize		= Request["MaxSize"].toULongLong();
		SearchRoot.availability	= Request["Availability"].toUInt();

		QString ErrorStr;
		if(uint32 SearchID = m_KadHandler->FindFiles(SearchRoot, ErrorStr))
			Response["SearchID"] = SearchID;
		else
			Response["Error"] = ErrorStr;
	}
	else if(Command == "GetFoundFiles")
	{
		uint32 SearchID = Request["SearchID"].toUInt();
		map<Kademlia::CUInt128, SSearch::SFile*> FileMap;
		Response["Finished"] = m_KadHandler->GetFoundFiles(SearchID, FileMap);
		QVariantList Files;
		for(map<Kademlia::CUInt128, SSearch::SFile*>::iterator I = FileMap.begin(); I != FileMap.end(); I++)
		{
			SSearch::SFile* pFile = I->second;

			QVariantMap File;
			QByteArray FileID(16,'\0');
			//pFile->FileID.ToByteArray((byte*)UserHash.data());
			I->first.ToByteArray((byte*)FileID.data());
			File["FileID"] = FileID;
			File["FileName"] = QString::fromStdWString(pFile->sName);
			File["FileSize"] = (quint64)pFile->uSize;
			File["FileType"] = QString::fromStdWString(pFile->sType);
			QVariantMap Tags;
			for(TagPtrList::iterator J = pFile->TagList.begin(); J != pFile->TagList.end(); J++)
			{
				CTag* pTag = *J;

				if(pTag->GetName() == TAG_KADAICHHASHPUB)		Tags["AICHHash"] = QByteArray((char*)pTag->GetBsob(), pTag->GetBsobSize());
				//else if(pTag->GetName() == TAG_KADNEOHASHPUB)	Tags["NeoHash"] = QByteArray((char*)pTag->GetBsob(), pTag->GetBsobSize());
				else if(pTag->GetName() == TAG_KADTORREN)		Tags["InfoHash"] = QByteArray::fromHex(QString::fromStdWString(pTag->GetStr()).toLatin1());
				else if(pTag->GetName() == TAG_MEDIA_ARTIST)	Tags["Artist"] = QString::fromStdWString(pTag->GetStr());
				else if(pTag->GetName() == TAG_MEDIA_ALBUM)		Tags["Album"] = QString::fromStdWString(pTag->GetStr());
				else if(pTag->GetName() == TAG_MEDIA_TITLE)		Tags["Title"] = QString::fromStdWString(pTag->GetStr());
				else if(pTag->GetName() == TAG_MEDIA_LENGTH)	Tags["Length"] = pTag->GetInt();
				else if(pTag->GetName() == TAG_MEDIA_BITRATE)	Tags["Bitrate"] = pTag->GetInt();
				else if(pTag->GetName() == TAG_MEDIA_CODEC)		Tags["Codec"] = QString::fromStdWString(pTag->GetStr());
			}
			File["Tags"] = Tags;
			File["Availability"] = pFile->uAvailability;
			File["DifferentNames"] = pFile->uDifferentNames;
			File["PublishersKnown"] = pFile->uPublishersKnown;
			File["TrustValue"] = pFile->uTrustValue;
			Files.append(File);

			delete pFile;
		}
		Response["Files"] = Files;
		Response["Result"] = "ok";
	}
	else if(Command == "FindNotes")
	{
		QByteArray FileID = Request["FileID"].toByteArray();
		uint64 uFileSize = Request["FileSize"].toULongLong();
		QString FileName = Request["FileName"].toString();
		Response["SearchID"] = m_KadHandler->FindNotes(FileID, uFileSize, FileName.toStdWString());
	}
	else if(Command == "GetFoundNotes")
	{
		uint32 SearchID = Request["SearchID"].toUInt();
		map<Kademlia::CUInt128, SSearch::SNote*> NoteMap;
		Response["Finished"] = m_KadHandler->GetFoundNotes(SearchID, NoteMap);
		QVariantList Notes;
		for(map<Kademlia::CUInt128, SSearch::SNote*>::iterator I = NoteMap.begin(); I != NoteMap.end(); I++)
		{
			SSearch::SNote* pNote = I->second;

			QByteArray SourceID(16,'0');
			I->first.ToByteArray((uint8_t*)SourceID.data());

			QVariantMap Note;
			Note["SourceID"] = SourceID;
			Note["FileName"] = QString::fromStdWString(pNote->sName);
			Note["Rating"] = pNote->uRating;
			Note["Description"] = QString::fromStdWString(pNote->sComment);
			Notes.append(Note);

			delete pNote;
		}
		Response["Notes"] = Notes;
		Response["Result"] = "ok";
	}
	else if(Command == "StopSearch")
	{
		uint32 SearchID = Request["SearchID"].toUInt();
		m_KadHandler->StopSearch(SearchID);
	}
	else if(Command == "ProcessUDP")
	{
		quint16 RecvPort= Request["RecvPort"].toUInt();

		quint8 Prot		= Request["Prot"].toUInt();
		QByteArray Data	= Request["Data"].toByteArray();
		Data.insert(0, Prot);

		quint32 IPv4	= QHostAddress(Request["Address"].toString()).toIPv4Address();
		quint16 UDPPort	= Request["KadPort"].toUInt();
		quint32 UDPKey	= Request["UDPKey"].toUInt();
		bool validKey	= Request["ValidKey"].toBool();

		emit ProcessPacket(Data, IPv4, UDPPort, validKey, UDPKey);
	}
	else if(Command == "Disconnect")
	{
		delete m_KadHandler;
		m_KadHandler = NULL;

		Response["Result"] = "ok";
	}
	else 
	{
		Response["Error"] = "Invalid Command";
		Response["Result"] = "fail";
	}

	Result = Response;
}

void CMuleKad::SendPacket(QByteArray Data, quint32 IPv4, quint16 UDPPort, bool Encrypt, QByteArray TargetKadID, quint32 UDPKey)
{
	ASSERT(Data.size() > 1);

	QVariantMap Request;
	Request["SendPort"] = m_KadHandler->GetKadPort(true);

	Request["Prot"] = Data.data()[0];
	Request["Data"] = QByteArray::fromRawData(Data.data() + 1, Data.size() -1);

	Request["Address"] = QHostAddress(IPv4).toString();
	Request["KadPort"] = UDPPort;
	if(Encrypt)
	{
		ASSERT(!TargetKadID.isEmpty() || UDPKey != 0);
		if(!TargetKadID.isEmpty())
			Request["KadID"] = TargetKadID;
		if(UDPKey)
			Request["UDPKey"] = UDPKey;
	}

	m_pInterface->PushNotification("SendUDP", Request);
}

void CMuleKad::Connect()
{
	if(m_KadHandler)
		return;

	m_KadHandler = new CKadHandler(m_KadID, 0, QByteArray(16,'\0'));
	m_KadHandler->SetupSocket(m_Settings->GetInt("Kademlia/UDPPort"), m_UDPKey);
	CheckAndBootstrap();
}

void CMuleKad::Disconnect()
{
	if(!m_KadHandler)
		return;

	delete m_KadHandler;
	m_KadHandler = NULL;
}

void CMuleKad::CheckAndBootstrap()
{
	uint32 uNodeCount = Kademlia::CKademlia::GetRoutingZone()->GetNumContacts();
	if(uNodeCount > 0)
		return;

	QString Url = Cfg()->GetString("Kademlia/Bootstrap");
	if(!Url.isEmpty())
	{
		QNetworkRequest Request = QNetworkRequest(Url);
		m_pBootstrap = m_pRequestManager->get(Request);
		connect(m_pBootstrap, SIGNAL(finished()), this, SLOT(OnRequestFinished()));
	}
}

void CMuleKad::OnRequestFinished()
{
	if(sender() != m_pBootstrap)
	{
		ASSERT(0);
		return;
	}

	CBuffer Buffer(m_pBootstrap->readAll());
	//QString Url = m_pBootstrap->url().toString();
	m_pBootstrap->deleteLater();
	//m_pBootstrap = NULL;
	m_pBootstrap = (QNetworkReply*)-1;

	if(m_KadHandler)
		m_KadHandler->LoadNodes(Buffer);
}
