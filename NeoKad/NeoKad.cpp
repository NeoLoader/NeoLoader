#include "GlobalHeader.h"
#include "NeoKad.h"
#include <QApplication>
#include "../Framework/Cryptography/AbstractKey.h"
#include "../Framework/Settings.h"
#include "../Framework/Xml.h"
#include "Kad/KadHeader.h"
#include "Kad/Kademlia.h"
#include "Kad/KadID.h"
#include "Kad/RoutingRoot.h"
#include "Kad/KadEngine/KadScript.h"
#include "Common/v8Engine/JSScript.h"
#include "Common/v8Engine/JSVariant.h"
#include "Common/v8Engine/JSDebug.h"
#include "../Framework/RequestManager.h"
#include "../Framework/OtherFunctions.h"

#include "Kad/KadNode.h"
#include "Kad/KadHandler.h"
#include "Kad/KadEngine/KadEngine.h"

#ifdef _DEBUG
void test();
void testKad(CKademlia* pKad);
#endif

CNeoKad* theKad = NULL;

CNeoKad::CNeoKad(QObject *parent)
 : CLoggerTmpl<QObject>("NeoKad", parent)
{
	ASSERT(theKad == NULL);
	theKad = this;

	m_uTimerCounter = 0;
	m_uTimerID = startTimer(10);

	m_bEmbedded = false;
	m_uLastContact = 0;

#ifdef _DEBUG
	test();
#endif

	CSettings::InitSettingsEnvironment(APP_ORGANISATION, APP_NAME, APP_DOMAIN);
	QMap<QString, CSettings::SSetting> Settings;

	Settings.insert("Kademlia/Port", CSettings::SSetting(GetRandomInt(9000, 9999)));
	Settings.insert("Kademlia/AutoConnect", CSettings::SSetting(true));
	Settings.insert("Kademlia/Debug", CSettings::SSetting(0));
	Settings.insert("Kademlia/Cache", CSettings::SSetting(""));
	Settings.insert("Kademlia/Bootstrap", CSettings::SSetting("http://nodes.neoloader.to/neonodes.php"));
	
	Settings.insert("Log/Store", CSettings::SSetting(false));
	Settings.insert("Log/Limit", CSettings::SSetting(256,128,1024));

	Settings.insert("Gui/LogLength", CSettings::SSetting(256,128,1024));
	Settings.insert("Gui/Window", CSettings::SSetting(""));
	Settings.insert("Gui/Main", CSettings::SSetting(""));
	Settings.insert("Gui/RoutingTable", CSettings::SSetting(""));
	Settings.insert("Gui/LookupList", CSettings::SSetting(""));
	Settings.insert("Gui/IndexList", CSettings::SSetting(""));

	m_Settings = new CSettings("NeoKad", Settings, this);

	SetLogLimit(m_Settings->GetInt("Log/Limit"));
	if(m_Settings->GetBool("Log/Store"))
		SetLogPath(m_Settings->GetSettingsDir() + "/Logs");

	QStringList Args = QApplication::instance()->arguments();

	QString Name;
	int NameIndex = Args.indexOf("-name");
	if(NameIndex != -1 && NameIndex + 1 < Args.size())
		Name = Args.at(NameIndex + 1);

	m_pInterface = new CIPCServer(this);
	if(Name.isEmpty())
		m_pInterface->LocalListen("NeoKad");
	else
		m_pInterface->LocalListen(Name);

	int ListenIndex = Args.indexOf("-listen");
	if(ListenIndex != -1 && ListenIndex + 1 < Args.size())
		m_pInterface->RemoteListen(Args.at(ListenIndex + 1).toULong());

	connect(m_pInterface, SIGNAL(RequestRecived(const QString&, const QVariant&, QVariant&)), this, SLOT(OnRequestRecived(const QString&, const QVariant&, QVariant&)));

	m_pBootstrap = NULL;
	m_pRequestManager = new CRequestManager(MIN2S(60), this);

	uint16 KadPort = 0;
	int PortIndex = Args.indexOf("-port");
	if(PortIndex != -1 && PortIndex + 1 < Args.size())
		KadPort = Args.at(PortIndex + 1).toULong();
	if(KadPort == 0)
		KadPort = m_Settings->GetInt("Kademlia/Port");

	/*QString CachePath = Cfg()->GetString("Kademlia/Cache");
	if(CachePath.isEmpty())
	{
#ifdef __APPLE__
		CachePath = QDir::homePath() + "/NeoLoader";
#else
		CachePath = Cfg()->GetAppDir();
#endif
		CachePath += "/Blocks/";
	}
	if(CachePath.right(1) != "/")
		CachePath.append("/");
	CreateDir(CachePath);*/

	QString ConfigDir = m_Settings->GetSettingsDir() + "/";
	QString ScriptCache = m_Settings->GetSettingsDir() + "/Cache/Scripts/";
	QString BlockCache = m_Settings->GetSettingsDir() + "/Cache/Blocks/";
	CreateDir(ScriptCache);
	CreateDir(BlockCache);

	QString Version = "NK " + QString::number(KAD_VERSION_MJR) + "." + QString::number(KAD_VERSION_MIN).rightJustified(2, '0');
#if KAD_VERSION_UPD > 0
	Version.append('a' + KAD_VERSION_UPD - 1);
#endif

	Version += " ";

	bool bIPv6 = true;
#ifdef Q_OS_WIN32
	Version += "W";
	switch(QSysInfo::windowsVersion())
	{
		// older platforms are not supported at all
		case QSysInfo::WV_2000:
		case QSysInfo::WV_XP:
			bIPv6 = false;
			break;
		case QSysInfo::WV_VISTA:
		case QSysInfo::WV_WINDOWS7:
			bIPv6 = true;
			break;
	}
#else
	Version += "U";
    bIPv6 = false;
#endif

#ifdef _DEBUG
	Version += "d";
#endif


	CVariant Config;
	Config["ConfigPath"] = ConfigDir.toStdWString();
	Config["ScriptCachePath"] = ScriptCache.toStdWString();
	Config["BinaryCachePath"] = BlockCache.toStdWString();
	m_pKademlia = new CKademlia(KadPort, bIPv6, Config, Version.toStdString());
	if(m_Settings->GetBool("Kademlia/AutoConnect"))
		Connect();

#ifdef _DEBUG
	testKad(m_pKademlia);
#endif
}

CNeoKad::~CNeoKad()
{
	theKad = NULL;

	killTimer(m_uTimerID);

	Disconnect();
	delete m_pKademlia;
}

void CNeoKad::Process()
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

	UINT Tick = MkTick(m_uTimerCounter);

	m_pKademlia->Process(Tick);

	if(Tick & EPerSec) // once a second
	{
		if(!m_pKademlia->IsDisconnected())
			CheckAndBootstrap();

		if (Tick & EPer10Sec) // every 10 seconds
		{
			//if(GetCurTick() - m_uLastSave > MIN2MS(15)) // X-ToDo-Customize
			//	StoreNodes();
		}
	}
}

void CNeoKad::CheckAndBootstrap()
{
	if(m_pBootstrap || (m_pKademlia->GetNodeCount() != 0 && GetTime() - m_pKademlia->GetLastContact() < HR2S(6)))
		return;

#ifdef _DEBUG
	//return;
#endif

	QString Url = Cfg()->GetString("Kademlia/Bootstrap");
	if(!Url.isEmpty())
	{
		QString Ver = QString::number(KAD_VERSION_MJR) + "." + QString::number(KAD_VERSION_MIN).rightJustified(2, '0');
//#if KAD_VERSION_UPD > 0
//		Ver.append('a' + KAD_VERSION_UPD - 1);
//#endif

		Url += QString("?id=%1&utp=%2&ver=%3").arg(QString::fromStdWString(m_pKademlia->GetID().ToHex())).arg(m_pKademlia->GetPort()).arg(Ver);
//#ifdef _DEBUG
//		Url += "&debug=1";
//#endif
		QNetworkRequest Request = QNetworkRequest(Url);
		m_pBootstrap = m_pRequestManager->get(Request);
		connect(m_pBootstrap, SIGNAL(finished()), this, SLOT(OnRequestFinished()));
	}
}

void CNeoKad::OnRequestFinished()
{
	if(sender() != m_pBootstrap)
	{
		ASSERT(0);
		return;
	}

	QByteArray Bootstrap = m_pBootstrap->readAll();
	//QString Url = m_pBootstrap->url().toString();
	m_pBootstrap->deleteLater();
	//m_pBootstrap = NULL;
	m_pBootstrap = (QNetworkReply*)-1;

	CRoutingRoot* pRoot = m_pKademlia->GetChild<CRoutingRoot>();
	if(!pRoot)
		return;

	QVariantMap Response = CXml::Parse(Bootstrap).toMap();
	foreach(const QVariant& vNode, Response["Nodes"].toList())
	{
		QVariantMap Node = vNode.toMap();

		if(!Node.contains("UTP"))
			continue;

		wstring sAddress = QString("utp://%1:%2").arg(Node["IP"].toString()).arg(Node["UTP"].toUInt()).toStdWString();
	
		CPointer<CKadNode> pNode = new CKadNode();
		pNode->GetID().FromHex(Node["ID"].toString().toStdWString());
		CSafeAddress Address(sAddress);
		pNode->UpdateAddress(Address);

		pRoot->AddNode(pNode);
	}
}

void CNeoKad::OnRequestRecived(const QString& Command, const QVariant& Parameters, QVariant& Result)
{
	QVariantMap Request = Parameters.toMap();
	//QString strTest = CXml::Serialize(Request);

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
		Disconnect();
		QTimer::singleShot(100,this,SLOT(Shutdown()));
	}
	else if(Command == "ShowGUI")
	{
		emit ShowGUI();
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
	// Kademlia generic
	else if(Command == "Connect")
	{
		Connect();
		Response["Result"] = "Connecting";
	}
	else if(Command == "SetupSockets")
	{
		uint16 uPort = Request["Port"].toUInt();
		CAddress IPv4 = CAddress(CAddress::IPv4);
		if(Request.contains("IPv4"))
			IPv4 = CAddress(Request["IPv4"].toString());
		CAddress IPv6 = CAddress(CAddress::IPv6);
		if(Request.contains("IPv6"))
			IPv6 = CAddress(Request["IPv6"].toString());
		m_pKademlia->BindSockets(uPort, IPv4, IPv6);
	}
	else if(m_pKademlia->IsDisconnected())
	{
		Response["Result"] = "Disconnected";
	}
	else if(Command == "QueryState")
	{
		Response["KadID"] = ((CVariant)m_pKademlia->GetID()).ToQVariant();
		Response["Port"] = m_pKademlia->GetPort();
		Response["IPv4"] = m_pKademlia->GetIPv4().ToQString();
		Response["IPv6"] = m_pKademlia->GetIPv6().ToQString();

		Response["UpRate"] = m_pKademlia->GetUpRate();
		Response["DownRate"] = m_pKademlia->GetDownRate();

		CSafeAddress UdtIpv4 = m_pKademlia->GetAddress(CSafeAddress::eUTP_IP4);
		if(UdtIpv4.IsValid())
		{
			Response["AddressV4"] = QString::fromStdWString(UdtIpv4.ToString());
			switch(m_pKademlia->GetFWStatus(CSafeAddress::eUTP_IP4))
			{
			case eFWOpen:	Response["IPStatusV4"] = "Open";
			case eFWNATed:	Response["IPStatusV4"] = "NATed";
			case eFWClosed:	Response["IPStatusV4"] = "Closed";
			}
		}
		CSafeAddress UdtIpv6 = m_pKademlia->GetAddress(CSafeAddress::eUTP_IP6);
		if(UdtIpv6.IsValid())
		{
			Response["AddressV6"] = QString::fromStdWString(UdtIpv6.ToString());
			switch(m_pKademlia->GetFWStatus(CSafeAddress::eUTP_IP6))
			{
			case eFWOpen:	Response["IPStatusV6"] = "Open";
			case eFWNATed:	Response["IPStatusV6"] = "NATed";
			case eFWClosed:	Response["IPStatusV6"] = "Closed";
			}
		}

		list<SKadScriptInfo> Scripts;
		m_pKademlia->QueryScripts(Scripts);

		QVariantList ScriptList;
		for(list<SKadScriptInfo>::iterator I = Scripts.begin(); I != Scripts.end(); I++)
		{
			QVariantMap Script;
			Script["CodeID"] = I->CodeID.ToQVariant();
			Script["Name"] = QString::fromStdWString(I->Name);
			Script["Version"] = I->Version;
			// K-ToDo: add more info
			ScriptList.append(Script);
		}
		Response["Scripts"] = ScriptList;

        Response["NodeCount"] = (int)m_pKademlia->GetNodeCount();
		Response["LastContact"] = (uint64)m_pKademlia->GetLastContact();
		Response["Result"] = m_pKademlia->IsConnected() ? "Connected" : "Connecting";
	}
	else if(Command == "InstallScript")
	{
		CVariant CodeID;
		CodeID.FromQVariant(Request["CodeID"]);

		string Source = Request["Source"].toString().toStdString();

		CVariant Authentication;
		if(Request.contains("Authentication"))
		{
			CBuffer Buffer(Request["Authentication"].toByteArray());
			Authentication.FromPacket(&Buffer);
		}

		Response["Result"] = m_pKademlia->InstallScript(CodeID, Source, Authentication) ? "ok" : "fail";
	}
	else if(Command == "Disconnect")
	{
		Disconnect();
		Response["Result"] = "Disconnected";
	}
	// Kademlia lookup
	else if(Command == "StartLookup")
	{
		CVariant TargetID;
		TargetID.FromQVariant(Request["TargetID"]);

		int Timeout = Request.value("Timeout", -1).toInt();

		int HopLimit = Request.value("HopLimit", -1).toInt();
		int JumpCount = Request.value("JumpCount", -1).toInt();
		if(HopLimit <= JumpCount)
			HopLimit = JumpCount + 1;
		int SpreadCount= Request.value("SpreadCount", -1).toInt();
		
		CPrivateKey* pStoreKey = NULL;
		if(Request.contains("StoreKey"))
		{
			QByteArray KeyValue = Request["StoreKey"].toByteArray();
			pStoreKey = new CPrivateKey();	
			if(!pStoreKey->SetKey(KeyValue))
			{
				delete pStoreKey;
				pStoreKey = NULL;

				LogLine(LOG_ERROR, L"Store Key Invalid, using none");
			}
		}

		CVariant CodeID;
		TCallMap Execute;
		if(Request.contains("CodeID"))
		{
			CodeID.FromQVariant(Request["CodeID"]);

			if(!m_pKademlia->HasScript(CodeID))
			{
				Response["Error"] = "InvalidScript";
				Result = Response;
				return;
			}

			foreach(const QVariant& vCall, Request["Execute"].toList())
			{
				QVariantMap Call = vCall.toMap();
				SKadCall KadCall; 
				KadCall.Function = Call["Function"].toString().toStdString();
				KadCall.Parameters.FromQVariant(Call["Parameters"]);
				CVariant XID;
				if(Call.contains("XID"))
					XID.FromQVariant(Call["XID"]);
				else
					XID = GetRand64() & MAX_FLOAT;
				Execute.insert(TCallMap::value_type(XID, KadCall));
			}
		}

		TStoreMap Store;		
		foreach(const QVariant& vPayload, Request["Store"].toList())
		{
			QVariantMap Payload = vPayload.toMap();

			SKadStore Entry;
			Entry.Data.FromQVariant(Payload["Data"]);
			Entry.Path = Payload["Path"].toString().toStdString();
			Store.insert(TStoreMap::value_type(Payload["XID"].toInt(), Entry));
		}

		TLoadMap Retrieve;
		foreach(const QVariant& vPayload, Request["Retrieve"].toList())
		{
			QVariantMap Payload = vPayload.toMap();
			
			SKadLoad Entry;
			Entry.Path = Payload["Path"].toString().toStdString();
			Retrieve.insert(TLoadMap::value_type(Payload["XID"].toInt(), Entry));
		}

		CVariant LookupID = m_pKademlia->StartLookup(TargetID, CodeID, Execute, Store, pStoreKey, Retrieve, Timeout, HopLimit, JumpCount, SpreadCount, false, Request["GUIName"].toString().toStdWString());
		if(LookupID.IsValid())
			Response["LookupID"] = LookupID.ToQVariant();
		else
			Response["Error"] = "InvalidLookup";
	}
	else if(Command == "QueryLookup")
	{
		CVariant LookupID;
		LookupID.FromQVariant(Request["LookupID"]);
		TRetMap Results;
		TStoredMap Stored;
		TLoadedMap Loaded;

		Response = m_pKademlia->QueryLookup(LookupID, Results, Stored, Loaded).ToQVariant().toMap();
		if(Request["AutoStop"] == true && Response["Staus"] == "Finished")
			m_pKademlia->StopLookup(LookupID);

		QVariantList ResultList;
		for(TRetMap::iterator I = Results.begin(); I != Results.end(); I++)
		{
			QVariantMap Result;
			Result["XID"] = I->first.To<uint64>();
			Result["Return"] = I->second.Return.ToQVariant();
			Result["Function"] = QString::fromStdString(I->second.Function);
			ResultList.append(Result);
		}
		Response["Results"] = ResultList;

		QVariantList StoredList;
		for(TStoredMap::iterator I = Stored.begin(); I != Stored.end(); I++)
		{
			QVariantMap Result;
			Result["XID"] = I->first.To<int>();
			Result["Expiration"] = (uint64)I->second.Expire;
			Result["Count"] = I->second.Count;
			StoredList.append(Result);
		}
		Response["Stored"] = StoredList;

		QVariantList RetrievedList;
		for(TLoadedMap::iterator I = Loaded.begin(); I != Loaded.end(); I++)
		{
			QVariantMap Result;
			Result["XID"] = I->first.To<int>();
			Result["Path"] = QString::fromStdString(I->second.Path);
			Result["Data"] = I->second.Data.ToQVariant();
			RetrievedList.append(Result);
		}
		Response["Retrieved"] = RetrievedList;
	}
	else if(Command == "StopLookup")
	{
		CVariant LookupID;
		LookupID.FromQVariant(Request["LookupID"]);
		m_pKademlia->StopLookup(LookupID);
		Response["Result"] = "ok";
	}
	// Kademlia routing
	else if(Command == "MakeCloseTarget")
	{
		int uDistance = 0;
		CVariant Target = m_pKademlia->MakeCloseTarget(&uDistance);
		if(uDistance != -1)
		{
			Response["Distance"] = uDistance;
			Response["TargetID"] = Target.ToQVariant();
			Response["Result"] = "ok";
		}
		else
			Response["Result"] = "fail";
	}
	else if(Command == "SetupRoute")
	{
		CVariant TargetID;
		TargetID.FromQVariant(Request["TargetID"]);

		CPrivateKey* pEntityKey = NULL;
		if(Request.contains("EntityKey"))
		{
			QByteArray KeyValue = Request["EntityKey"].toByteArray();
			pEntityKey = new CPrivateKey();	
			if(!pEntityKey->SetKey(KeyValue))
			{
				delete pEntityKey;
				pEntityKey = NULL;

				LogLine(LOG_ERROR, L"Entity Key Invalid, using new key");
			}
		}

		int HopLimit = Request.value("HopLimit", -1).toInt();
		int JumpCount = Request.value("JumpCount", -1).toInt();
		if(HopLimit <= JumpCount)
			HopLimit = JumpCount + 1;

		CVariant MyEntityID = m_pKademlia->SetupRoute(TargetID, pEntityKey, HopLimit, JumpCount);
		if(!Request.contains("EntityKey") && MyEntityID.IsValid())
		{
			CPrivateKey* pEntityKey = m_pKademlia->GetRouteKey(MyEntityID);
			ASSERT(pEntityKey);
			Response["EntityKey"] = QByteArray((char*)pEntityKey->GetKey(), (int)pEntityKey->GetSize());
		}
		Response["MyEntityID"] = MyEntityID.ToQVariant();
	}
	else if(Command == "QueryRoute")
	{
		CVariant MyEntityID;
		MyEntityID.FromQVariant(Request["MyEntityID"]);
		Response = m_pKademlia->QueryRoute(MyEntityID).ToQVariant().toMap();
	}
	else if(Command == "OpenSession")
	{
		CVariant MyEntityID;
		MyEntityID.FromQVariant(Request["MyEntityID"]);

		CVariant EntityID;
		EntityID.FromQVariant(Request["EntityID"]);
		CVariant TargetID;
		TargetID.FromQVariant(Request["TargetID"]);

		CVariant SessionID = m_pKademlia->OpenSession(MyEntityID, EntityID, TargetID);
		if(SessionID.IsValid())
		{
			Response["SessionID"] = SessionID.ToQVariant();
			Response["Result"] =  "ok";
		}
		else
			Response["Result"] =  "fail";
	}
	else if(Command == "QueueBytes")
	{
		CVariant MyEntityID;
		MyEntityID.FromQVariant(Request["MyEntityID"]);

		CVariant EntityID;
		EntityID.FromQVariant(Request["EntityID"]);
		CVariant SessionID;
		SessionID.FromQVariant(Request["SessionID"]);

		CBuffer Buffer(Request["Data"].toByteArray());
		bool bStream = Request.value("Stream", true).toBool();

		Response["Result"] = m_pKademlia->QueueBytes(MyEntityID, EntityID, SessionID, Buffer, bStream) ? "ok" : "fail";
	}
	else if(Command == "QuerySessions")
	{
		CVariant MyEntityID;
		MyEntityID.FromQVariant(Request["MyEntityID"]);

		list<SRouteSession> Sessions;
		Response["Result"] = m_pKademlia->QuerySessions(MyEntityID, Sessions) ? "ok" : "fail";

		QVariantList SessionList;
		for(list<SRouteSession>::const_iterator I = Sessions.begin(); I != Sessions.end(); I++)
		{
			QVariantMap Session;
			Session["EntityID"] = I->EntityID.ToQVariant();
			Session["TargetID"] = I->TargetID.ToQVariant();
			Session["SessionID"] = I->SessionID.ToQVariant();
			Session["QueuedBytes"] = I->QueuedBytes;
			Session["PendingBytes"] = I->PendingBytes;
			Session["Connected"] = I->Connected;
			SessionList.append(Session);
		}
		Response["Sessions"] = SessionList;
	}
	else if(Command == "PullBytes")
	{
		CVariant MyEntityID;
		MyEntityID.FromQVariant(Request["MyEntityID"]);

		CVariant EntityID;
		EntityID.FromQVariant(Request["EntityID"]);
		CVariant SessionID;
		SessionID.FromQVariant(Request["SessionID"]);

		size_t MaxBytes = Request["MaxBytes"].toUInt();
		if(!MaxBytes)
			MaxBytes = -1;

		CBuffer Buffer;
		bool bStream = false;
		if(m_pKademlia->PullBytes(MyEntityID, EntityID, SessionID, Buffer, bStream, MaxBytes))
		{
			Response["Data"] = Buffer.ToByteArray();
			Response["Stream"] = bStream;

			Response["Result"] = "ok";
		}
		else
			Response["Result"] = "fail";
	}
	else if(Command == "CloseSession")
	{
		CVariant MyEntityID;
		MyEntityID.FromQVariant(Request["MyEntityID"]);

		CVariant EntityID;
		EntityID.FromQVariant(Request["EntityID"]);
		CVariant SessionID;
		SessionID.FromQVariant(Request["SessionID"]);

		m_pKademlia->CloseSession(MyEntityID, EntityID, SessionID);
		Response["Result"] = "ok";
	}
	else if(Command == "BreakRoute")
	{
		CVariant MyEntityID;
		MyEntityID.FromQVariant(Request["MyEntityID"]);
		m_pKademlia->BreakRoute(MyEntityID);
	}
	// Kademlia Index
	else if(Command == "InvokeScript")
	{
		CVariant CodeID;
		CodeID.FromQVariant(Request["CodeID"]);

		CVariant Parameters;
		Parameters.FromQVariant(Request["Parameters"]);
		CVariant Return;
		if(m_pKademlia->InvokeScript(CodeID, Request["Function"].toString().toStdString(), Parameters, Return))
		{
			Response["Return"] = Return.ToQVariant();
			Response["Result"] = "ok";
		}
		else
			Response["Result"] = "fail";
	}
	else if(Command == "StorePayload")
	{
		CVariant TargetID;
		TargetID.FromQVariant(Request["TargetID"]);
		CVariant Data;
		Data.FromQVariant(Request["Data"]);
		time_t Expire = Request.value("Expiration", -1).toULongLong();

		/*CPrivateKey* pStoreKey = NULL;
		if(Request.contains("StoreKey"))
		{
			QByteArray KeyValue = Request["StoreKey"].toByteArray();
			pStoreKey = new CPrivateKey();	
			if(!pStoreKey->SetKey(KeyValue))
			{
				delete pStoreKey;
				pStoreKey = NULL;

				LogLine(LOG_ERROR, L"Store Key Invalid, using none");
			}
		}*/

		if(m_pKademlia->Store(TargetID, Request["Path"].toString().toStdString(), Data, Expire))
			Response["Result"] = "ok";
		else
			Response["Result"] = "fail";
	}
	else if(Command == "RetrievePayload")
	{
		CVariant TargetID;
		TargetID.FromQVariant(Request["TargetID"]);
		
		Response["Result"] = m_pKademlia->Load(Request["Index"].toLongLong()).ToQVariant();
	}
	else if(Command == "ListIndex")
	{
		vector<SKadEntryInfo> Entrys;
		bool bOk = true;
		if(Request.contains("TargetID"))
		{
			CVariant TargetID;
			TargetID.FromQVariant(Request["TargetID"]);
		
			bOk = m_pKademlia->List(TargetID, Request["Path"].toString().toStdString(), Entrys);
		}
		else
		{
			m_pKademlia->List(Request["Path"].toString().toStdString(), Entrys);
		}

		if(bOk)
		{
			QVariantList EntryList;
			for(size_t i=0; i < Entrys.size(); i++)
			{
				QVariantMap Entry;
				Entry["DBIndex"] = Entrys[i].Index;
				Entry["TargetID"] = CVariant(Entrys[i].ID).ToQVariant();
				Entry["Path"] = QString::fromStdString(Entrys[i].Path);
				Entry["Date"] = (uint64)Entrys[i].Date;
				Entry["Expire"] = (uint64)Entrys[i].Expire;
				EntryList.append(Entry);
			}
			Response["List"] = EntryList;
			Response["Result"] = "ok";
		}
		else
			Response["Result"] = "fail";
	}
	else if(Command == "RemovePayload")
	{
		CVariant TargetID;
		TargetID.FromQVariant(Request["TargetID"]);
		m_pKademlia->Remove(Request["Index"].toLongLong());
	}
	//
	else 
	{
		Response["Error"] = "Invalid Command";
		Response["Result"] = "fail";
	}

	//ASSERT(!Response.isEmpty());
	Result = Response;
}

void CNeoKad::Connect()
{
	m_pKademlia->Connect();
	CheckAndBootstrap();
	//LoadNodes();
}

void CNeoKad::Disconnect()
{
	m_pBootstrap = NULL;
	//StoreNodes();
	m_pKademlia->Disconnect();
}

/*void CNeoKad::LoadNodes()
{
	if(!m_pKademlia->Handler())
		return;

	m_uLastSave = GetCurTick();
	NodeList Nodes;
	QVariantList NodeList = CXml::Parse(ReadFileAsString(CSettings::GetSettingsDir() + "/NeoKad.xml")).toList();
	for(int i=0; i < NodeList.count(); i++)
	{
		CVariant Node;
		Node.FromQVariant(NodeList.at(i));
		CPointer<CKadNode> pNode = new CKadNode();
		try
		{
			pNode->Load(Node, true);
		}
		catch(...)
		{
			ASSERT(0);
		}
		Nodes.push_back(pNode);
	}
	m_pKademlia->Handler()->SetNodes(Nodes);
}

void CNeoKad::StoreNodes() 
{
	if(!m_pKademlia->Handler())
		return;

	NodeList Nodes = m_pKademlia->Handler()->GetNodes();

	QVariantList NodeList;
	for (NodeList::iterator I = Nodes.begin(); I != Nodes.end(); I++)
	{
		CKadNode* pNode = *I;
		NodeList.append(pNode->Store(true).ToQVariant());
	}
	WriteStringToFile(CSettings::GetSettingsDir() + "/NeoKad.xml",CXml::Serialize(NodeList));
}*/

void CNeoKad::Authenticate(QString ScriptPath)
{
	if(ScriptPath.isEmpty())
		return;

	if(ScriptPath.right(1) != "/")
		ScriptPath.append("/");

	QStringList ScriptList = ListDir(ScriptPath);

	QFile::remove(ScriptPath + "Index.inf");
	QSettings Index(ScriptPath + "Index.inf",  QSettings::IniFormat);
	foreach(const QString& FileName, ScriptList)
	{
		StrPair NameExt = Split2(FileName, ".", true);
		if(NameExt.second != "js")
			continue;

		string Source = ReadFileAsString(ScriptPath + FileName).toStdString();

		map<string, string> HeaderFields = CKadScript::ReadHeader(Source);
		uint32 Version = CKadScript::GetVersion(s2w(HeaderFields["Version"]));
		wstring Name = s2w(HeaderFields["Name"]);
		if(Version == 0 || Name.empty())
				continue; // Header is mandatory

		/*CPointer<CJSScript> pScript = new CJSScript();

		map<string, CObject*> Objects;
		wstring wSource;
		Utf8ToWStr(wSource, Source);
		if(!pScript->Initialize(wSource, Name, Objects))
		{
			LogLine(LOG_DEBUG | LOG_ERROR, tr("Error Parsing Script: %1").arg(FileName));
			continue;
		}

		vector<CPointer<CObject> > Arguments;
		CPointer<CObject> pReturn;

		pScript->Execute("infoAPI", "getName", Arguments, pReturn);
		if(CVariantPrx* pName = pReturn->Cast<CVariantPrx>())
		{
			Name = pName->GetCopy().AsStr();
			if(Name.empty())
			{
				LogLine(LOG_DEBUG | LOG_ERROR, tr("Invalid name given by Script: %1").arg(FileName));
				continue;
			}
		}

		pScript->Execute("infoAPI", "getVersion", Arguments, pReturn);
		if(CVariantPrx* pVersion = pReturn->Cast<CVariantPrx>())
		{
			wstring sVersion = pVersion->GetCopy().AsStr();
			if(!sVersion.empty())
				Version = CKadScript::GetVersion(sVersion);
			else
				Version = pVersion->GetCopy().AsNum<uint32>();
			if(Version == 0)
			{
				LogLine(LOG_DEBUG | LOG_ERROR, tr("Invalid version given by Script: %1").arg(FileName));
				continue;
			}
		}*/

		CScoped<CPrivateKey> pPrivKey = new CPrivateKey(CPrivateKey::eECP);
		QFile KeyFile(ScriptPath + NameExt.first + ".der");
		if(!KeyFile.exists())
		{
			pPrivKey->GenerateKey("brainpoolP512r1");
			KeyFile.open(QFile::WriteOnly);
			KeyFile.write(pPrivKey->ToByteArray());
		}
		else
		{
			KeyFile.open(QFile::ReadOnly);
			pPrivKey->SetKey(KeyFile.readAll());
		}
		KeyFile.close();

		CScoped<CPublicKey> pPubKey = pPrivKey->PublicKey();

		CVariant Authentication;
		Authentication["PK"] = CVariant(pPubKey->GetKey(), pPubKey->GetSize());
		CBuffer SrcBuff((byte*)Source.data(), Source.size(), true);
		CBuffer Signature;
		pPrivKey->Sign(&SrcBuff,&Signature);
		Authentication["SIG"] = Signature;
		Authentication.Freeze();

		CVariant CodeID((byte*)NULL, KEY_128BIT);
		CKadID::MakeID(pPubKey, CodeID.GetData(), CodeID.GetSize());

		//QSettings Info(ScriptPath + NameExt.first + ".inf",  QSettings::IniFormat);
		Index.setValue(NameExt.first + "/CodeID", QString(QByteArray((char*)CodeID.GetData(), CodeID.GetSize()).toHex()));
		Index.setValue(NameExt.first + "/Name", QString::fromStdWString(Name));
		Index.setValue(NameExt.first + "/Version", Version);
		CBuffer Packet;
		Authentication.ToPacket(&Packet);
		QVariant Auth = QString(Packet.ToByteArray().toBase64());
		Index.setValue(NameExt.first + "/Authentication", Auth);
		//Info.sync();
	}
	Index.sync();
}



#ifdef _DEBUG
void testKad(CKademlia* pKad)
{
	if(pKad->IsDisconnected())
		return;

	//int d = 0;
	//pKad->MakeCloseTarget(&d);

/*	CUInt128 m_uZoneIndex = 1;
	int m_uLevel = 1;

	CUInt128 me = pKad->GetID();
QString m = MakeBin(me);

	CUInt128 uPrefix(m_uZoneIndex);
QString a = MakeBin(uPrefix);
	uPrefix.ShiftLeft(uPrefix.GetBitSize() - m_uLevel);
QString b = MakeBin(uPrefix);
int i = uPrefix.GetBit(0);
	CUInt128 uRandom(uPrefix, m_uLevel);
QString c = MakeBin(uRandom);
	uRandom.Xor(me);
QString d = MakeBin(uRandom);


QString z = "";*/
	/*QFile TestFile("OutBound_EZ23IGP1UQs.bin");
	TestFile.open(QFile::ReadOnly);
	CBuffer TestBuffer = TestFile.readAll();

	while(TestBuffer.GetSizeLeft())
	{
		string Name;
		CVariant Packet;
		StreamPacket(TestBuffer, Name, Packet);

		ASSERT(Packet.GetType() == CVariant::EMap);
	}

	ASSERT(0);*/

	/*if(CKadScript* pKadScript = pKad->GetChild<CKadEngine>()->GetScript(CVariant((byte*)QByteArray::fromHex("9F9ED5728F474928FB9509B44E4DFF80").data(), 16)))
	{
		vector<CPointer<CObject> > Parameters;
		CPointer<CObject> Return;
		pKadScript->ExecuteAPI("remoteAPI", "storeKeyword", Parameters, Return);
		if(CVariantPrx* pVariant = Return->Cast<CVariantPrx>())
		{
			ASSERT(1);
		}
	}

	ASSERT(1);*/

	/*CPrivateKey PrivKey(DEFAULT_KAD_CRYPTO);
	PrivKey.GenerateKey("secp128r1");
	CScoped<CPublicKey> pKey = PrivKey.PublicKey();

	CBuffer Source = ReadFileAsString(CSettings::GetSettingsDir() + "/Test.js").toUtf8();

	CVariant Authentication;
	Authentication["PK"] = CVariant(pKey->GetKey(), pKey->GetSize());
	CBuffer Signature;
	PrivKey.Sign(&Source,&Signature);
	Authentication["SIG"] = &Signature;
	CVariant CodeID((byte*)NULL, KEY_128BIT);
	CKadID::MakeID(pKey, CodeID.GetData(), CodeID.GetSize());

	CVariant InstallReq;
	InstallReq["CID"]= CodeID;
	InstallReq["AUTH"] = Authentication;
	InstallReq["EXP"] = GetTime() + DAY2S(10);
	InstallReq["SRC"] = &Source;

	CVariant InstallRes = pKad->Engine()->Install(InstallReq);

	QString InstallResTest = CXml::Serialize(InstallRes.ToQVariant());*/

	/*CUInt128 TargetID;
	CVariant ExecuteReq;
	ExecuteReq["CID"]= CVariant((byte*)NULL, 8);
	CVariant Call;
		Call["FX"] = "test";
		CVariant Arguments;
			Arguments["Test"] = L"Execution Test";
			Arguments["Echo"] = L"Beeep";
		Call["ARG"] = Arguments;
	ExecuteReq["REQ"].Append(Call);

	CVariant ExecuteRes = pKad->Engine()->Execute(ExecuteReq, TargetID);

	QString ExecuteResTest = CXml::Serialize(ExecuteRes.ToQVariant());*/

	/*CKadScript* pScript = pKad->Engine()->GetScript(CVariant((byte*)NULL, 8));
	ASSERT(pScript);

	pScript->ProcessPacket("test", CVariant(), CVariant());*/

//	while(!v8::V8::IdleNotification()) {}; // force garbage collection
	
#if 0
	int Taken = 0;
	int Dropped = 0;
	for(int i=0; i<300; i++)
	{
		CUInt128 ID;
		ID.SetDWord(0,GetRand64());
		ID.SetDWord(1,GetRand64());
		ID.SetDWord(2,GetRand64());
		ID.SetDWord(3,GetRand64());
		if(pKad->GetChild<CRoutingRoot>()->CanAdd(ID))
		{
			Taken++;
			CPointer<CKadNode> pTemp = new CKadNode();
			pTemp->GetID().SetValue(ID);
			pTemp->UpdateAddress(CSafeAddress(L"udt://127.0.0.1:0"));
			pKad->GetChild<CRoutingRoot>()->AddNode(pTemp);
		}
		else
		{
			Dropped++;
		}
	}

	//m_pSocket->SecureSession(Address);
	//m_pSocket->Rendevouz(Address);
	//Address.SetDirect(true);
	//m_pSocket->QueuePacket(":ECHO", CVariant(), Address);
#endif

#if 1
	CJSScript ScriptTest;
	wstring JavaScript = 
	L"function TestDebug(param)\r\n"
	L"{\r\n"
	L"	if('map' in param)\r\n"
	L"		debug.log('Variant: ' + param['map']['emtry']);\r\n"
	L"};\r\n"
	L"\r\n"
	L"'a' + 'b'\r\n"
	;

	map<string, CObject*> Objects;
	Objects["debug"] = new CDebugObj(L"test", NULL);
	ScriptTest.Initialize(JavaScript, L"", Objects);
	
	CVariant Variant;
	Variant["map"]["emtry"] = "MapEntry";
	CVariantPrx* pVariant = new CVariantPrx(Variant);

	vector<CPointer<CObject> > Arguments;
	Arguments.push_back(pVariant);
	CPointer<CObject> Return;
	ScriptTest.Call(string(""), "TestDebug", Arguments, Return);

	QString VariantTest = CXml::Serialize(pVariant->GetCopy().ToQVariant());

	//while(!v8::V8::IdleNotification()) {}; // force garbage collection
	



	if(0)
	{
	CJSScript ScriptTest;
	wstring JavaScript = 
	L"function TestDebug(param)\r\n"
	L"{\r\n"
	L"	debug.log('Variant: ' + param['map']['emtry']);\r\n"
	L"	debug.log('Variant: ' + param['str']);\r\n"
	/*L"	debug.log('Variant: ' + param['int']);\r\n"
	L"	debug.log('Variant: ' + param['num']);\r\n"
	L"	debug.log('Variant: ' + param['bin']);\r\n"*/
	L"	debug.log('Variant: ' + param['arr'][1]['x']);\r\n"
	L"	param['tmp'] = 'test1';\r\n"
	L"	param['tmp'] = 'test2';\r\n"
	L"	param['map']['test'] = 'MapTest';\r\n"
	L"	var variant = new CVariant(); \r\n"
	L"	variant.append('test');\r\n"
	L"	variant.append('test');\r\n"
	L"	variant.append('test');\r\n"
	L"	param['map']['sub'] = variant;\r\n"
	L"	delete param['bin'];\r\n"
	L"	\r\n"
	L"	\r\n"
	L"	var key = new CCryptoKey(); \r\n"
	L"	key.Make('', 16);\r\n"
	L"	var iv = new CCryptoKey(); \r\n"
	L"	param['map']['sub'].encrypt(key,iv);\r\n"
	L"	\r\n"
	L"	\r\n"
	L"	var priv = new CCryptoKey(); \r\n"
	L"	var pub = priv.Make('ECP', 'secp128r1');\r\n"
	L"	param.sign(priv);\r\n"
	L"	var ok = param.verify(pub);\r\n"
	L"	debug.log('verify: ' + ok);\r\n"
	L"	var ret = 0;\r\n"
	L"	debugger;\r\n"
	L"	return ret;\r\n"
	L"};\r\n"
	L"\r\n"
	L"'a' + 'b'\r\n"
	;
	map<string, CObject*> Objects;
	Objects["debug"] = new CDebugObj(L"test", NULL);
	ScriptTest.Initialize(JavaScript, L"", Objects);
	
	CVariant Variant1 = "Test";
	CVariant Variant2 = 123;
	CVariant Variant3 = 0.12;
	CVariant Variant4((byte*)"\xFF\xAA\x12",3);

	CVariant VariantX;
	VariantX["x"] = "XXX";

	CVariant Variant;
	Variant["map"]["emtry"] = "MapEntry";
	Variant["str"] = Variant1;
	Variant["int"] = Variant2;
	Variant["num"] = Variant3;
	Variant["bin"] = Variant4;
	Variant["arr"].Append(0);
	Variant["arr"].Append(VariantX);
	CVariantPrx* pVariant = new CVariantPrx(Variant);

	vector<CPointer<CObject> > Arguments;
	Arguments.push_back(pVariant);
	/*Arguments.push_back(new CVariantPrx(Variant1));
	Arguments.push_back(new CVariantPrx(Variant2));
	Arguments.push_back(new CVariantPrx(Variant3));
	Arguments.push_back(new CVariantPrx(Variant4));*/
	CPointer<CObject> Return;
	ScriptTest.Call(string(""), "TestDebug", Arguments, Return);

	QString VariantTest = CXml::Serialize(pVariant->GetCopy().ToQVariant());

	return;
	//while(!v8::V8::IdleNotification()) {}; // force garbage collection
	}

#endif
}

#include "../sqlite3/sqlite3.h"

void test()
{
	/*CVariant XXX;
	XXX["a"] = 1;
	XXX["b"]["c"] = CUInt128(true);

	CBuffer Packet;
	XXX.ToPacket(&Packet);
	

	CVariant xxx;
	Packet.SetPosition(0);
	xxx.FromPacket(&Packet);

	CUInt128 x = xxx["b"]["c"];*/
	return ;

	/*sqlite3 *db;

	sqlite3_stmt    *res;
	int             rec_count = 0;
	const char      *errMSG;
	const char      *tail;

	int error = sqlite3_open("F:\\Projects\\Filesharing\\NeoLoader\\NeoLoader\\Debug\\Config\\Crawling\\test.cache.sq3", &db);
	if (error)
	{
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
	}

	error = sqlite3_prepare_v2(db, "SELECT * FROM aux", 1000, &res, &tail);

	if (error != SQLITE_OK)
	{
		puts("We did not get any data!");
		return;
	}

	puts("==========================");

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		printf("%d|", sqlite3_column_text(res, 0));
		printf("%s|", sqlite3_column_text(res, 1));
		printf("%s|", sqlite3_column_text(res, 2));
		printf("%d\n", sqlite3_column_int(res, 3));

		rec_count++;
	}

	puts("==========================");
	printf("We received %d records.\n", rec_count);

error = sqlite3_exec(db,
    "INSERT INTO command(status,level,priority,squad_name,command,target) VALUES (0,2,1.0,\'gonzo.roam\',\'hunt\',\'jerr\')",
    0, 0, 0);

error = sqlite3_exec(db,
    "UPDATE command SET status = 0, priority=1.0 WHERE row_id = 5",
    0, 0, 0);

error = sqlite3_exec(db,
    "DELETE FROM command WHERE target = \'jerr\' AND priority > 1.0",
    0, 0, 0);


	sqlite3_finalize(res);
	sqlite3_close (db); */

return;


/*CBuffer TestBuff;
	//TestBuff.AllocBuffer(16*1024,false);
	for(int i=0; i <=5; i++ )
	{
		QFile InPacket;
		InPacket.setFileName("InStream_" + QString::number(i) + ".bin");
		InPacket.open(QFile::ReadOnly);
		QByteArray Data = InPacket.readAll();
		InPacket.close();

		TestBuff.SetData(-1, Data.data(), Data.length());
	}{
		while(TestBuff.GetSize() > 0)
		{
			qDebug() << TestBuff.GetSize();
			string Name;
			CVariant Packet;
			if(!StreamPacket(TestBuff, Name, Packet))
				break; // incomplete

			if(Packet.GetType() != CVariant::EMap)
				LogLine(LOG_ERROR, L"BAM!!!!");
		}
	}*/

	{
	uint64 ui1 = 0xFFFFFFFFFFFFFFFF;
	uint32 _i1 = ui1;
	uint64 ui2 = 0;
	uint32 _i2 = ui2;
	uint64 ui3 = 0;
	uint32 _i3 = ui3;

	CUInt128 u1;
	memcpy(u1.GetData(), QByteArray::fromHex("8CECAD166305362792D868C3C4C366E6").data(), 8); // further

	/*double test = 0;
	for(int i=0; i<128; i++)
	{
		test += ((double)u1.GetBit(i)) * pow(2.0,i/2);
	}

	ui2 = test;*/
	CUInt128 u2;
	memcpy(u2.GetData(), QByteArray::fromHex("BA43013827A72DED0A695E58405083E8").data(), 8); // closer
	
	CUInt128 u3;
	//u3 = u1;
	memcpy(u3.GetData(), QByteArray::fromHex("C0D0D390D65C44D3471EAE51CBC3F9C1").data(), 8); // target

	CUInt128 d1 = u1 ^ u3;
	CUInt128 d2 = u2 ^ u3;

	/*bool a = (d1 < d2);
	double x1 = SampleDown(d1);
	double x2 = SampleDown(d2);
	bool b = (x1 < x2);*/


	u2 = u1;
	u3 = u1;
	u2.ShiftLeft(64);

	QByteArray H1;
	for(int i=u1.GetSize()-1; i >= 0; i--)
		H1.append(QString::number(u1.GetData()[i],16).rightJustified(2,'0'));
	QString B1;
	for(int i=u1.GetDWordCount()-1; i >= 0; i--)
		B1.append(QString::number(u1.GetDWord(i),2).rightJustified(32,'0'));

	QByteArray H2;
	for(int i=u2.GetSize()-1; i >= 0; i--)
		H2.append(QString::number(u2.GetData()[i],16).rightJustified(2,'0'));
	QString B2;
	for(int i=u2.GetDWordCount()-1; i >= 0; i--)
		B2.append(QString::number(u2.GetDWord(i),2).rightJustified(32,'0'));

	QByteArray H3;
	for(int i=u3.GetSize()-1; i >= 0; i--)
		H3.append(QString::number(u3.GetData()[i],16).rightJustified(2,'0'));
	QString B3;
	for(int i=u3.GetDWordCount()-1; i >= 0; i--)
		B3.append(QString::number(u3.GetDWord(i),2).rightJustified(32,'0'));

	QString X3;
	for(int i=u3.GetBitSize()-1; i >= 0; i--)
		X3.append(u3.GetBit(i) ? "1" : "0");


	/*for(int i=0; i < 128; i+=1)
	{
		CUInt128 ux = u2;
		ux.ShiftRight(i);

		QByteArray Hx;
		for(int i=ux.GetSize()-1; i >= 0; i--)
			Hx.append(QString::number(ux.GetData()[i],16).rightJustified(2,'0'));
		QString Bx;
		for(int i=ux.GetDWordCount()-1; i >= 0; i--)
			Bx.append(QString::number(ux.GetDWord(i),2).rightJustified(32,'0'));
		qDebug() << Hx << " - " << Bx;
	}*/

	}

	{
	CPrivateKey PrivKey(CAbstractKey::eECP);
	PrivKey.GenerateKey("secp128r1");

	CVariant Variant;

	wstring I = L"bla bla bla bla bla";
	wstring J = L"blup blup blup blup blup";
	//Variant["test"] = I;
	//Variant.Append(I);

	CVariant VariantMap;
	VariantMap.Append(J);

	CVariant VariantEx = VariantMap;
	VariantEx.Append(I);

	//VariantEx.Hash(eSHA, KEY_256BIT);

	
	VariantEx.Sign(&PrivKey);

	CAbstractKey SymKey;
	SymKey.SetKey(QByteArray("1234567890ABCDEF"));

	//VariantEx.Encrypt(&SymKey);

	//VariantEx.Decrypt(&SymKey);

	//VariantEx.Encrypt(PrivKey.PublicKey());

	//VariantEx.Decrypt(&PrivKey);

	CBuffer Buffer;
	VariantEx.ToPacket(&Buffer);
	Buffer.SetPosition(0);
	VariantEx.FromPacket(&Buffer);

	CPublicKey* pKey = PrivKey.PublicKey();
	bool Ok = VariantEx.Verify(pKey);
	ASSERT(Ok);

	//string i = VariantEx["test"][0];
	//string i = Variant[0];
	QVariant qVaraint = VariantEx.ToQVariant();

	QString Test = CXml::Serialize(qVaraint);

	CVariant cVariant;
	cVariant.FromQVariant(qVaraint);

	bool bOk = cVariant.Verify(PrivKey.PublicKey());

	ASSERT(bOk);
	}
}
#endif

