#include "GlobalHeader.h"
#include "CoreServer.h"
#include "../NeoCore.h"
#include "../NeoVersion.h"
#include "../FileTransfer/FileGrabber.h"
#include "../FileList/FileList.h"
#include "../FileList/IOManager.h"
#include "../FileList/Hashing/HashingThread.h"
#include "../FileList/Hashing/FileHashTreeEx.h"
#include "../FileList/FileManager.h"
#include "../FileList/File.h"
#include "../FileList/FileStats.h"
#include "../FileList/FileDetails.h"
#include "../FileTransfer/Transfer.h"
#include "WebRoot.h"
#include "../GUI/WebUI.h"
#include "WebAPI.h"
#include <QCoreApplication>
#include <QNetworkInterface>
#include "CoreBus.h"
#include "../FileList/PartMap.h"
#ifdef CRAWLER
#include "../FileTransfer/HosterTransfer/CrawlerManager.h"
#endif
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/WebEngine.h"
#include "../FileTransfer/HosterTransfer/WebManager.h"
#include "../FileTransfer/HosterTransfer/WebCrawler.h"
#include "../FileTransfer/HosterTransfer/WebDebugger/DebugManager.h"
#include "../FileTransfer/HosterTransfer/WebDebugger/DebugTask.h"
#include "../FileTransfer/HosterTransfer/WebTask.h"
#include "../FileTransfer/HosterTransfer/WebSession.h"
#include "../FileTransfer/HosterTransfer/ScriptUpdater.h"
#include "../FileTransfer/HosterTransfer/CaptchaSolver.h"
#include "../FileTransfer/HosterTransfer/LinkGrabber.h"
#include "../FileTransfer/HosterTransfer/ContainerDecoder.h"
#include "../FileTransfer/HosterTransfer/LoginManager.h"
#include "../FileTransfer/HosterTransfer/ConnectorManager.h"
#include "../FileTransfer/HosterTransfer/ArchiveDownloader.h"
#include "../FileTransfer/HosterTransfer/ArchiveUploader.h"
#include "../FileTransfer/HosterTransfer/ArchiveSet.h"
#endif
#include "../FileTransfer/FileGrabber.h"
#ifdef CRAWLER
#include "../FileSearch/SiteCrawling/CrawlingManager.h"
#include "../FileSearch/SiteCrawling/CrawlingSite.h"
#include "../FileSearch/SiteCrawling/KadBlaster/KadBlaster.h"
#include "../FileSearch/SiteCrawling/KadBlaster/KeywordBlaster.h"
#include "../FileSearch/SiteCrawling/KadBlaster/LinkBlaster.h"
#include "../FileSearch/SiteCrawling/KadBlaster/RatingBlaster.h"
#include "../FileList/Archiving/FileArchiver.h"
#endif
#include "../FileTransfer/UploadManager.h"
#include "../FileTransfer/DownloadManager.h"
#include "../FileTransfer/BitTorrent/TorrentManager.h"
#include "../FileTransfer/BitTorrent/TorrentClient.h"
#include "../FileTransfer/BitTorrent/TorrentServer.h"
#include "../FileTransfer/BitTorrent/TorrentInfo.h"
#include "../FileTransfer/BitTorrent/Torrent.h"
#include "../FileTransfer/BitTorrent/TorrentTracker/TrackerClient.h"
#include "../FileTransfer/ed2kMule/MuleManager.h"
#include "../FileTransfer/ed2kMule/MuleServer.h"
#include "../FileTransfer/ed2kMule/MuleKad.h"
#include "../FileTransfer/ed2kMule/MuleCollection.h"
#include "../FileTransfer/ed2kMule/ServerClient/ServerList.h"
#include "../FileTransfer/ed2kMule/ServerClient/Ed2kServer.h"
#include "../FileSearch/SearchManager.h"
#include "../FileSearch/Search.h"
#include "../FileSearch/Collection.h"
#include "../FileSearch/SearchAgent.h"
#include "../Networking/SocketThread.h"
#include "../Networking/BandwidthControl/BandwidthLimit.h"
#include "../Networking/Pinger.h"
#include "../Interface/InterfaceManager.h"
#include "../FileTransfer/NeoShare/NeoManager.h"
#include "../FileTransfer/NeoShare/NeoClient.h"
#include "../FileTransfer/NeoShare/NeoKad.h"
#include "../FileTransfer/NeoShare/NeoRoute.h"
#include "../FileTransfer/NeoShare/NeoKad/KadPublisher.h"
#include "../FileTransfer/PeerWatch.h"
#include "../FileTransfer/HashInspector.h"
#ifdef _DEBUG
#include "../../Framework/Xml.h"
#include <QApplication>
#include <QClipboard>
#endif
#include "NeoFS.h"

CCoreServer::CCoreServer(QObject* qObject)
 : CIPCServer(qObject)
{
	LocalListen(theCore->Cfg(false)->GetString("Core/LocalName"));
	RemoteListen(theCore->Cfg(false)->GetUInt("Core/RemotePort"));

	/*QString BusName = theCore->Cfg(false)->GetString("Core/BusName");
	quint16 BusPort = theCore->Cfg(false)->GetUInt("Core/BusPort");
	if(!BusName.isEmpty() || BusPort != 0)
	{
		m_Bus = new CCoreBus(BusName, BusPort);
		m_Bus->setParent(this);
	}
	else
		m_Bus = NULL;*/

	m_WebRoot = new CWebRoot(this);
	m_WebUI = new CWebUI(this);
	m_WebAPI = new CWebAPI(this);

#ifndef __APPLE__
    QString FuseMount = theCore->Cfg()->GetString("Content/FuseMount");
    m_NeoFS = FuseMount.isEmpty() ? NULL : new CNeoFS(FuseMount, this);
#endif
}

CCoreServer::~CCoreServer()
{
	foreach(SStateCache* pCache, m_StatusCache)
		delete pCache;
	m_StatusCache.clear();
}

QString CCoreServer::CheckLogin(const QString &UserName, const QString &Password)
{
	//if(theCore->TestLogin(UserName, Password))
	if(theCore->TestLogin(Password))
		return theCore->GetLoginToken();
	return "";
}


#define RESPONSE(y, x){ \
QVariantMap Response; \
Response[y] = x; \
return Response; \
}

#define SMPL_RESPONSE(x) {RESPONSE("Result",x)}

QVariant CCoreServer::ProcessRequest(const QString& Command, const QVariant& Parameters)
{
	ASSERT(theCore->thread() == QThread::currentThread());	// Note: the ProcessRequest can access variouse not thread safe parts of the core.

	if((rand() % 100) == 0)
	{
		uint64 uNow = GetCurTick();
		for(QMap<uint64, SStateCache*>::iterator I = m_StatusCache.begin(); I != m_StatusCache.end(); )
		{
			if(uNow - I.value()->LastUpdate > SEC2MS(20))
			{
				delete I.value();
				I = m_StatusCache.erase(I);
			}
			else
				I++;
		}
	}


	QVariantMap Request = Parameters.toMap();
	QVariantMap Response;

	QVariant UID;
	if(Request.contains("UID"))
		UID = Request.take("UID");

	QList<CLog::SLine> Log;
	if(Request["Log"].toBool())
		theCore->SetLogInterceptor(&Log);

	if(Command == "Console")					Response = Console(Request);
	else if(Command == "GetInfo")				Response = GetInfo(Request);
	else if(Command == "GetLog")				Response = GetLog(Request);
	else if(Command == "Shutdown")				Response = Shutdown(Request);
	else if(Command == "FileList")				Response = FileList(Request);
	else if(Command == "GetProgress")			Response = GetProgress(Request);
	else if(Command == "GetFile")				Response = GetFile(Request);
	else if(Command == "SetFile")				Response = SetFile(Request);
	else if(Command == "AddFile")				Response = AddFile(Request);
	else if(Command == "FileAction")			Response = FileAction(Request);
	else if(Command == "FileIO")				Response = FileIO(Request);
	else if(Command == "GetTransfers")			Response = GetTransfers(Request);
	else if(Command == "TransferAction")		Response = TransferAction(Request);
	else if(Command == "GetClients")			Response = GetClients(Request);
#ifndef NO_HOSTERS
	else if(Command == "GetHosting")			Response = GetHosting(Request);
#endif

	else if(Command == "GetCore")				Response = GetCore(Request);
	else if(Command == "SetCore")				Response = SetCore(Request);
	else if(Command == "CoreAction")			Response = CoreAction(Request);

	else if(Command == "ServerList")			Response = ServerList(Request);
	else if(Command == "ServerAction")			Response = ServerAction(Request);

	else if(Command == "ServiceList")			Response = ServiceList(Request);
	else if(Command == "GetService")			Response = GetService(Request);
	else if(Command == "SetService")			Response = SetService(Request);
	else if(Command == "ServiceAction")			Response = ServiceAction(Request);

	else if(Command == "WebTaskList")			Response = WebTaskList(Request);
	else if(Command == "GrabLinks")				Response = GrabLinks(Request);
	else if(Command == "MakeLink")				Response = MakeLink(Request);
	else if(Command == "GrabberList")			Response = GrabberList(Request);
	else if(Command == "GrabberAction")			Response = GrabberAction(Request);

#ifdef CRAWLER
	else if(Command == "CrawlerList")			Response = CrawlerList(Request);
	else if(Command == "CrawlerAction")			Response = CrawlerAction(Request);
	else if(Command == "GetCrawler")			Response = GetCrawler(Request);
	else if(Command == "SetCrawler")			Response = SetCrawler(Request);
#endif

	else if(Command == "SearchList")			Response = SearchList(Request);
	else if(Command == "StartSearch")			Response = StartSearch(Request);
	else if(Command == "StopSearch")			Response = StopSearch(Request);

	else if(Command == "DiscoverContent")		Response = DiscoverContent(Request);
	else if(Command == "FetchContents")			Response = FetchContents(Request);
	else if(Command == "GetContent")			Response = GetContent(Request);
	else if(Command == "GetStream")				Response = GetStream(Request);
	

	else if(Command == "GetCaptchas")			Response = GetCaptchas(Request);
	else if(Command == "SetSolution")			Response = SetSolution(Request);

#ifndef NO_HOSTERS
	else if(Command == "DbgAddTask")			Response = theCore->m_WebManager->GetDebugger()->DbgAddTask(Request);
	else if(Command == "DbgRemoveTask")			Response = theCore->m_WebManager->GetDebugger()->DbgRemoveTask(Request);
	else if(Command == "DbgCommand")			Response = theCore->m_WebManager->GetDebugger()->DbgCommand(Request);
#endif

	else if(Command == "GetTrackerSummary")		Response["Result"] = theTracker.Summary();
	else if(Command == "GetTrackerReport")		Response["Result"] = theTracker.Report();
	else if(Command == "Test")					Response = Test(Request);
	else										Response["Error"] = "Invalid Command";

	if(UID.isValid())
		Response["UID"] = UID;

	if(Request["Log"].toBool())
	{
		theCore->SetLogInterceptor(NULL);

		QVariantList LogList = DumpLog(Log);
		if(!LogList.isEmpty())
			Response["Log"] = LogList;
	}

	return Response;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Main Secion, Console, Log, Settings
//

/**
* The Console function is a command proxy, 
*
* Request:
*	{
*		"Command":		String; Command
*		"Parameters":	Variant; Request Parameters
* or
*		"Commands":
*		[Request ,...]
*	}
*
* Response:
*	{
*		"Command":		String; Command
*		"Parameters":	Variant; Response Parameters
* or
*		"Results":
*		[Response ,...]
*	}
*
*	Note: all other parameters sent in the request are sent back unchanged in the response, this applyes also for the command/result lists
*/
QVariantMap CCoreServer::Console(QVariantMap Request)
{
	if(Request.contains("Commands"))
	{
		QVariantList Results;
		foreach(const QVariant& Command, Request["Commands"].toList())
			Results.append(Console(Command.toMap()));
		Request.remove("Commands");
		Request["Results"] = Results;
	}
	else if(Request["Parameters"].canConvert(QVariant::String))
	{
		// Text Console
	}
	else if(Request.contains("Command"))
		Request["Parameters"] = ProcessRequest(Request["Command"].toString(), Request["Parameters"]);
	return Request;
}

/**
* The GetInfo returns core version and privilegs set
*
* Request: 
*	{
*		"Version":		String; (GuiName) (Version)
*	}
*
* Response:
*	{
*		"Version":		String; (CoreName) (Version)
*		"Privileges":	String; space separated set of provileg diviations form default (for now defailt is everything)
*		
*		"P2P": 
*		{
*			"Ed2kMule":
*			{
*				"TCPPort":		Int;	TCPPort
*				"UDPPort":		Int;	UDPPort
*				"Firewalled":	Bool;	true/false
*				"KadStatus":	String;	"Disabled"/"Connected"/"Connecting"/"Disconnected"
*				"KadPort":		Int;	KADPort as seen externaly (NAT) if UDP Open and firewalled
*				"Address":		String;	seen external IP address
*				"KadStats":
*				{
*					TotalUsers":
*					TotalFiles":
*					IndexedSource":
*					IndexedKeyword":
*					IndexedNotes":
*					IndexLoad":
*				}
*			}
*			"BitTorrent":
*			{
*				"Port":		Int;	TCPPort
*			}
*		}
*	}
*/
QVariantMap CCoreServer::GetInfo(QVariantMap Request)
{
	QVariantMap Response;
	Response["Version"] = GetNeoVersion();
	//if(Request["Version"] != ...) // ToDo:
	//	Response["Incompatible"] = true;
	//Response["Privileges"] = "-index"
#ifdef _DEBUG
	Response["Debug"] = true;
#endif

	QVariantMap Bandwidth;
		Bandwidth["UpRate"] = theCore->m_Network->GetUpLimit()->GetRate();
		Bandwidth["UploadAck"] = theCore->m_Network->GetUpLimit()->GetRate(CBandwidthCounter::eAck);
		Bandwidth["UploadHdr"] = theCore->m_Network->GetUpLimit()->GetRate(CBandwidthCounter::eHeader);
		Bandwidth["UploadEx"] = theCore->m_Network->GetUpLimit()->GetRate(CBandwidthCounter::eProtocol);
		Bandwidth["Upload"] = theCore->m_Network->GetUpLimit()->GetRate(CBandwidthCounter::ePayload);
		Bandwidth["UpLimit"] = theCore->m_Network->GetUpLimit()->GetLimit();
#ifdef NAFC
		Bandwidth["UpTotal"] = theCore->m_Network->GetNafcUp()->GetRate();
#endif
		//Bandwidth["UpVar"] = theCore->m_Network->GetUpLimit()->GetDeviation();
		Bandwidth["DownRate"] = theCore->m_Network->GetDownLimit()->GetRate();
		Bandwidth["DownloadAck"] = theCore->m_Network->GetDownLimit()->GetRate(CBandwidthCounter::eAck);
		Bandwidth["DownloadHdr"] = theCore->m_Network->GetDownLimit()->GetRate(CBandwidthCounter::eHeader);
		Bandwidth["DownloadEx"] = theCore->m_Network->GetDownLimit()->GetRate(CBandwidthCounter::eProtocol);
		Bandwidth["Download"] = theCore->m_Network->GetDownLimit()->GetRate(CBandwidthCounter::ePayload);
		Bandwidth["DownLimit"] = theCore->m_Network->GetDownLimit()->GetLimit();
#ifdef NAFC
		Bandwidth["DownTotal"] = theCore->m_Network->GetNafcDown()->GetRate();
#endif
		//Bandwidth["DownVar"] = theCore->m_Network->GetDownLimit()->GetDeviation();
		Bandwidth["UploadedTotal"] = theCore->m_Network->GetStats().UploadedTotal;
		Bandwidth["UploadedSession"] = theCore->m_Network->GetStats().UploadedSession;
		Bandwidth["DownloadedTotal"] = theCore->m_Network->GetStats().DownloadedTotal;
		Bandwidth["DownloadedSession"] = theCore->m_Network->GetStats().DownloadedSession;
		Bandwidth["Connections"] = theCore->m_Network->GetCount();
		Bandwidth["ActiveUploads"] = theCore->m_UploadManager->GetActiveCount();
		Bandwidth["WaitingUploads"] = theCore->m_UploadManager->GetWaitingCount();
		Bandwidth["ActiveDownloads"] = theCore->m_DownloadManager->GetActiveCount();
		Bandwidth["WaitingDownloads"] = theCore->m_DownloadManager->GetWaitingCount();
		QVariantMap Pinger;
		Pinger["Host"] = theCore->m_Pinger->GetHost().toString();
		Pinger["TTL"] = theCore->m_Pinger->GetTTL();
		Pinger["Average"] = theCore->m_Pinger->GetAveragePing();
		Pinger["Lowest"] = theCore->m_Pinger->GetLowestPing();
		Bandwidth["Pinger"] = Pinger;
		Bandwidth["DefaultNIC"] = theCore->Cfg()->GetString("Bandwidth/DefaultNIC");
		QVariantMap PeerWatch;
		PeerWatch["Alive"] = theCore->m_PeerWatch->AliveCount();
		PeerWatch["Dead"] = theCore->m_PeerWatch->DeadCount();
		PeerWatch["Banned"] = theCore->m_PeerWatch->BannedCount();
		Bandwidth["PeerWatch"] = PeerWatch;
	Response["Bandwidth"] = Bandwidth;

	QVariantMap Networks;


	QVariantMap NeoShare;
	NeoShare["Anonymity"] = theCore->Cfg()->GetInt("NeoShare/Anonymity");
	NeoShare["UpRate"] = theCore->m_NeoManager->GetKad()->GetUpRate();
	NeoShare["DownRate"] = theCore->m_NeoManager->GetKad()->GetDownRate();

	NeoShare["KadStatus"] = theCore->m_NeoManager->GetKad()->GetStatus();
	NeoShare["Port"] = theCore->m_NeoManager->GetKad()->GetPort();
	NeoShare["Firewalled"] = theCore->m_NeoManager->GetKad()->IsFirewalled(CAddress::IPv4);
	NeoShare["NATed"] = theCore->m_NeoManager->GetKad()->IsFirewalled(CAddress::IPv4, true);
	NeoShare["Address"] = theCore->m_NeoManager->GetKad()->GetAddress(CAddress::IPv4).ToQString();
	CAddress IPv6 = theCore->m_NeoManager->GetKad()->GetAddress(CAddress::IPv6);
	if(!IPv6.IsNull())
	{
		NeoShare["Firewalled"] = theCore->m_NeoManager->GetKad()->IsFirewalled(CAddress::IPv6);
		NeoShare["NATed"] = theCore->m_NeoManager->GetKad()->IsFirewalled(CAddress::IPv6, true);
		NeoShare["Address"] = theCore->m_NeoManager->GetKad()->GetAddress(CAddress::IPv6).ToQString();
	}

	if(theCore->Cfg()->GetBool("NeoShare/Enable"))
	{
		NeoShare["UploadEx"] = theCore->m_NeoManager->GetUpLimit()->GetRate(CBandwidthCounter::eProtocol);
		NeoShare["Upload"] = theCore->m_NeoManager->GetUpLimit()->GetRate(CBandwidthCounter::ePayload);
		NeoShare["DownloadEx"] = theCore->m_NeoManager->GetDownLimit()->GetRate(CBandwidthCounter::eProtocol);
		NeoShare["Download"] = theCore->m_NeoManager->GetDownLimit()->GetRate(CBandwidthCounter::ePayload);

		NeoShare["UploadedTotal"] = theCore->m_NeoManager->GetStats().UploadedTotal;
		NeoShare["UploadedSession"] = theCore->m_NeoManager->GetStats().UploadedSession;
		NeoShare["DownloadedTotal"] = theCore->m_NeoManager->GetStats().DownloadedTotal;
		NeoShare["DownloadedSession"] = theCore->m_NeoManager->GetStats().DownloadedSession;

		NeoShare["Sessions"] = theCore->m_NeoManager->GetSessionCount();
		QVariantList Routes;
		foreach(CNeoRoute* pRoute, theCore->m_NeoManager->GetAllRoutes())
		{
			QVariantMap Route;
			Route["EntityID"] = pRoute->GetEntityID();
			Route["TargetID"] = pRoute->GetTargetID();
			if(pRoute->IsStatic())
				Route["IsStatic"] = true;
			else
			{
				uint64 uNow = GetCurTick();
				if(pRoute->GetTimeOut() > uNow)
					Route["TimeOut"] = pRoute->GetTimeOut() - uNow;
				else
					Route["TimeOut"] = 0;
			}
			Routes.append(Route);
		}
		NeoShare["Routes"] = Routes;
	}

	Networks["NeoShare"] = NeoShare;

	if(theCore->Cfg()->GetBool("Ed2kMule/Enable"))
	{
		QVariantMap Ed2kMule;
		Ed2kMule["UpRate"] = theCore->m_MuleManager->GetServer()->GetUpLimit()->GetRate();
		Ed2kMule["UploadAck"] = theCore->m_MuleManager->GetServer()->GetUpLimit()->GetRate(CBandwidthCounter::eAck);
		Ed2kMule["UploadHdr"] = theCore->m_MuleManager->GetServer()->GetUpLimit()->GetRate(CBandwidthCounter::eHeader);
		Ed2kMule["UploadEx"] = theCore->m_MuleManager->GetServer()->GetUpLimit()->GetRate(CBandwidthCounter::eProtocol);
		Ed2kMule["Upload"] = theCore->m_MuleManager->GetServer()->GetUpLimit()->GetRate(CBandwidthCounter::ePayload);
		Ed2kMule["DownRate"] = theCore->m_MuleManager->GetServer()->GetDownLimit()->GetRate();
		Ed2kMule["DownloadAck"] = theCore->m_MuleManager->GetServer()->GetDownLimit()->GetRate(CBandwidthCounter::eAck);
		Ed2kMule["DownloadHdr"] = theCore->m_MuleManager->GetServer()->GetDownLimit()->GetRate(CBandwidthCounter::eHeader);
		Ed2kMule["DownloadEx"] = theCore->m_MuleManager->GetServer()->GetDownLimit()->GetRate(CBandwidthCounter::eProtocol);
		Ed2kMule["Download"] = theCore->m_MuleManager->GetServer()->GetDownLimit()->GetRate(CBandwidthCounter::ePayload);
		Ed2kMule["UploadedTotal"] = theCore->m_MuleManager->GetStats().UploadedTotal;
		Ed2kMule["UploadedSession"] = theCore->m_MuleManager->GetStats().UploadedSession;
		Ed2kMule["DownloadedTotal"] = theCore->m_MuleManager->GetStats().DownloadedTotal;
		Ed2kMule["DownloadedSession"] = theCore->m_MuleManager->GetStats().DownloadedSession;
		Ed2kMule["TCPPort"] = theCore->m_MuleManager->GetServer()->GetPort();
		Ed2kMule["UDPPort"] = theCore->m_MuleManager->GetServer()->GetUTPPort();
		Ed2kMule["Firewalled"] = theCore->m_MuleManager->IsFirewalled(CAddress::IPv4);
		Ed2kMule["NATed"] = theCore->m_MuleManager->GetKad()->IsUDPOpen();
		Ed2kMule["Address"] = theCore->m_MuleManager->GetAddress(CAddress::IPv4).ToQString();
		CAddress IPv6 = theCore->m_MuleManager->GetAddress(CAddress::IPv6);
		if(!IPv6.IsNull())
		{
			Ed2kMule["FirewalledV6"] = theCore->m_MuleManager->IsFirewalled(CAddress::IPv6);
			//Ed2kMule["NATedV6"]
			Ed2kMule["AddressV6"] = IPv6.ToQString();
		}
		Ed2kMule["Connections"] = theCore->m_MuleManager->GetConnectionCount();
	
		Ed2kMule["KadStatus"] = theCore->m_MuleManager->GetKad()->GetStatus();
		Ed2kMule["KadPort"] = theCore->m_MuleManager->GetKad()->GetKadPort();
		Ed2kMule["KadStats"] = theCore->m_MuleManager->GetKad()->GetKadStats();

		Networks["Ed2kMule"] = Ed2kMule;
	}

	if(theCore->Cfg()->GetBool("BitTorrent/Enable"))
	{
		QVariantMap BitTorrent;
		BitTorrent["UpRate"] = theCore->m_TorrentManager->GetServer()->GetUpLimit()->GetRate();
		BitTorrent["UploadAck"] = theCore->m_TorrentManager->GetServer()->GetUpLimit()->GetRate(CBandwidthCounter::eAck);
		BitTorrent["UploadHdr"] = theCore->m_TorrentManager->GetServer()->GetUpLimit()->GetRate(CBandwidthCounter::eHeader);
		BitTorrent["UploadEx"] = theCore->m_TorrentManager->GetServer()->GetUpLimit()->GetRate(CBandwidthCounter::eProtocol);
		BitTorrent["Upload"] = theCore->m_TorrentManager->GetServer()->GetUpLimit()->GetRate(CBandwidthCounter::ePayload);
		BitTorrent["DownRate"] = theCore->m_TorrentManager->GetServer()->GetDownLimit()->GetRate();
		BitTorrent["DownloadAck"] = theCore->m_TorrentManager->GetServer()->GetDownLimit()->GetRate(CBandwidthCounter::eAck);
		BitTorrent["DownloadHdr"] = theCore->m_TorrentManager->GetServer()->GetDownLimit()->GetRate(CBandwidthCounter::eHeader);
		BitTorrent["DownloadEx"] = theCore->m_TorrentManager->GetServer()->GetDownLimit()->GetRate(CBandwidthCounter::eProtocol);
		BitTorrent["Download"] = theCore->m_TorrentManager->GetServer()->GetDownLimit()->GetRate(CBandwidthCounter::ePayload);
		BitTorrent["UploadedTotal"] = theCore->m_TorrentManager->GetStats().UploadedTotal;
		BitTorrent["UploadedSession"] = theCore->m_TorrentManager->GetStats().UploadedSession;
		BitTorrent["DownloadedTotal"] = theCore->m_TorrentManager->GetStats().DownloadedTotal;
		BitTorrent["DownloadedSession"] = theCore->m_TorrentManager->GetStats().DownloadedSession;
		BitTorrent["Port"] = theCore->m_TorrentManager->GetServer()->GetPort();
		BitTorrent["Firewalled"] = theCore->m_TorrentManager->IsFirewalled(CAddress::IPv4);
		BitTorrent["NATed"] = theCore->m_TorrentManager->IsFirewalled(CAddress::IPv4, true);
		BitTorrent["Address"] = theCore->m_TorrentManager->GetAddress(CAddress::IPv4).ToQString();
		CAddress IPv6 = theCore->m_TorrentManager->GetAddress(CAddress::IPv6);
		if(!IPv6.IsNull())
		{
			BitTorrent["FirewalledV6"] = theCore->m_TorrentManager->IsFirewalled(CAddress::IPv6);
			BitTorrent["NATedV6"] = theCore->m_TorrentManager->IsFirewalled(CAddress::IPv6, true) == false;
			BitTorrent["AddressV6"] = IPv6.ToQString();
		}
		BitTorrent["Connections"] = theCore->m_TorrentManager->GetConnectionCount();

		BitTorrent["DHTStats"] = theCore->m_TorrentManager->GetDHTStatus();

		Networks["BitTorrent"] = BitTorrent;
	}

	Response["Networks"] = Networks;

#ifndef NO_HOSTERS
	QVariantMap Hosters;
		Hosters["Upload"] = theCore->m_WebManager->GetUpLimit()->GetRate(CBandwidthCounter::ePayload);
		Hosters["Download"] = theCore->m_WebManager->GetDownLimit()->GetRate(CBandwidthCounter::ePayload);
		Hosters["UploadedTotal"] = theCore->m_WebManager->GetStats().UploadedTotal;
		Hosters["UploadedSession"] = theCore->m_WebManager->GetStats().UploadedSession;
		Hosters["DownloadedTotal"] = theCore->m_WebManager->GetStats().DownloadedTotal;
		Hosters["DownloadedSession"] = theCore->m_WebManager->GetStats().DownloadedSession;
		Hosters["ActiveUploads"] = theCore->m_WebManager->GetUploads();
		Hosters["ActiveDownloads"] = theCore->m_WebManager->GetDownloads();
		Hosters["WebTasks"] = theCore->m_WebManager->GetActiveTasks();
		Hosters["WebProxy"] = theCore->Cfg()->GetString("Bandwidth/WebProxy");
	Response["Hosters"] = Hosters;
#endif

	QVariantMap IOStats;
	IOStats["PendingRead"] = theCore->m_IOManager->GetPendingReadSize();
	IOStats["PendingWrite"] = theCore->m_IOManager->GetPendingWriteSize();
	IOStats["HashingCount"] = theCore->m_Hashing->GetCount();
	IOStats["AllocationCount"] = theCore->m_IOManager->GetAllocationCount();
	Response["IOStats"] = IOStats;

	QStringList NICs;
	foreach(QNetworkInterface NIC, QNetworkInterface::allInterfaces())
	{
		if ((NIC.flags().testFlag(QNetworkInterface::IsLoopBack) && !NIC.flags().testFlag(QNetworkInterface::IsPointToPoint)))
			continue;
		if(!NIC.flags().testFlag(QNetworkInterface::IsUp))
			continue;

		QString NICName = NIC.humanReadableName();

		if(!NICs.contains(NICName))
			NICs.append(NICName);
	}
	Response["NICs"] = NICs;

	return Response;
}

/**
* The GetLog returns the currently available core log
*
* Request: 
*	{
*		"LastID": uint64; ID of the last already retrived log line
*	}
*
* Response:
*	{
*		"Lines":
*		[{
*			"ID":		uint64; unique LogLine ID
*			"Flag":		uint32; LogLine Flags
*			"Stamp":	time_t; LogLien Time Stamp
*			"Line":		String; LogLine Text
*		}
*		,...]
*	}
*/
QVariantMap CCoreServer::GetLog(QVariantMap Request)
{
	QList<CLog::SLine> Log;
	
	if(Request.contains("File"))
	{
		if(CFile* pFile = CFileList::GetFile(Request["File"].toULongLong()))
		{
			if(!Request.contains("Transfer"))
				Log = CLogger::Instance()->GetLog((uint64)pFile);
			else if(CTransfer* pTransfer = pFile->GetTransfer(Request["Transfer"].toULongLong()))
			{
#ifndef NO_HOSTERS
				if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
				{
					Log = CLogger::Instance()->GetLog((uint64)pHosterLink);
				}
				else 
#endif
					if(CTorrentPeer* pTorrentPeer = qobject_cast<CTorrentPeer*>(pTransfer))
				{
					Log = CLogger::Instance()->GetLog((uint64)pTorrentPeer);
				}
				else if(CMuleSource* pMuleSource = qobject_cast<CMuleSource*>(pTransfer))
				{
					if(CMuleClient* pClient = pMuleSource->GetClient())
						Log = CLogger::Instance()->GetLog((uint64)pClient);
				}
				else if(CNeoEntity* pNeoEntity = qobject_cast<CNeoEntity*>(pTransfer))
				{
					Log = CLogger::Instance()->GetLog((uint64)pNeoEntity);
				}
			}	
		}
	}
#ifndef NO_HOSTERS
	else if(Request.contains("Task"))
	{
		foreach(CWebTask* pTask, theCore->m_WebManager->GetAllTasks())
		{
			if((uint64)pTask == Request["Task"].toULongLong())
			{
				Log = CLogger::Instance()->GetLog((uint64)pTask);
				break;
			}
		}
	}
#endif
#ifdef CRAWLER
	else if(Request.contains("SiteName"))
	{
		if(CCrawlingSite* pSite = theCore->m_CrawlingManager->GetSite(Request["SiteName"].toString()))
			Log = pSite->GetLog();
	}
#endif
	else
		Log = CLogger::Instance()->GetLog();

	RESPONSE("Lines", DumpLog(Log, Request["LastID"].toULongLong()));
}


/**
* Shutdown terminates the core
*/
QVariantMap CCoreServer::Shutdown(QVariantMap Request)
{
	QTimer::singleShot(1000, QCoreApplication::instance(), SLOT(quit()));
	SMPL_RESPONSE("ok");
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	UI Section
//

#define UPDATE(name, value) \
		if(!pState || pState->name != (value)) { \
			if(pState) \
				pState->name = (value); \
			if(!pUpdate) \
				pUpdate = new QVariantMap(); \
			(*pUpdate)[STR(name)] = (value); \
		}

#define UPDATE_EX(name, value, ex) \
		if(!pState || pState->name != (value)) { \
			if(pState) \
				pState->name = (value); \
			if(!pUpdate) \
				pUpdate = new QVariantMap(); \
			(*pUpdate)[STR(name)] = ex(value); \
		}

/**
* FileList returns a file list that matches given selection criteries
*
* Request: 
*	{
*		"Status":		[String; Status, ...] acceptable states
*		"FileName":		String;	Filename wildcard selection
*		"GrabberID":	uint64; FileGrabber ID
*		"SearchID":		uint64; Search ID
*		"RootID":		uint64; ID of a file with sub files
*		"RootOnly":		bool; return only root files
*	}
*
* Response:	
*	{
*		Files:
*		[{
*			"ID":			uint64; ID
*			"FileName":		String;	file name
*			"Status":		String;	file status
*			"Type":			String;	file Type
*
*			"FileSize":		uint64;	file size
*
*			"DownRate":		uint64;	downlaod rante in bytes per second
*			"UpRate":		uint64;	upload rante in bytes per second
*
*			"Downloaded":	uint64;
*			"Verifyed":		uint64;
*			"Sheduled":		uint64;
*
*			"SubFiles":		[uint64; FileID, ...]
*		}
*		,...]
*	}
*
*/

struct SFileListSorter 
{
	enum ESorter
	{
		eUnknown = 0,
		eAvailability,
	};

	SFileListSorter(const QString& param){
		if(param == "Availability")
			Param = eAvailability;
		else
			Param = eUnknown;
	}
	bool operator() (CFile* L, CFile* R) 
	{
		switch(Param)
		{
			case eAvailability:
			{
				double l = L->GetStats()->GetAvailStats();
				if(!l) l = L->GetDetails()->GetAvailStats();

				double r = R->GetStats()->GetAvailStats();
				if(!r) r = R->GetDetails()->GetAvailStats();

				return l > r;
			}
			default: return L->GetFileName() < R->GetFileName();
		}
	}

	ESorter Param;
};

QVariantMap CCoreServer::FileList(QVariantMap Request)
{
	bool bAllExtended = true;
	bool bSellection = false;

	QList<CFile*> AllFiles;
	if(Request.contains("GrabberID"))
	{
		if(uint64 GrabberID = Request["GrabberID"].toULongLong())
			AllFiles = theCore->m_FileGrabber->GetFiles(GrabberID);
		else
			AllFiles = theCore->m_FileGrabber->GetFiles();
	}
	else if(Request.contains("SearchID"))
	{
		if(CSearch* pSearch = theCore->m_SearchManager->GetSearch(Request["SearchID"].toULongLong()))
			AllFiles = pSearch->GetFiles();
	}
	else if(Request.contains("RootID"))
	{
		if(CFile* pFile = CFileList::GetFile(Request["RootID"].toULongLong()))
		{
			foreach(uint64 FileID, pFile->GetSubFiles())
			{
				if(CFile* pSubFile = CFileList::GetFile(FileID))
					AllFiles.append(pSubFile);
			}	
		}
	}
	else if(Request.contains("SelectedList"))
	{
		bSellection = true;
		foreach(const QVariant& vID, Request["SelectedList"].toList())
		{
			if(CFile* pFile = CFileList::GetFile(vID.toULongLong()))
				AllFiles.append(pFile);
		}
	}
	else
	{
		AllFiles = theCore->m_FileManager->GetFiles();
	}



	bool bComplete = true;
	bool bIncomplete = true;
	bool bCompleted = true;
	QString FileStatus = Request["FileStatus"].toString();
	if(FileStatus == "Complete")
	{
		bIncomplete = false;
		bCompleted = false;
	}
	else if(FileStatus == "Completed")
	{
		bIncomplete = false;
		bComplete = false;
	}
	else if(FileStatus == "Incomplete")
	{
		bComplete = false;
	}

	CFile::EState State = CFile::eUnknown;
	QString FileState = Request["FileState"].toString();
	if(FileState == "Started")
		State = CFile::eStarted;
	else if(FileState == "Paused")
		State = CFile::ePaused;
	else if(FileState == "Stopped")
		State = CFile::eStopped;
			
	bool bRemoved = (FileState == "Removed");
	int iCustom = Request.contains("RootID") ? 2 : Request.contains("GrabberID") || Request.contains("SearchID") ? 1 : 0;

	bool bRootOnly = Request["RootOnly"].toBool();

	QList<CFile*> Files;
	foreach(CFile* pFile, AllFiles)
	{
		switch(State)
		{
			case CFile::eStarted:	if(!pFile->IsStarted() || pFile->IsPaused(true))	continue; break;
			case CFile::ePaused:	if(!pFile->IsPaused(true))							continue; break;
			case CFile::eStopped:	if(pFile->IsStarted())								continue; break;
		}

		if(iCustom == 0 && bRemoved != pFile->IsRemoved())
			continue;
		if(iCustom != 2 && pFile->IsHalted())
			continue;

		if(!(pFile->IsComplete(true) ? bComplete : bIncomplete) && !(bCompleted && pFile->HasProperty("CompletedTime")))
			continue;

		if(bRootOnly && pFile->IsSubFile())
			continue;

		// ToDo-Now: Filename wildcard selection

		Files.append(pFile);
	}



	if(Request.contains("Sort"))
	{
		SFileListSorter Sorter(Request["Sort"].toString());
		std::sort(Files.begin(), Files.end(), Sorter);
	}
	int Limit = Request["Limit"].toInt();
	if(Limit && Limit < Files.size())
		Files.erase(Files.begin() + Limit, Files.end());

	QSet<uint64> ExtendedList;
	if(Request.contains("ExtendedList"))
	{
		bAllExtended = false;
		foreach(const QVariant& vID, Request["ExtendedList"].toList())
			ExtendedList.insert(vID.toULongLong());
	}

	QVariantMap Response;
	SFileStateCache* pMapState = NULL;
	QMap<uint64, SCachedFile*> CacheMap;
	if(Request.contains("Token"))
	{
		uint64 Token = Request["Token"].toULongLong();
		Token = ((Token & 0x000F000000000000) == 0x0001000000000000) ? Token & 0x0000FFFFFFFFFFFF : 0; // check if tocken is valid and remove flag
		pMapState = (SFileStateCache*)m_StatusCache.value(Token);
		if(!pMapState)
		{
			pMapState = new SFileStateCache;

			do Token = (GetRand64() & 0x0000FFFFFFFFFFFF);
			while (Token && m_StatusCache.contains(Token));

			m_StatusCache.insert(Token, pMapState);
		}
		pMapState->LastUpdate = GetCurTick();
		CacheMap = pMapState->Map;
		Response["Token"] = Token | 0x0001000000000000; // file flag
	}

	QVariantList FileList;

	foreach(CFile* pFile, Files)
	{
		uint64 FileID = pFile->GetFileID();
		ASSERT(FileID);

		SCachedFile* pState = CacheMap.take(FileID);
		if(!pState && pMapState)
		{
			pState = new SCachedFile();
			ASSERT(!pMapState->Map.contains(FileID));
			pMapState->Map.insert(FileID, pState);
		}
		QVariantMap* pUpdate = NULL;

		UPDATE(FileName, pFile->GetFileName());
		UPDATE(FileDir, pFile->GetFileDir());

		uint64 uFileSize = pFile->GetFileSizeEx();
		UPDATE(FileSize, uFileSize);
		
		UPDATE_EX(FileType, GetFileType(pFile), FileTypeToStr);
		UPDATE_EX(FileState, GetFileState(pFile), FileStateToStr);
		UPDATE_EX(FileStatus, GetFileStatus(pFile), FileStatusToStr);
#ifndef NO_HOSTERS
		UPDATE(HosterStatus, pFile->GetProperty("HosterStatus").toString());
#endif
		UPDATE_EX(FileJobs, GetFileJobs(pFile), FileJobsToStr);
		

		int Progress = 0;
		if(pFile->IsComplete(true))
			Progress = 100;
		else if(uFileSize)
		{
			uint64 uSize = theCore->m_Hashing->GetProgress(pFile);
			if(uSize == -1)
			{
				if(pFile->IsAllocating())
					uSize = pFile->GetStatusStats(Part::NotAvailable);
				else
					uSize = pFile->GetStatusStats(Part::Available);
			}
			Progress = 100 * uSize / uFileSize;
			if(Progress > 100) Progress = 100;
		}
		UPDATE(Progress, Progress);

		int Transfers = pFile->GetStats()->GetTransferCount(eTypeUnknown);
		UPDATE(Transfers, Transfers);
		CFileStats::STemp Temp = pFile->GetStats()->GetTempCount(eTypeUnknown);
		UPDATE(ConnectedTransfers, Temp.Connected);
		UPDATE(CheckedTransfers, Temp.Checked);
		UPDATE(SeedTransfers, Temp.Complete);

		UPDATE(Availability, pFile->GetStats()->GetAvailStats());
		UPDATE(AuxAvailability, pFile->GetDetails()->GetAvailStats());


		UPDATE(Downloaded, pFile->DownloadedBytes());
		UPDATE(Uploaded, pFile->UploadedBytes());

		CBandwidthLimit* UpLimit = pFile->GetUpLimit();
		UPDATE(UpRate, UpLimit ? UpLimit->GetRate() : 0);
		UPDATE(Upload, UpLimit ? UpLimit->GetRate(CBandwidthCounter::ePayload) : 0);
		CBandwidthLimit* DownLimit = pFile->GetDownLimit();
		UPDATE(DownRate, DownLimit ? DownLimit->GetRate() : 0);
		UPDATE(Download, DownLimit ? DownLimit->GetRate(CBandwidthCounter::ePayload) : 0);

		UPDATE(QueuePos, pFile->GetQueuePos());

		if(bAllExtended || ExtendedList.contains(FileID))
		{
			QVariantMap File;
			File["FullPath"] = pFile->GetFilePath();

#ifndef __APPLE__
			if(m_NeoFS)
				File["VirtualPath"] = m_NeoFS->GetVirtualPath(pFile);
#endif

			File["Error"] = pFile->GetError();

			QStringList Hashes;
			foreach(EFileHashType eType, pFile->GetHashMap().uniqueKeys())
				Hashes.append(CFileHash::HashType2Str(eType));
			if(pFile->GetTorrents().count() > 0)
				Hashes.append(CFileHash::HashType2Str(HashTorrent));
#ifndef NO_HOSTERS
			if(pFile->GetArchives().count() > 0)
				Hashes.append(CFileHash::HashType2Str(HashArchive));
#endif
			File["Hashes"] = Hashes;
			if(CFileHashPtr pMasterHash = pFile->GetMasterHash())
				File["MasterHash"] = CFileHash::HashType2Str(pMasterHash->GetType()) + ":" + QString(pMasterHash->ToString());
			else
				File["MasterHash"] = "";

			File["ActiveTime"] = (uint64)pFile->GetActiveTime();

			File["ActiveUploads"] = pFile->GetActiveUploads();
			File["WaitingUploads"] = pFile->GetWaitingUploads();
			File["ActiveDownloads"] = pFile->GetActiveDownloads();
			File["WaitingDownloads"] = pFile->GetWaitingDownloads();

			File["AutoShare"] = pFile->IsAutoShare();
			File["Torrent"] = pFile->IsTorrent();
			File["Ed2kShare"] = pFile->IsEd2kShared();
			File["NeoShare"] = pFile->IsNeoShared();
#ifndef NO_HOSTERS
			File["HosterDl"] = pFile->IsHosterDl();
			File["HosterUl"] = pFile->IsHosterUl();
			File["ReUpload"] = pFile->GetProperty("ReUpload");
#endif

			File["Torrenting"] = pFile->GetProperty("Torrenting"); // this tells if the queue system enabled torrenting on this file

			File["Priority"] = pFile->GetProperty("Priority");
			File["Stream"] = pFile->GetProperty("Stream");
			File["Force"] = pFile->GetProperty("Force");

			File["Streamable"] = pFile->GetProperty("Streamable");
			File["Stream"] = pFile->GetProperty("Stream");

			File["KnownStatus"] = pFile->GetProperty("KnownStatus");

			int ShareRatio = pFile->GetProperty("ShareRatio").toInt();
			if(!ShareRatio)
				ShareRatio = theCore->Cfg()->GetInt("Content/ShareRatio");
			File["ShareRatio"] = ShareRatio;

			File["MaxUpload"] = pFile->GetProperty("Upload", 0);
			File["MaxDownload"] = pFile->GetProperty("Download", 0);

			for(QVariantMap::iterator J = File.begin(); J != File.end(); J++)
			{
				if(!pState || pState->File[J.key()] != J.value())
				{
					if(pState)
						pState->File[J.key()] = J.value();
					if(!pUpdate)
						pUpdate = new QVariantMap();
					(*pUpdate)[J.key()] = J.value();
				}
			}
		}

		if(pUpdate)
		{
			(*pUpdate)["ID"] = FileID;
			FileList.append(*pUpdate);
			delete pUpdate;
		}
	}

	if(!bSellection)
	{
		if(CSearchAgent* pSearchAgent = qobject_cast<CSearchAgent*>(theCore->m_SearchManager->GetSearch(Request["SearchID"].toULongLong())))
		{
			foreach(CCollection* pCollection, pSearchAgent->GetAllCollections())
			{
				uint64 FileID = pCollection->GetID();

				SCachedFile* pState = CacheMap.take(FileID);
				if(!pState && pMapState)
				{
					pState = new SCachedFile();
					ASSERT(!pMapState->Map.contains(FileID));
					pMapState->Map.insert(FileID, pState);
				}
				QVariantMap* pUpdate = NULL;

				UPDATE(FileName, pCollection->GetName());
				QStringList Path = pCollection->GetPath();
				Path.removeFirst();
				Path.removeLast();
				Path.append("");
				UPDATE(FileDir, Path.join("/"));

				UPDATE_EX(FileType, SCachedFile::eCollection, FileTypeToStr);

				if(pUpdate)
				{
					(*pUpdate)["ID"] = FileID;
					FileList.append(*pUpdate);
					delete pUpdate;
				}
			}
		}
	}

	if(!bSellection)
	{
		for(QMap<uint64, SCachedFile*>::iterator I = CacheMap.begin(); I != CacheMap.end(); I++)
		{
			QVariantMap Update;
			Update["ID"] = I.key();
			Update["FileState"] = "Deprecated";
			FileList.append(Update);

			if(pMapState)
				delete pMapState->Map.take(I.key());
		}
	}

	Response["Files"] = FileList;
	return Response;
}

/**
* GetProgress
*
* Request: 
*	{
*		"Mode":			Mode
*		"ID":			uint64;	FileID
*		"SubID":		uint64; TransferID
*		"Hoster":		string; hoster name
*		"Width":		int; Width
*		"Height":		int; Height
*	}
*
* Response:	
*	{
*		"Data":			ByteArray
*	}
*
*/
QVariantMap CCoreServer::GetProgress(QVariantMap Request)
{
	QBuffer Data;
	uint64 uTick = GetCurTick();

	if(Request.contains("PartClass"))
		CWebUI::GetProgress(&Data, Request["ID"].toULongLong(), Request["PartClass"].toString(), Request["HostName"].toString(), Request["UserName"].toString(),
						Request["Width"].toInt(), Request["Height"].toInt(), Request.contains("Depth") ? Request["Depth"].toInt() : 5);
	else if(Request["SubID"].toULongLong())
		CWebUI::GetProgress(&Data, Request["ID"].toULongLong(), Request["SubID"].toULongLong(), 
						Request["Width"].toInt(), Request["Height"].toInt(), Request.contains("Depth") ? Request["Depth"].toInt() : 5);
	else
		CWebUI::GetProgress(&Data, Request["Mode"].toString(), Request["ID"].toULongLong(), 
						Request["Width"].toInt(), Request["Height"].toInt(), Request.contains("Depth") ? Request["Depth"].toInt() : 5);

	uint64 uRenderTime = GetCurTick() - uTick;

	QVariantMap Response;
	Response["Data"] = Data.data();
	Response["RenderTime"] = uRenderTime;
	return Response;
}

/**
* GetFile get detail of one aprticular file
*
* Request: 
*	{
*		"ID":			uint64; FileID
*	}
*
* Response:	
*	{
*		"ID":			uint64; ID
*		"FileName":		String;	file name
*		"Status":		String;	file status
*		"Type":			String;	file Type
*
*		"FileSize":		uint64;	file size
*
*		"DownRate":		uint64;	downlaod rante in bytes per second
*		"UpRate":		uint64;	upload rante in bytes per second
*
*		"FileHash":	
*		{
*			"HashType": ByteArray Hash
*			,...
*		}
*
*		"Trackers":
*		[{
*			"Url":			String; tracker URL
*			"Status":		String;	tracker status	
*			"Tier":			Int; Groupe
*		},...]
*
*		"SubFiles":		[uint64; FileID, ...]
*
*		"Properties":	{...}
*	}
*
*/
QVariantMap CCoreServer::GetFile(QVariantMap Request)
{
	QVariantMap File;
	File["ID"] = Request["ID"];

	if(CFile* pFile = CFileList::GetFile(Request["ID"].toULongLong()))
	{
		File["FileName"] = pFile->GetFileName();
		File["FullPath"] = pFile->GetFilePath();

		File["FileType"] = FileTypeToStr(GetFileType(pFile));
		File["FileState"] = FileStateToStr(GetFileState(pFile));
		File["FileStatus"] = FileStatusToStr(GetFileStatus(pFile));
#ifndef NO_HOSTERS
		File["HosterStatus"] = pFile->GetProperty("HosterStatus").toString();
#endif
		File["FileJobs"] = FileJobsToStr(GetFileJobs(pFile));
		if(pFile->HasError())
			File["Error"] = pFile->GetError();

#ifndef __APPLE__
        if(m_NeoFS)
            File["VirtualPath"] = m_NeoFS->GetVirtualPath(pFile);
#endif

		uint64 uFileSize = pFile->GetFileSizeEx();
		File["FileSize"] = uFileSize;

		File["Availability"] = pFile->GetStats()->GetAvailStats();
		File["Transfers"] = pFile->GetStats()->GetTransferCount(eTypeUnknown);
		File["ConnectedTransfers"] = pFile->GetStats()->GetTempCount(eTypeUnknown).Connected;
		File["CheckedTransfers"] = pFile->GetStats()->GetTempCount(eTypeUnknown).Checked;
		File["SeedTransfers"] = pFile->GetStats()->GetTempCount(eTypeUnknown).Complete;

#ifndef NO_HOSTERS
		File["HosterList"] = pFile->GetStats()->GetHosterList();
#endif

        File["ActiveTime"] = (uint64)pFile->GetActiveTime();

		if(pFile->IsComplete(true))
			File["Progress"] = 100;
		else if(!uFileSize)
			File["Progress"] = 0;
		else 
		{
			uint64 uSize = theCore->m_Hashing->GetProgress(pFile);
			if(uSize == -1)
			{
				if(pFile->IsAllocating())
					uSize = pFile->GetStatusStats(Part::NotAvailable);
				else
					uSize = pFile->GetStatusStats(Part::Available);
			}
			int Progress = 100 * uSize / uFileSize;
			if(Progress > 100) Progress = 100;
			File["Progress"] = Progress;
		}

		File["Downloaded"] = pFile->DownloadedBytes();
		File["Uploaded"] = pFile->UploadedBytes();

		CBandwidthLimit* UpLimit = pFile->GetUpLimit();
		File["UpRate"] = UpLimit ? UpLimit->GetRate() : 0;
		File["Upload"] = UpLimit ? UpLimit->GetRate(CBandwidthCounter::ePayload) : 0;
		CBandwidthLimit* DownLimit = pFile->GetDownLimit();
		File["DownRate"] = DownLimit ? DownLimit->GetRate() : 0;
		File["Download"] = DownLimit ? DownLimit->GetRate(CBandwidthCounter::ePayload) : 0;

		File["ActiveUploads"] = pFile->GetActiveUploads();
		File["WaitingUploads"] = pFile->GetWaitingUploads();
		File["ActiveDownloads"] = pFile->GetActiveDownloads();
		File["WaitingDownloads"] = pFile->GetWaitingDownloads();

		QVariantList HashMap;

		if(pFile->IsMultiFile())
		{
			QVariantList SubFiles;
			foreach(uint64 FileID, pFile->GetSubFiles())
				SubFiles.append(FileID);
			File["SubFiles"] = SubFiles;
		}
		else
		{
			QVariantList ParentFiles;
			foreach(uint64 FileID, pFile->GetParentFiles())
			{
				ParentFiles.append(FileID);

				if(CFile* pParentFile = pFile->GetList()->GetFileByID(FileID))
				{
					if(CFileHashPtr pMasterHash = pParentFile->GetMasterHash())
					{
						QVariantMap Hash;
						Hash["Type"] = CFileHash::HashType2Str(pMasterHash->GetType());
						Hash["Value"] = QString(pMasterHash->ToString());
						Hash["State"] = "Parent";
						HashMap.append(Hash);
					}
				}
			}
			File["ParentFiles"] = ParentFiles;
		}

		File["Ratings"] = pFile->GetDetails()->Dump();

		CFileHashPtr pMasterHash = pFile->GetMasterHash();
		bool bHasMaster = false;
		foreach(CFileHashPtr pFileHash, pFile->GetAllHashes(true))
		{
			QVariantMap Hash;
			Hash["Type"] = CFileHash::HashType2Str(pFileHash->GetType());
			Hash["Value"] = QString(pFileHash->ToString());
			CFileHashEx* pFileHashEx = qobject_cast<CFileHashEx*>(pFileHash.data());
			if(pFileHash->IsValid() && ((pFileHashEx && pFileHashEx->CanHashParts()) || pFileHash->GetType() == HashArchive))
				Hash["State"] = "Full";
			else
				Hash["State"] = "Empty";

			if(pFileHash->Compare(pMasterHash.data())) 
				bHasMaster = true;

			HashMap.append(Hash);
		}

		if(pMasterHash)
		{
			/*if(!bHasMaster)
			{
				QVariantMap Hash;
				Hash["Type"] = CFileHash::HashType2Str(pMasterHash->GetType());
				Hash["Value"] = QString(pMasterHash->ToString());
				Hash["State"] = "Empty";
				HashMap.append(Hash);
			}*/

			if(!pFile->IsRemoved())
			{
				if(CHashInspector* pInspector = pFile->GetInspector())
				{
					foreach(CFileHashPtr pFileHash, pInspector->GetAuxHashes())
					{
						QVariantMap Hash;
						Hash["Type"] = CFileHash::HashType2Str(pFileHash->GetType());
						Hash["Value"] = QString(pFileHash->ToString());
						Hash["State"] = "Aux";
						HashMap.append(Hash);
					}

					foreach(CFileHashPtr pFileHash, pInspector->GetBlackedHashes())
					{
						QVariantMap Hash;
						Hash["Type"] = CFileHash::HashType2Str(pFileHash->GetType());
						Hash["Value"] = QString(pFileHash->ToString());
						Hash["State"] = "Bad";
						HashMap.append(Hash);
					}
				}
			}

			File["MasterHash"] = CFileHash::HashType2Str(pMasterHash->GetType()) + ":" + QString(pMasterHash->ToString());
		}
		File["HashMap"] = HashMap;

		QVariantList Trackers;
		if(pFile->IsTorrent())
		{
			foreach(CTorrent* pTorrent, pFile->GetTorrents())
			{
				if(!pTorrent)
					continue;

				CTorrentInfo* pTorrentInfo = pTorrent->GetInfo();
				foreach(int Tier, pTorrentInfo->GetTrackerList().uniqueKeys())
				{
					foreach(const QString& Url, pTorrentInfo->GetTrackerList().values(Tier))
					{
						QVariantMap Tracker;
						QString UrlEx = Url;
						if(Url.left(6).compare("udp://", Qt::CaseInsensitive) == 0)
						{
							if(UrlEx.right(1) != "/")	UrlEx+= "/";
							UrlEx += QString(pTorrent->GetInfoHash().toHex());
						}
						if(Url.left(7).compare("http://", Qt::CaseInsensitive) == 0 || Url.left(8).compare("https://", Qt::CaseInsensitive) == 0)
						{
							if(!UrlEx.contains("?"))	UrlEx+= "?";
							else						UrlEx+= "&";
							UrlEx += "infohash=" + QString(pTorrent->GetInfoHash().toHex());
						}
						Tracker["Url"] = UrlEx;
						Tracker["Type"] = "Tracker";
						uint64 Next = 0;
						Tracker["Status"] = theCore->m_TorrentManager->GetTrackerStatus(pTorrent, Url, &Next);
						Tracker["Next"] = Next;
						Tracker["Tier"] = Tier;
						Trackers.append(Tracker);
					}
				}

				QVariantMap DHT;
				DHT["Url"] = QString("dht://%1/%2").arg(QString(theCore->m_TorrentManager->GetNodeID().toHex())).arg(QString(pTorrent->GetInfoHash().toHex()));
				DHT["Type"] = "DHT";
				uint64 Next = 0;
				DHT["Status"] = theCore->m_TorrentManager->GetDHTStatus(pTorrent, &Next);
				DHT["Next"] = Next;
				Trackers.append(DHT);
			}
		}

		if(pFile->IsEd2kShared())
		{
			QVariantMap MuleKad;
			MuleKad["Url"] = QString("ed2k://|node,%1|/").arg(QString(theCore->m_MuleManager->GetKad()->GetKadID().toHex()));
			MuleKad["Type"] = "MuleKad";
			uint64 Next = 0;
			MuleKad["Status"] = theCore->m_MuleManager->GetKad()->GetStatus(pFile, &Next);
			MuleKad["Next"] = Next;
			Trackers.append(MuleKad);

			foreach(const QString& Url, pFile->GetProperty("Ed2kServers").toStringList())
			{
				QVariantMap Server;
				Server["Url"] = Url;
				Server["Type"] = "Server";
				uint64 Next = 0;
				Server["Status"] = theCore->m_MuleManager->GetServerList()->GetServerStatus(pFile, Url, &Next);
				Server["Next"] = Next;
				Server["Tier"] = 0;
				Trackers.append(Server);
			}

			foreach(CEd2kServer* pServer, theCore->m_MuleManager->GetServerList()->GetServers())
			{
				const QString& Url = pServer->GetUrl();
				
				QVariantMap Server;
				Server["Url"] = Url;
				Server["Type"] = "Server";
				uint64 Next = 0;
				Server["Status"] = theCore->m_MuleManager->GetServerList()->GetServerStatus(pFile, Url, &Next);
				Server["Next"] = Next;
				Server["Tier"] = -1;
				Trackers.append(Server);
			}
		}

		QVariantMap NeoKad;
		NeoKad["Url"] = QString("neo://%1/").arg(QString(theCore->m_NeoManager->GetKad()->GetNodeID().toHex()));
		NeoKad["Type"] = "NeoKad";
		uint64 Next = 0;
		NeoKad["Status"] = theCore->m_NeoManager->GetKad()->GetStatus(pFile, &Next);
		NeoKad["Next"] = Next;
		Trackers.append(NeoKad);

		File["Trackers"] = Trackers;

		if(Request.contains("Options"))
		{
			QVariantMap Options;
			foreach(const QString& Name, Request["Options"].toStringList())
				Options[Name] = pFile->GetProperty(Name);
			File["Options"] = Options;
		}
		else
		{
			QVariantMap Properties;
			foreach(const QString& Name, pFile->GetAllProperties())
				Properties[Name] = pFile->GetProperty(Name);

			if(Properties["Description"].toString().isEmpty())
				Properties["Description"] = pFile->GetDetails()->GetProperty(PROP_DESCRIPTION);
			if(Properties["CoverUrl"].toString().isEmpty())
				Properties["CoverUrl"] = pFile->GetDetails()->GetProperty(PROP_COVERURL);
			if(Properties["Rating"].toInt() == 0)
				Properties["Rating"] = pFile->GetDetails()->GetProperty(PROP_RATING);

			File["Properties"] = Properties;
		}
	}
	else if(CCollection* pCollection = CCollection::GetCollection(Request["ID"].toULongLong()))
	{
		File["Ratings"] = pCollection->GetDetails()->Dump();

		QVariantMap Properties;

		Properties["FileName"] = pCollection->GetName();

		Properties["Description"] = pCollection->GetDetails()->GetProperty(PROP_DESCRIPTION);
		Properties["CoverUrl"] = pCollection->GetDetails()->GetProperty(PROP_COVERURL);
		Properties["Rating"] = pCollection->GetDetails()->GetProperty(PROP_RATING);

		File["Properties"] = Properties;
	}
	else
		File["FileStatus"] = "File Not Found";

	return File;
}

/**
* SetFile set file properties
*
* Request: 
*	{
*		"ID":			uint64; FileID
*		"FileName":		String;	File Name
*
*		"Trackers":
*		[{
*			"Url":			String; tracker URL
*			"Tier":			Int; Groupe
*		},...]
*
*		"Properties":	{...}
*						"Stream"
*						"AltNames"
*						SourceUrl|Title|Artist|Album|Length|LangDS|Description|Cover
*						"Availability"/"MuleDifferentNames"/"MulePublishersKnown"/"MuleTrustValue"
*	}
*
* Response:
*	{
*		"ID":			uint64; FileID 
*		"Status":		String; Status
*	}
*/
QVariantMap CCoreServer::SetFile(QVariantMap Request)
{
	QVariantMap File;
	File["ID"] = Request["ID"];

	if(CFile* pFile = CFileList::GetFile(Request["ID"].toULongLong()))
	{
		if(Request.contains("FileName"))
			pFile->SetFileName(Request["FileName"].toString());

		if(Request.contains("MasterHash") && pFile->IsIncomplete())
		{
			StrPair TypeHash = Split2(Request["MasterHash"].toString(), ":");
			EFileHashType Type = CFileHash::Str2HashType(TypeHash.first);
			QByteArray HashValue = CFileHash::DecodeHash(Type, TypeHash.second.toLatin1());
			if(pFile->IsIncomplete())
			{
				if(CFileHashPtr pMasterHash = pFile->GetHashPtrEx(Type, HashValue))
				{
					CFileHashPtr OldMaster = pFile->GetMasterHash(); 
					ASSERT(OldMaster); // if file i incomplete there must be a master hash set

					pFile->SetMasterHash(pMasterHash);

					foreach(uint64 FileID, pFile->GetSubFiles())
					{
						if(CFile* pSubFile = CFileList::GetFile(FileID))
						{
							if(OldMaster->Compare(pSubFile->GetMasterHash().data()))
								pSubFile->SetMasterHash(pMasterHash);
							else
								LogLine(LOG_WARNING, tr("Sub File %1 has a custom master hash").arg(pSubFile->GetFileName()));
						}
					}
				}
				else // its a parent master
				{
					foreach(uint64 FileID, pFile->GetParentFiles())
					{
						if(CFile* pParentFile = pFile->GetList()->GetFileByID(FileID))
						{
							if(CFileHashPtr pMasterHash = pParentFile->GetHashPtrEx(Type, HashValue))
							{
								pFile->SetMasterHash(pMasterHash);
								break;
							}
						}
					}
				}
			}

			//if(pFile->IsAutoShare())
			//	pFile->SetProperty("AutoShare", 2);
		}

		if(Request.contains("Trackers"))
		{
			QStringList ServerList;
			QMap<QByteArray, QMultiMap<int,QString> > TrackerList;
			foreach(const QVariant& vTracker, Request["Trackers"].toList())
			{
				QVariantMap Tracker = vTracker.toMap();
				QString Url = Tracker["Url"].toString();
				if(Url.left(7).compare("ed2k://", Qt::CaseInsensitive)  == 0)
					ServerList.append(Url);
				else
				{
					QByteArray InfoHash;
					if(Url.left(6).compare("udp://", Qt::CaseInsensitive) == 0)
					{
						StrPair UrlHash = Split2(Url, "/", true);
						Url = UrlHash.first;
						InfoHash = QByteArray::fromHex(UrlHash.second.toLatin1());
					}
					else if(Url.left(7).compare("http://", Qt::CaseInsensitive) == 0 || Url.left(8).compare("https://", Qt::CaseInsensitive) == 0)
					{
						StrPair UrlHash = Split2(Url, "infohash=", true);
						Url = UrlHash.first.left(UrlHash.first.length() - 1);
						InfoHash = QByteArray::fromHex(UrlHash.second.toLatin1());
					}
					TrackerList[InfoHash].insert(Tracker["Tier"].toInt(),Url);
				}
			}

			foreach(const QByteArray& InfoHash, TrackerList.keys())
			{
				QMultiMap<int,QString> Trackers = TrackerList[InfoHash];
				if(CTorrent* pTorrent = InfoHash.isEmpty() ? pFile->GetTopTorrent() : pFile->GetTorrent(InfoHash))
				{
					CTorrentInfo* pTorrentInfo = pTorrent->GetInfo();
					pTorrentInfo->SetTrackerList(Trackers);
					if(!pFile->IsPending())
					{
						if(!pTorrentInfo->IsEmpty())
							pTorrent->SaveTorrentToFile();
					}
				}
			}

			pFile->SetProperty("Ed2kServers", ServerList);
		}

		if(Request.contains("Options"))
		{
			QVariantMap Options = Request["Options"].toMap();
			foreach(const QString& Name, Options.keys())
				pFile->SetProperty(Name, Options.value(Name));
		}
		else
		{
			QVariantMap Properties = Request["Properties"].toMap();
			foreach(const QString& Name, Properties.keys())
				pFile->SetProperty(Name, Properties.value(Name));
		}
	}

	return File;
}

/**
* AddFile add a new file
*
* Request: 
*	{
*		"Magnet":		String;	magnet link
*	or
*		"ed2k":			String;	ed2k link
*	or
*		"FileName":		String;	File Name (mandatory for all types except "Files")
*
*		"FileSize":		uint64;	file size
*		"FileHash":	
*		{
*			"HashType": ByteArray Hash
*			,...
*		}
*	or 
*		"Links":		[String; Link, ...]
*	or 
*		"SubFiles":		[uint64; FileID, ...] (optional)
*
*		"Properties":
*		{
*			"GrabName":		Bool
*			"Stream":		Bool
*			"Directory":	string
*			...
*		}
*	}
*
* Response:
*	{
*		"ID":			uint64; FileID 
*	or
*		"Files":		[uint64; FileID, ...] in case of Type "Files" a list of files found in the directory
*	}
*/
QVariantMap CCoreServer::AddFile(QVariantMap Request)
{
	CFile* pFile = NULL;
	bool bDirect = false;
	bool bResume = true;
	uint64 GrabberID = 0;
	if(Request.contains("ID"))
	{
		CFile* pFoundFile = CFileList::GetFile(Request["ID"].toULongLong());
		//if(CSearch* pSearch = qobject_cast<CSearch*>(pFoundFile->GetList()))
		
		pFile = new CFile();
		// Note: the search is not like the grabber it has its own files and thay stay in search, 
		//			thay also ar not allowed to be started due to possible torrent info hash conflicts in the manager
		//			so we make an exact copy and grab thise

		QVariantMap File = pFoundFile->Store();
		File["ID"] = 0; // ensure new unique file ID will be assigned
		pFile->Load(File);	
		pFile->SetFileDir("");
	}
	else if(Request.contains("Magnet") || Request.contains("ed2k"))
	{
		if(Request.contains("Magnet"))
			pFile = theCore->m_FileGrabber->AddMagnet(Request["Magnet"].toString());
		else
			pFile = theCore->m_FileGrabber->AddEd2k(Request["ed2k"].toString());
	}
#ifndef NO_HOSTERS
	else if(Request.contains("Links"))
	{
		pFile = new CFile();
		GrabberID = theCore->m_FileGrabber->GrabFile(pFile);
		pFile->AddEmpty(HashArchive, Request["FileName"].toString(), 0);

		QStringList Links;
		if(Request["Links"].canConvert(QVariant::List))
			Links = Request["Links"].toStringList();
		else
			Links = theCore->m_FileGrabber->SplitUris(Request["Links"].toString());

		foreach(const QString& Link, Links)
			theCore->m_WebManager->AddToFile(pFile, Link, eOther);
	}
#endif
	else if(Request.contains("Import"))
	{
		bResume = false;
		if(Request["Import"] == "Ed2kMule")
			pFile = theCore->m_MuleManager->ImportDownload(Request["FilePath"].toString(), Request["Move"].toBool());
	}
	else if(Request.contains("FileName"))
	{
		if(Request.contains("SubFiles"))
		{
			pFile = new CFile();
			theCore->m_FileManager->AddFile(pFile);
			bDirect = true;

			QList<uint64> SubFiles;
			foreach(const QVariant& FileID, Request["SubFiles"].toList())
				SubFiles.append(FileID.toULongLong());

			if(pFile->AddNewMultiFile(Request["FileName"].toString(), SubFiles))
				pFile->Resume();
			else
			{
				delete pFile;
				pFile = NULL;
			}
		}
		else if(uint64 uFileSize = Request["FileSize"].toULongLong())
		{
			pFile = new CFile();
			GrabberID = theCore->m_FileGrabber->GrabFile(pFile);

			QVariantMap Hashes = Request["FileHash"].toMap();
			foreach(const QString& Type, Hashes.keys())
			{
				if(CFileHashPtr pHash = CFileHashPtr(CFileHash::FromString(Hashes[Type].toByteArray(), CFileHash::Str2HashType(Type), uFileSize)))
					pFile->AddHash(pHash);
			}

			pFile->AddEmpty(HashUnknown, Request["FileName"].toString(), uFileSize, false);
		}
		else if(Request.contains("FileData"))
		{
			QString FileName = Request["FileName"].toString();
			if(Split2(FileName, ".", true).second.compare("Torrent", Qt::CaseInsensitive) == 0)
			{
				pFile = new CFile();
				GrabberID = theCore->m_FileGrabber->GrabFile(pFile);
				pFile->SetPending();
				if(!pFile->AddTorrentFromFile(Request["FileData"].toByteArray()))
				{
					LogLine(LOG_ERROR, tr("The torrent file %1 cannot not be parsed.").arg(FileName));

					pFile = NULL;
				}
			}
			else if(Split2(FileName, ".", true).second.compare("emulecollection", Qt::CaseInsensitive) == 0)
			{
				CMuleCollection Collection;
				if(Collection.LoadFromData(Request["FileData"].toByteArray(), FileName))
				{
					pFile = new CFile();
					GrabberID = theCore->m_FileGrabber->GrabFile(pFile);
					Collection.Populate(pFile);
				}
				else
					LogLine(LOG_ERROR, tr("The collection file %1 cannot not be parsed.").arg(FileName));
			}
		}
	}

	QVariantMap File;
	if(pFile)
	{
		CFile* pKnownFile = NULL;
		if(bDirect || theCore->m_FileManager->GrabbFile(pFile, false, &pKnownFile))
		{
			QVariantMap Properties = Request["Properties"].toMap();
			foreach(const QString& Name, Properties.keys())
				pFile->SetProperty(Name, Properties.value(Name));

			File["ID"] = pFile->GetFileID();
		}
		else
		{
			if(!GrabberID)
				delete pFile;
			ASSERT(pKnownFile);
			File["ID"] = pKnownFile->GetFileID();
		}
	}
	else
		File["ID"] = 0;

	// Note: this deletes a file if it was not taken
	if(GrabberID)
		theCore->m_FileGrabber->Clear(true, GrabberID);

	return File;
}

/**
* FileAction perform an action on a file
*
* Request: 
*	{
*		"ID":			uint64; FileID
*		"Action":		String; Action to perform	"Start", "Pause", "Stop", "Remove", "Delete",
*													"MakeTorrent", "ClearTorrent", "UploadTorrent"
*													"Grab", "UploadParts",  
*													"UploadArchive", "UploadSolid", "RemoveArchive"
*													"Check", "Cleanup", "Reset",
*		"Recursive":	bool
*	}
*
* Response:
*	{
*		"ID":			uint64; FileID 
*		"Result":		String; Action Result
*	}
*/
QVariantMap CCoreServer::FileAction(QVariantMap Request)
{
	QVariantMap File;
	File["ID"] = Request["ID"];

	if(CFile* pFile = CFileList::GetFile(Request["ID"].toULongLong()))
	{
		bool bResult = true;

		QString Action = Request["Action"].toString();
		if(Action == "Start" || Action == "Pause" || Action == "Stop") // possible recursive commands
		{
			QList<CFile*> FileList;
			FileList.append(pFile);
			if(Request["Recursive"].toBool())
			{
				foreach(uint64 FileID, pFile->GetSubFiles())
				{
					if(CFile* pSubFile = CFileList::GetFile(FileID))
						FileList.append(pSubFile);
				}
			}

			if(Action == "Start")
			{
				foreach(CFile* pCurFile, FileList)
					pCurFile->Start();
			}
			else if(Action == "Pause")
			{
				foreach(CFile* pCurFile, FileList)
					pCurFile->Pause();
			}
			else if(Action == "Stop")
			{
				foreach(CFile* pCurFile, FileList)
					pCurFile->Stop();
			}
		}
		else if(Action == "Remove" || Action == "Delete")
		{
			// Note: remove may remove some subfiles so we need a approche that test whats left
			QList<uint64> SubFiles;
			if(Request["Recursive"].toBool())
				SubFiles = pFile->GetSubFiles();

			pFile->Remove(Action == "Remove", true);

			foreach(uint64 FileID, SubFiles)
			{
				if(CFile* pSubFile = CFileList::GetFile(FileID))
					pSubFile->Remove(Action == "Remove", true);
			}
		}
		else if(Action == "Grab")
		{
			// Note: Grabb is intrinsicaly recursive
			if(pFile->GetList() == theCore->m_FileGrabber)
				bResult = theCore->m_FileManager->GrabbFile(pFile);
			else if(CSearch* pSearch = qobject_cast<CSearch*>(pFile->GetList()))
				bResult = (pSearch->GrabbFile(pFile) != 0);
			else
			{
				if(theCore->Cfg()->GetBool("Content/AddPaused"))
					pFile->Pause();
				else if(!pFile->IsStarted())
					pFile->Start();

				foreach(uint64 FileID, pFile->GetSubFiles())
				{
					if(CFile* pSubFile = CFileList::GetFile(FileID))
					{
						if(theCore->Cfg()->GetBool("Content/AddPaused"))
							pSubFile->Pause();
						else if(!pSubFile->IsStarted())
							pSubFile->Start();
					}
				}
			}
		}
		else if(Action == "Disable")
			pFile->SetHalted(true);
		else if(Action == "Enable")
			pFile->SetHalted(false);
		else if(Action == "MakeTorrent")
		{
			if(bResult = pFile->MakeTorrent(Request["PieceLength"].toULongLong(), Request["MerkleTorrent"].toBool(), Request["TorrentName"].toString(), Request["PrivateTorrent"].toBool()))
			{

				CTorrentInfo* pTorrentInfo = pFile->GetTorrent(EMPTY_INFO_HASH)->GetInfo();

				QMultiMap<int,QString> TrackerList;
				foreach(const QVariant& vTracker, Request["Trackers"].toList())
				{
					QVariantMap Tracker = vTracker.toMap();
					TrackerList.insert(Tracker["Tier"].toInt(),Tracker["Url"].toString());
				}
				pTorrentInfo->SetTrackerList(TrackerList);

				pTorrentInfo->SetProperty("CreationTime", QDateTime::currentDateTime());
				pTorrentInfo->SetProperty("Creator", Request["Creator"]);
				pTorrentInfo->SetProperty("Description", Request["Description"]);

				/*Request["DHTNodes"]
				Request["WebSeeds"]*/
			}
		}
		else if(Action == "SetShare")
		{
			pFile->SetProperty(Request["Network"].toString(), Request["Share"]);
			//if(pFile->IsAutoShare())
			//	pFile->SetProperty("AutoShare", 2);
		}
		else if(Action == "Announce")
		{
			if(pFile->IsNeoShared())
				theCore->m_NeoManager->GetKad()->FindSources(pFile);

			theCore->m_TorrentManager->AnnounceNow(pFile);

			if(pFile->IsEd2kShared())
			{
				theCore->m_MuleManager->GetKad()->FindSources(pFile);

				theCore->m_MuleManager->GetServerList()->FindSources(pFile);
			}
		}
		else if(Action == "Republish")
		{
			theCore->m_NeoManager->GetKad()->ResetPub(pFile);

			//theCore->m_TorrentManager->AnnounceNow(pFile);

			if(pFile->IsEd2kShared())
			{
				theCore->m_MuleManager->GetKad()->ResetPub(pFile);

				//theCore->m_MuleManager->GetServerList()->ResetPub(pFile);
			}
		}
		else if(Action == "AddHash")
		{
			StrPair TypeHash = Split2(Request["Hash"].toString(), ":");
			EFileHashType Type = CFileHash::Str2HashType(TypeHash.first);

			if(Type == HashNone)
				LogLine(LOG_WARNING, tr("Atempted to add hash of unknown type %2 to %1").arg(pFile->GetFileName(), Type));
			else if(CFileHashPtr pHash = CFileHashPtr(CFileHash::FromString(TypeHash.second.toLatin1(), Type, pFile->GetFileSize())))
				pFile->AddHash(pHash);
			else
				LogLine(LOG_WARNING, tr("Atempted to add hash invalid hash value %2 to %1").arg(pFile->GetFileName(), Type));
		}
		else if(Action == "RemoveHash" || Action == "SelectHash" || Action == "UnSelectHash" || Action == "BanHash" || Action == "UnBanHash")
		{
			StrPair TypeHash = Split2(Request["Hash"].toString(), ":");
			EFileHashType Type = CFileHash::Str2HashType(TypeHash.first);
			QByteArray HashValue = CFileHash::DecodeHash(Type, TypeHash.second.toLatin1());

			if(Action == "RemoveHash")
			{
				if(CFileHashPtr pHash = pFile->GetHashPtrEx(Type, HashValue))
					pFile->DelHash(pHash);
			}
			else if(pFile->IsIncomplete())
			{
				if(CHashInspector* pInspector = pFile->GetInspector())
				{
					if(Action == "SelectHash")
						pInspector->SelectHash(Type, HashValue);
					else if(Action == "UnSelectHash")
						pInspector->UnSelectHash(Type, HashValue);
					else if(Action == "BanHash")
						pInspector->BanHash(Type, HashValue);
					else if(Action == "UnBanHash")
						pInspector->UnBanHash(Type, HashValue);
				}
			}
		}
		else if(Action == "AddSource")
		{
			QString Url = Request["Url"].toString();
			if(CTransfer* pTransfer = CTransfer::FromUrl(Url, pFile))
				pFile->AddTransfer(pTransfer);
			else
				bResult = false;
		}
		else if(Action == "FindIndex")
			bResult = theCore->m_NeoManager->GetKad()->FindIndex(pFile);
		else if(Action == "FindAliases")
		{
			if(CFileHash* pFileHash = CKadPublisher::SelectTopHash(pFile))
			{
				QVariantMap Criteria;
				Criteria["Hash"] = pFileHash->GetHash();
				//Criteria["Type"] = CFileHash::HashType2Str(pFileHash->GetType());
				bResult = theCore->m_SearchManager->StartSearch(eNeoKad, "", Criteria) != 0;
			}
			else
				bResult = false;
		}
		else if(Action == "FindRating")
		{
			pFile->GetDetails()->Find();
		}
		else if(Action == "ClearRating")
		{
			pFile->GetDetails()->Clear();
		}
#ifndef NO_HOSTERS
		else if(Action.left(6) == "Upload")
		{
			QStringList Hosts = Request["Hosters"].toStringList();
			if(Hosts.isEmpty())
				Hosts = theCore->m_WebManager->GetUploadHosts();
			
			if(pFile->GetList() == theCore->m_FileManager)
			{
				if(Action.mid(6) == "Parts")
					bResult = theCore->m_UploadManager->OrderUpload(pFile, Hosts);
				else if(Action.mid(6) == "Archive")
				{
					if(Request.contains("ArchiveID"))
						bResult = theCore->m_ArchiveUploader->OrderEncUpload(pFile, Hosts, QByteArray::fromBase64(Request["ArchiveID"].toByteArray().replace("-","+").replace("_","/")));
					else
						bResult = theCore->m_ArchiveUploader->OrderEncUpload(pFile, Hosts);
				}
				else if(Action.mid(6) == "Solid")
					bResult = theCore->m_UploadManager->OrderSolUpload(pFile, Hosts);
			}
		}
		else if(Action == "ReUpload")
		{
			bool bArchiveID = Request.contains("ArchiveID");
			QByteArray ArchiveID = QByteArray::fromBase64(Request["ArchiveID"].toByteArray().replace("-","+").replace("_","/"));

			foreach(CTransfer* pTransfer, pFile->GetTransfers())
			{
				if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
				{
					CArchiveSet* pArchive = pHosterLink->GetArchive();
					if(bArchiveID && (pArchive ? ArchiveID != pArchive->GetID() : !ArchiveID.isEmpty()))
						continue;

					if(pHosterLink->GetUploadInfo() == CHosterLink::eManualUpload)
					{
						if(pHosterLink->GetError() == "TaskFailed")
						{
							theCore->m_UploadManager->OrderReUpload(pHosterLink);
							pHosterLink->SetDeprecated();
						}
					}
				}
			}
		}
		else if(Action == "CreateArchive")
		{
			bResult = theCore->m_ArchiveUploader->CreateArchive(pFile, Request["PartSize"].toULongLong());
		}
		else if(Action == "RemoveArchive")
		{
			if(Request.contains("ArchiveID"))
				CArchiveDownloader::PurgeTemp(pFile, QByteArray::fromBase64(Request["ArchiveID"].toByteArray().replace("-","+").replace("_","/")), true);
			else
				CArchiveDownloader::PurgeTemp(pFile, true);
		}
		else if(Action == "CheckLinks")
		{
			foreach(CTransfer* pTransfer, pFile->GetTransfers())
			{
				if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
					pHosterLink->Check();
			}
		}
		else if(Action == "CleanupLinks" || Action == "ResetLinks")
		{
			foreach(CTransfer* pTransfer, pFile->GetTransfers())
			{
				if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
				{
					if(pHosterLink->HasError())
					{
						if(Action == "CleanupLinks")
							pFile->RemoveTransfer(pHosterLink);
						else
							pHosterLink->ClearError();
					}
					break;
				}
			}
		}
		else if(Action == "InspectLinks")
			theCore->m_FileGrabber->GetLinkGrabber()->InspectLinks(pFile);
#endif
		else if(Action == "RehashFile")
		{
			if(pFile->IsIncomplete() && pFile->GetPartMap())
				pFile->Rehash();
			else
				pFile->CalculateHashes(true);
		}
#ifdef CRAWLER
		else if(Action == "ArchiveFile")
			bResult = theCore->m_FileArchiver->ArchiveFile(pFile);
#endif
		else if(Action == "Test")
		{
#ifdef _DEBUG
					
#endif
		}

		File["Result"] = bResult ? "ok" : "failed";
	}
	else
		File["Result"] = "failed";
	return File;
}

/**
* FileIO perform an io operation on a file
*
* Request: 
*	{
*		"ID":			uint64; FileID
*	}
*
* Response:
*	{
*		"ID":			uint64; FileID 
*		"Result":		String; Action Result
*	}
*/
QVariantMap CCoreServer::FileIO(QVariantMap Request)
{
	QVariantMap Response;
	Response["ID"] = Request["ID"];

	if(CFile* pFile = CFileList::GetFile(Request["ID"].toULongLong()))
	{
		if(QIODevice* pIODevice = theCore->m_IOManager->GetDevice(pFile->GetFileID()))
		{
			if(Request.contains("Offset"))
			{
				pIODevice->open(QIODevice::ReadOnly);
				pIODevice->seek(Request["Offset"].toLongLong());
				Response["Data"] = pIODevice->read(Request["ToGo"].toLongLong());
			}
			else
				Response["Size"] = pIODevice->size();
			delete pIODevice;
		}
		else
			Response["Error"] = "NotReadable";
	}
	else
		Response["Error"] = "NotFound";
	return Response;
}

/**
* GetTransfers get transfers of one aprticular file
*
* Request: 
*	{
*		"ID":			uint64; FileID
*		"Type":			string: "Uploads"/"Downloads"/"Active"
*	}
*
* Response:	
*	{
*		"ID":			uint64; ID
*
*		"Transfers":
*		[{
*			"Url":			String; Identifying URL
*			"Status":		String;	transfer status
*			"Type":			String;	transfer type
*
*			"DownRate":		uint64;	downlaod rante in bytes per second
*			"UpRate":		uint64;	upload rante in bytes per second
*
*			"FileName":		String;	file name
*			"FileSize":		uint64;	file size
*
*			"Downlaoded":	uint64;
*			"Uploaded":		uint64;
*		},...]
*
*	}
*
*/
QVariantMap CCoreServer::GetTransfers(QVariantMap Request)
{
	QVariantMap Response;

	QSet<CTransfer*> List;
	if(Request.contains("ID"))
	{
		Response["ID"] = Request["ID"];

		if(CFile* pFile = CFileList::GetFile(Request["ID"].toULongLong()))
			List = pFile->GetTransfers();
		else
			Response["Status"] = "File Not Found";
	}
	else
	{
		if(Request["Type"] == "Downloads" || Request["Type"] == "Active")
		{
			foreach(QPointer<CTransfer> pTransfer, theCore->m_DownloadManager->GetTransfers())
			{
				if(pTransfer)
					List.insert(pTransfer);
			}
		}
		if(Request["Type"] == "Uploads" || Request["Type"] == "Active")
		{
			foreach(QPointer<CTransfer> pTransfer, theCore->m_UploadManager->GetTransfers())
			{
				if(pTransfer && !List.contains(pTransfer))
					List.insert(pTransfer);
			}
		}
	}

	bool bAllExtended = true;
	bool bSellection = false;

	QSet<uint64> ExtendedList;
	if((bSellection = Request.contains("SelectedList")) || Request.contains("ExtendedList"))
	{
		bAllExtended = false;
		foreach(const QVariant& vID, bSellection ? Request["SelectedList"].toList() : Request["ExtendedList"].toList())
			ExtendedList.insert(vID.toULongLong());
	}


	STransferStateCache* pMapState = NULL;
	QMap<uint64, SCachedTransfer*> CacheMap;
	if(Request.contains("Token"))
	{
		uint64 Token = Request["Token"].toULongLong();
		Token = ((Token & 0x000F000000000000) == 0x0002000000000000) ? Token & 0x0000FFFFFFFFFFFF : 0; // check if tocken is valid and remove flag
		pMapState = (STransferStateCache*)m_StatusCache.value(Token);
		if(!pMapState)
		{
			pMapState = new STransferStateCache;

			do Token = (GetRand64() & 0x0000FFFFFFFFFFFF);
			while (Token && m_StatusCache.contains(Token));

			m_StatusCache.insert(Token, pMapState);
		}
		pMapState->LastUpdate = GetCurTick();
		CacheMap = pMapState->Map;
		Response["Token"] = Token | 0x0002000000000000; // transfer flag
	}

	QVariantList TransferList;
	foreach(CTransfer* pTransfer, List)
	{
		if(bSellection && !ExtendedList.contains((uint64)pTransfer))
			continue;

		CFile* pFile = pTransfer->GetFile();

		SCachedTransfer* pState = CacheMap.take((uint64)pTransfer);
		if(!pState && pMapState)
		{
			pState = new SCachedTransfer();
			ASSERT(!pMapState->Map.contains((uint64)pTransfer));
			pMapState->Map.insert((uint64)pTransfer, pState);
		}
		QVariantMap* pUpdate = NULL;

		CP2PTransfer* pP2PTransfer = qobject_cast<CP2PTransfer*>(pTransfer);
#ifndef NO_HOSTERS
		CHosterLink* pHosterLink = pP2PTransfer ? NULL : qobject_cast<CHosterLink*>(pTransfer);
#endif
		CMuleSource* pMuleSource = pP2PTransfer ? qobject_cast<CMuleSource*>(pP2PTransfer) : NULL;
		CTorrentPeer* pTorrentPeer = pP2PTransfer ? qobject_cast<CTorrentPeer*>(pP2PTransfer) : NULL;

		UPDATE(Url, pTransfer->GetDisplayUrl())
		UPDATE(Type, pTransfer->GetTypeStr())
		UPDATE_EX(TransferStatus, GetTransferStatus(pTransfer), TransferStatusToStr);
		UPDATE_EX(UploadState, GetUploadState(pTransfer), TransferStateToStr);
		UPDATE_EX(DownloadState, GetDownloadState(pTransfer), TransferStateToStr);
		UPDATE(Found, pTransfer->GetFoundByStr())
		
		UPDATE(Downloaded, pTransfer->DownloadedBytes())
		UPDATE(Uploaded, pTransfer->UploadedBytes())

		CBandwidthLimit* UpLimit = pTransfer->GetUpLimit();
		UPDATE(UpRate, UpLimit ? UpLimit->GetRate() : 0);
		UPDATE(Upload, UpLimit ? UpLimit->GetRate(CBandwidthCounter::ePayload) : 0);
		CBandwidthLimit* DownLimit = pTransfer->GetDownLimit();
		UPDATE(DownRate, DownLimit ? DownLimit->GetRate() : 0);
		UPDATE(Download, DownLimit ? DownLimit->GetRate(CBandwidthCounter::ePayload) : 0);

#ifndef NO_HOSTERS
		if(pHosterLink)
		{
#ifndef _DEBUG
			UPDATE(FileName, (pHosterLink->GetProtection() ? "File ..." : pHosterLink->GetFileName()));
#else
			UPDATE(FileName, pHosterLink->GetFileName());
#endif
			UPDATE(FileSize, pHosterLink->GetFileSize());

			UPDATE(Available, pHosterLink->GetFileSize());

			uint64 uFileSize = pFile->GetFileSize();
			if(uFileSize == 0)
				uFileSize = pFile->GetProperty("EstimatedSize").toULongLong();
			UPDATE(Progress, (uFileSize ? 100 * pHosterLink->GetFileSize() / uFileSize : 0));

			UPDATE(LastDownloaded, pTransfer->DownloadedBytes())
			UPDATE(LastUploaded, pTransfer->UploadedBytes())

			UPDATE(Software, pHosterLink->GetHoster());
		}
		else
#endif
			if(pP2PTransfer)
		{
			if(pMuleSource) {
				UPDATE(FileName, pMuleSource->GetFileName());
			} else if(pTorrentPeer) {
				UPDATE(FileName, pTorrentPeer->GetTorrent()->GetInfo()->GetTorrentName());
			} else {
				UPDATE(FileName, pFile->GetFileName());
			}
			//Transfer["FileSize"] = pTransfer->GetFile()->GetFileSize();

			UPDATE(Available, pP2PTransfer->GetAvailableSize())

			uint64 uFileSize = pFile->GetFileSize();
			UPDATE(Progress, (uFileSize ? 100 * pP2PTransfer->GetAvailableSize() / uFileSize : 0))

			UPDATE(Software, pP2PTransfer->GetSoftware());
		}

		if(bAllExtended || ExtendedList.contains((uint64)pTransfer))
		{
			QVariantMap Transfer;

			Transfer["Error"] = pTransfer->GetError();

#ifndef NO_HOSTERS
			if(pHosterLink)
			{
				if(CWebTask* pWebTask = pHosterLink->GetTask())
					Transfer["TransferInfo"] = pWebTask->GetStatusStr();
				else if(pHosterLink->HasError())
					Transfer["TransferInfo"] = pHosterLink->GetError();
				else
					Transfer["TransferInfo"] = "";
			}
			else
#endif
				if(pP2PTransfer)
			{
				if(pP2PTransfer->IsConnected())
					Transfer["TransferInfo"] = pP2PTransfer->GetConnectionStr();
				else
					Transfer["TransferInfo"] = "";

				if(pMuleSource)
				{
					if(CMuleClient* pClient = pMuleSource->GetClient())
					{
						if(!pClient->GetDownSource())
							Transfer["SourceInfo"] = "NA4F";
						else if(pClient->GetDownSource() != pMuleSource)
							Transfer["SourceInfo"] = "A4AF";
						else if(pTransfer->IsWaitingDownload())
							Transfer["SourceInfo"] = QString::number(pMuleSource->GetRemoteQueueRank());
						else
							Transfer["SourceInfo"] = "";

						if(pClient->ProtocolRevision() != 0)
						{
							switch(pClient->GetHordeState())
							{
								case CMuleClient::eNone:		Transfer["Horde"] = "None"; break;
								case CMuleClient::eRequested:	Transfer["Horde"] = "Requested"; break;
								case CMuleClient::eAccepted:	Transfer["Horde"] = "Accepted"; break;
								case CMuleClient::eRejected:	Transfer["Horde"] = "Rejected"; break;
							}
						}
						else
							Transfer["Horde"] = "";
					}
					else
						Transfer["SourceInfo"] = "???";
				}

				Transfer["PendingBytes"] = pP2PTransfer->PendingBytes();
				Transfer["PendingBlocks"] = pP2PTransfer->PendingBlocks();
				Transfer["LastUploaded"] = pP2PTransfer->LastUploadedBytes();
				Transfer["UploadDuration"] = pP2PTransfer->UploadDuration();

				if(pP2PTransfer->IsActiveUpload())
				{
					if(CBandwidthLimit* UpLimit = pTransfer->GetUpLimit())
					{
						Transfer["IsFocused"] = UpLimit->GetPriority() && (UpLimit->GetPriority() > BW_PRIO_NORMAL);
						if(UpLimit->GetPriority() == (BW_PRIO_NORMAL/2))
						{
							Transfer["IsBlocking"] = true;
							Transfer["IsTrickle"] = false;
						}
						else
						{
							Transfer["IsBlocking"] = false;
							Transfer["IsTrickle"] = UpLimit->GetPriority() && (UpLimit->GetPriority() < BW_PRIO_NORMAL);
						}
					
					}
					//Transfer["IsBlocking"] = pP2PTransfer->IsBlocking();
				}


				Transfer["ReservedBytes"] = pP2PTransfer->ReservedBytes();
				Transfer["RequestedBlocks"] = pP2PTransfer->RequestedBlocks();
				Transfer["LastDownloaded"] = pP2PTransfer->LastDownloadedBytes();
				Transfer["DownloadDuration"] = pP2PTransfer->DownloadDuration();
			}

			for(QVariantMap::iterator J = Transfer.begin(); J != Transfer.end(); J++)
			{
				if(!pState || pState->Transfer[J.key()] != J.value())
				{
					if(pState)
						pState->Transfer[J.key()] = J.value();
					if(!pUpdate)
						pUpdate = new QVariantMap();
					(*pUpdate)[J.key()] = J.value();
				}
			}
		}

		if(pUpdate)
		{
			(*pUpdate)["ID"] = (uint64)pTransfer;
			(*pUpdate)["FileID"] = pFile->GetFileID();
			TransferList.append(*pUpdate);
			delete pUpdate;
		}
	}

	if(!bSellection)
	{
		for(QMap<uint64, SCachedTransfer*>::iterator I = CacheMap.begin(); I != CacheMap.end(); I++)
		{
			QVariantMap Update;
			Update["ID"] = I.key();
			Update["TransferStatus"] = "Deprecated";
			TransferList.append(Update);

			if(pMapState)
				delete pMapState->Map.take(I.key());
		}
	}

	Response["Transfers"] = TransferList;
	return Response;
}

/**
* FileAction perform an action on a file
*
* Request: 
*	{
*		"ID":			uint64; FileID
*		"SubID":		uint64; TransferID
*		"Action":		String; Action to perform	"CancelTransfer", "ClearError"
*	}
*
* Response:
*	{
*		"ID":			uint64; FileID 
*		"Result":		String; Action Result
*	}
*/
QVariantMap CCoreServer::TransferAction(QVariantMap Request)
{
	QVariantMap Transfer;
	Transfer["ID"] = Request["ID"];
	Transfer["SubID"] = Request["SubID"];

	CFile* pFile = CFileList::GetFile(Request["ID"].toULongLong());
	CTransfer* pTransfer = pFile ? pFile->GetTransfer(Request["SubID"].toULongLong()) : NULL;
	if(pFile && pTransfer)
	{
		bool bResult = true;

		QString Action = Request["Action"].toString();
		if(Action == "RemoveTransfer")
			pFile->RemoveTransfer(pTransfer);
		else if (Action == "CancelTransfer")
		{
#ifndef NO_HOSTERS
			if (CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
				pHosterLink->StopTask(true);
			else 
#endif
				if (CP2PTransfer* pP2PTransfer = qobject_cast<CP2PTransfer*>(pTransfer))
				pP2PTransfer->Disconnect();
		}
		else if (Action == "StartDownload")
		{
			if (pTransfer->IsDownload() && !pTransfer->IsActiveDownload())
			{
#ifndef NO_HOSTERS
				if (CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
					pHosterLink->StartDownload();
				else
#endif
					if (CTorrentPeer* pTorrentPeer = qobject_cast<CTorrentPeer*>(pTransfer))
					pTorrentPeer->Connect();
				else if (CMuleSource* pMuleSource = qobject_cast<CMuleSource*>(pTransfer))
					pMuleSource->AskForDownload();
				else if (CNeoEntity* pNeoEntity = qobject_cast<CNeoEntity*>(pTransfer))
					pNeoEntity->AskForDownload();
			}
		}
		else if (Action == "StartUpload")
		{
			if (pTransfer->IsUpload() && !pTransfer->IsActiveUpload())
				pTransfer->StartUpload();
		}
		else if(Action == "ClearError")
			pTransfer->ClearError();
		else if(Action == "CheckLink")
		{
#ifndef NO_HOSTERS
			if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
				pHosterLink->Check();
#endif
		}
		else if(Action == "ReUpload")
		{
#ifndef NO_HOSTERS
			if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
			{
				if(pHosterLink->GetUploadInfo() == CHosterLink::eManualUpload)
				{
					theCore->m_UploadManager->OrderReUpload(pHosterLink);
					pHosterLink->SetDeprecated();
				}
			}
#endif
		}
		else 
			bResult = false;

		Transfer["Result"] = bResult ? "ok" : "failed";
	}
	else
		Transfer["Result"] = "failed";
	return Transfer;
}
	
/**
* GetClients get transfers of one aprticular file
*
* Request: 
*	{
*	}
*
* Response:	
*	{
*		"ID":			uint64; ID
*
*		"Transfers":
*		[{
*			"Url":			String; Identifying URL
*			"Status":		String;	transfer status
*			"Type":			String;	transfer type
*
*			"DownRate":		uint64;	downlaod rante in bytes per second
*			"UpRate":		uint64;	upload rante in bytes per second
*		},...]
*
*	}
*
*/
QVariantMap CCoreServer::GetClients(QVariantMap Request)
{
	QVariantMap Response;

	QSet<CP2PClient*> List;
	foreach(CMuleClient* pClient, theCore->m_MuleManager->GetClients())
		List.insert(pClient);
	foreach(CNeoClient* pClient, theCore->m_NeoManager->GetClients())
		List.insert(pClient);
	foreach(CTorrentClient* pClient, theCore->m_TorrentManager->GetClients())
		List.insert(pClient);

	bool bAllExtended = true;
	bool bSellection = false;

	QSet<uint64> ExtendedList;
	if((bSellection = Request.contains("SelectedList")) || Request.contains("ExtendedList"))
	{
		bAllExtended = false;
		foreach(const QVariant& vID, bSellection ? Request["SelectedList"].toList() : Request["ExtendedList"].toList())
			ExtendedList.insert(vID.toULongLong());
	}


	STransferStateCache* pMapState = NULL;
	QMap<uint64, SCachedTransfer*> CacheMap;
	if(Request.contains("Token"))
	{
		uint64 Token = Request["Token"].toULongLong();
		Token = ((Token & 0x000F000000000000) == 0x0002000000000000) ? Token & 0x0000FFFFFFFFFFFF : 0; // check if tocken is valid and remove flag
		pMapState = (STransferStateCache*)m_StatusCache.value(Token);
		if(!pMapState)
		{
			pMapState = new STransferStateCache;

			do Token = (GetRand64() & 0x0000FFFFFFFFFFFF);
			while (Token && m_StatusCache.contains(Token));

			m_StatusCache.insert(Token, pMapState);
		}
		pMapState->LastUpdate = GetCurTick();
		CacheMap = pMapState->Map;
		Response["Token"] = Token | 0x0002000000000000; // transfer flag
	}

	QVariantList TransferList;
	foreach(CP2PClient* pClient, List)
	{
		if(bSellection && !ExtendedList.contains((uint64)pClient))
			continue;

		SCachedTransfer* pState = CacheMap.take((uint64)pClient);
		if(!pState && pMapState)
		{
			pState = new SCachedTransfer();
			ASSERT(!pMapState->Map.contains((uint64)pClient));
			pMapState->Map.insert((uint64)pClient, pState);
		}
		QVariantMap* pUpdate = NULL;

		UPDATE(Url, pClient->GetUrl())
		UPDATE_EX(TransferStatus, SCachedTransfer::eUnchecked, TransferStatusToStr);
		UPDATE(Type, pClient->GetTypeStr())
		
		CBandwidthLimit* UpLimit = pClient->GetUpLimit();
		UPDATE(UpRate, UpLimit ? UpLimit->GetRate() : 0);
		UPDATE(Upload, UpLimit ? UpLimit->GetRate(CBandwidthCounter::ePayload) : 0);
		CBandwidthLimit* DownLimit = pClient->GetDownLimit();
		UPDATE(DownRate, DownLimit ? DownLimit->GetRate() : 0);
		UPDATE(Download, DownLimit ? DownLimit->GetRate(CBandwidthCounter::ePayload) : 0);

		UPDATE(Software, pClient->GetSoftware());
		
		CFile* pFile = pClient->GetFile();

		UPDATE(FileName, pFile ? pFile->GetFileName() : "");

		if(bAllExtended || ExtendedList.contains((uint64)pClient))
		{
			QVariantMap Transfer;

			Transfer["TransferInfo"] = pClient->GetConnectionStr();

			Transfer["Error"] = pClient->GetError();

			for(QVariantMap::iterator J = Transfer.begin(); J != Transfer.end(); J++)
			{
				if(!pState || pState->Transfer[J.key()] != J.value())
				{
					if(pState)
						pState->Transfer[J.key()] = J.value();
					if(!pUpdate)
						pUpdate = new QVariantMap();
					(*pUpdate)[J.key()] = J.value();
				}
			}
		}

		if(pUpdate)
		{
			(*pUpdate)["ID"] = (uint64)pClient;
			TransferList.append(*pUpdate);
			delete pUpdate;
		}
	}

	if(!bSellection)
	{
		for(QMap<uint64, SCachedTransfer*>::iterator I = CacheMap.begin(); I != CacheMap.end(); I++)
		{
			QVariantMap Update;
			Update["ID"] = I.key();
			Update["TransferStatus"] = "Deprecated";
			TransferList.append(Update);

			if(pMapState)
				delete pMapState->Map.take(I.key());
		}
	}

	Response["Transfers"] = TransferList;
	return Response;
}

#ifndef NO_HOSTERS

// Keys:
//=====================
// Any/All				- total stare ratio on all hosters
// Any/Me				- my share ratio on all hosters
// [Hoster]/All			- total share ratio on this hoster
// [Hoster]/Me			- my total ratio on this hoster
// [Hoster]/[User]		- my account share ratio on hoster for given account
// [Hoster]/Anonymouse	- my account less share ratio on hoster

QVariantMap CCoreServer::DumpHosters(QMap<QString, QMap<QString, double> > HostingInfo, QMap<QString, QMap<QString, QList<CHosterLink*> > > &AllLinks, CFile* pFile, CArchiveSet* pArchive)
{
	QVariantMap Groupe;
	if(pArchive)
	{
		Groupe["ID"] = pArchive->GetID().toBase64().replace("+","-").replace("/","_").replace("=","");
		
		Groupe["Status"] = pArchive->IsComplete() ? "Complete" : "Incomplete";
	}
	else 
	{
		Groupe["ID"] = "";

		Groupe["Status"] = pFile->IsComplete() ? "Complete" : "Incomplete";
	}
	Groupe["Share"] = HostingInfo["Any"]["All"];
	Groupe["MyShare"] = HostingInfo["Any"]["Me"];

	QVariantList Hosters;

	for(QMap<QString, QMap<QString, double> >::iterator I = HostingInfo.begin(); I != HostingInfo.end(); I++)
	{
		if(I.key() == "Any")
			continue;

		QVariantMap Hoster;
		Hoster["HostName"] = I.key();
		//Hoster["Status"] = 
		Hoster["Share"] = HostingInfo[I.key()]["All"];
		Hoster["MyShare"] = HostingInfo[I.key()]["Me"];

		QVariantList Users;
		for(QMap<QString, double>::iterator J = I.value().begin(); J != I.value().end(); J++)
		{
			if(J.key() == "Me")
				continue;

			QVariantMap Share;

			QList<CHosterLink*> LinkList;
			if(J.key() == "All")
			{
				LinkList = AllLinks[I.key()][""];
				if(LinkList.isEmpty())
					continue;

				Share["UserName"] = ""; // the Public
				//Hoster["Status"] = 
				Share["Share"] = J.value() - HostingInfo[I.key()]["Me"];
			}
			else
			{
				LinkList = AllLinks[I.key()][J.key()];

				Share["UserName"] = J.key();
				//Hoster["Status"] = 
				Share["Share"] = J.value();
			}

			QVariantList Links;
			foreach(CHosterLink* pHosterLink, LinkList)
			{
				QVariantMap Link;
				Link["ID"] = (uint64)((CTransfer*)pHosterLink);
				Link["Url"] = pHosterLink->GetDisplayUrl();
				Link["FileName"] = pHosterLink->GetFileName();
				Link["FileSize"] = pHosterLink->GetFileSize();
				
				Link["TransferStatus"] = TransferStatusToStr(GetTransferStatus(pHosterLink));

				Link["UploadState"] = TransferStateToStr(GetUploadState(pHosterLink));
				Link["DownloadState"] = TransferStateToStr(GetDownloadState(pHosterLink));

				if(CWebTask* pWebTask = pHosterLink->GetTask())
					Link["TransferInfo"] = pWebTask->GetStatusStr();
				else if(pHosterLink->HasError())
					Link["TransferInfo"] = pHosterLink->GetError();
				else
					Link["TransferInfo"] = "";

				Link["Share"] = pHosterLink->GetUploadInfoStr();

				Links.append(Link);
			}
			Share["Links"] = Links;

			Users.append(Share);
		}
		Hoster["Users"] = Users;

		Hosters.append(Hoster);
	}

	Groupe["Hosters"] = Hosters;

	return Groupe;
}

QVariantMap CCoreServer::GetHosting(QVariantMap Request)
{
	QVariantMap Response;
	Response["ID"] = Request["ID"];

	if(CFile* pFile = CFileList::GetFile(Request["ID"].toULongLong()))
	{
		QVariantList Groupes;

		QMap<QString, QMap<QString, QList<CHosterLink*> > > AllLinks;
		foreach(CTransfer* pTransfer, pFile->GetTransfers())
		{
			if(!pTransfer->GetPartMap())
				continue;
			if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
				AllLinks[pHosterLink->GetHoster()][pHosterLink->GetUploadAcc()].append(pHosterLink);
		}

		Groupes.append(DumpHosters(pFile->GetStats()->GetHostingInfo(), AllLinks, pFile));

		foreach(CArchiveSet* pArchive, pFile->GetArchives())
		{
			QMap<QString, QMap<QString, QList<CHosterLink*> > > ArchLinks;
			foreach(CHosterLink* pHosterLink, pArchive->GetAvailable())
			{
				if(pFile->GetTransfers().contains(pHosterLink))
					ArchLinks[pHosterLink->GetHoster()][pHosterLink->GetUploadAcc()].append(pHosterLink);
			}
			foreach(CHosterLink* pHosterLink, pArchive->GetPending())
			{
				if(pFile->GetTransfers().contains(pHosterLink))
					ArchLinks[pHosterLink->GetHoster()][pHosterLink->GetUploadAcc()].append(pHosterLink);
			}

			Groupes.append(DumpHosters(pArchive->GetHostingInfo(), ArchLinks, pFile, pArchive));
		}

		Response["Groups"] = Groupes;
	}
	else
		Response["Status"] = "File Not Found";

	return Response;
}
#endif

/**
* GetCore returns core settings
*
* Request: 
*	{
*	}
*
* Response:
*	{
*	}
*
*/
QVariantMap CCoreServer::GetCore(QVariantMap Request)
{
	QVariantMap Response;
	if(Request.contains("Options"))
	{
		QVariantMap Options;
		QMultiMap<QString, QString> Modules;
		foreach(const QString& Key, Request["Options"].toStringList())
		{
			StrPair Path = Split2(Key, "/");
			if(Path.first == "NeoLoader")
				Options[Key] = theCore->Cfg(false)->value(Path.second);
			else if(Path.first == "NeoCore")
				Options[Key] = theCore->Cfg()->value(Path.second);
			else
				Modules.insert(Path.first, Path.second);
		}
		foreach(const QString& Module, Modules.uniqueKeys())
		{
			QVariantMap SubRequest;
			SubRequest["Options"] = QStringList(Modules.values(Module));
			QVariantMap SubResponse = theCore->m_Interfaces->RemoteProcedureCall(Module, "GetSettings", SubRequest).toMap();
			QVariantMap SubOptions = SubResponse["Options"].toMap();
			foreach(const QString& Key, SubOptions.keys())
				Options[Module + "/" + Key] = SubOptions[Key];
		}
		Response["Options"] = Options;
	}
	else
	{
		QVariantMap Properties;
		foreach(const QString& Module, QString("NeoLoader|NeoCore|NeoKad|MuleKad|TorrentDHT").split("|"))
		{
			QVariantMap Root = Request[Module].toMap();

			CSettings* Cfg = NULL;
			if(Module == "NeoLoader")
				Cfg = theCore->Cfg(false);
			else if(Module == "NeoCore")
				Cfg = theCore->Cfg();
			else
			{
				QVariantMap Request;
				Request["Properties"] = Root;
				Properties[Module] = theCore->m_Interfaces->RemoteProcedureCall(Module, "GetSettings", Request).toMap()["Properties"];
				continue;
			}

			QVariantMap Settings;
			foreach(const QString& Groupe, Cfg->ListGroupes())
			{
				QVariantMap Branche;
				foreach(const QString& Key, Cfg->ListKeys(Groupe))
					Branche[Key] = Cfg->GetSetting(Groupe + "/" + Key);
				Settings[Groupe] = Branche;
			}
			Properties[Module] = Settings;
		}
		Response["Properties"] = Properties;
	}
	return Response;
}

/**
* SetCore sets core settings
*
* Request: 
*	{
*	}
*
* Response:
*	{
*		"Result":	String; "ok"
*	}
*/
QVariantMap CCoreServer::SetCore(QVariantMap Request)
{
	if(Request.contains("Options"))
	{
		QVariantMap Options = Request["Options"].toMap();
		QMultiMap<QString, QString> Modules;
		foreach(const QString& Key, Options.keys())
		{
			StrPair Path = Split2(Key, "/");
			if(Path.first == "NeoLoader")
				theCore->Cfg(false)->SetSetting(Path.second, Options[Key]);
			else if(Path.first == "NeoCore")
				theCore->Cfg()->SetSetting(Path.second, Options[Key]);
			else
				Modules.insert(Path.first, Path.second);
		}
		foreach(const QString& Module, Modules.uniqueKeys())
		{
			QVariantMap SubRequest;
			QVariantMap SubOptions;
			foreach(const QString& Key, SubOptions.keys())
				SubOptions[Key] = Options[Module + "/" + Key];
			SubRequest["Options"] = SubOptions;
			theCore->m_Interfaces->RemoteProcedureCall(Module, "SetSettings", SubRequest);
		}
	}
	else if(Request.contains("Properties"))
	{
		QVariantMap Properties = Request["Properties"].toMap();
		foreach(const QString& Module, Properties.uniqueKeys())
		{
			QVariantMap Root = Properties[Module].toMap();

			CSettings* Cfg = NULL;
			if(Module == "NeoLoader")
				Cfg = theCore->Cfg(false);
			else if(Module == "NeoCore")
				Cfg = theCore->Cfg();
			else
			{
				QVariantMap Request;
				Request["Properties"] = Root;
				theCore->m_Interfaces->RemoteProcedureCall(Module, "SetSettings", Request);
				continue;
			}

			foreach(const QString& Groupe, Root.uniqueKeys())
			{
				QVariantMap Branche = Root[Groupe].toMap();
				foreach(const QString& Key, Branche.uniqueKeys())
					Cfg->SetSetting(Groupe + "/" + Key, Branche[Key]);
			}
		}
	}
	else
	{
		if(Request.contains("UpLimit"))
			theCore->SetUpLimit(Request["UpLimit"].toInt());

		if(Request.contains("DownLimit"))
			theCore->SetDownLimit(Request["DownLimit"].toInt());
	}
	SMPL_RESPONSE("ok");
}

/**
* CoreAction
*
* Request: 
*	{
*		"Action":	String; Action to perform
*	}
*
* Response:
*	{
*		"Result":	bool;	test result
*	}
*/
QVariantMap CCoreServer::CoreAction(QVariantMap Request)
{
	bool bOK = true;
	QString Action = Request["Action"].toString();
	if(Action == "ScanShare")
		theCore->m_FileManager->ScanShare();
	else if(Action == "ClearCompleted")
	{
		foreach(CFile* pFile, theCore->m_FileManager->GetFiles())
			pFile->SetProperty("CompletedTime", QVariant());
	}
	else if(Action == "ClearHistory")
	{
		QList<uint64> CleanupList;
		foreach(CFile* pFile, theCore->m_FileManager->GetFiles())
		{
			if(pFile->IsRemoved())
			{
				if(pFile->IsMultiFile()) // remove multifiles first
					CleanupList.prepend(pFile->GetFileID());
				else
					CleanupList.append(pFile->GetFileID());
			}
		}
		foreach(uint64 FileID, CleanupList)
		{
			if(CFile* pFile = theCore->m_FileManager->GetFileByID(FileID))
				pFile->Remove(true, true);
		}	
	}
	/*else if(Action == "LoadIpFilter")
		theCore->m_PeerWatch->LoadIPFilter();*/
#ifdef CRAWLER
	else if(Action == "UploadSongs")
	{
		// false means there's already a song upload in progress (or no songs to upload have been found)
		bOK = theCore->m_CrawlerManager->InitPushing(Request["Dir"].toString(), Request["Start"].toBool());
	}
#endif
	else
		bOK = false;
	SMPL_RESPONSE(bOK ? "ok" : "failed");
}

/**
* ServerList
*
* Request: 
*	{
*	}
*
* Response:
*	{
*		"Hosters":
*		[{
*			"HostName":	String; Name
*			"Version":	int;	Version
*			"Logins":
*			[{
*				"UserName":	String; User Name
*				"Status":	String; Status
*			}
*			,...]
*			"Status":	string;	Status
*		}
*		,...]
*	}
*/
QVariantMap CCoreServer::ServerList(QVariantMap Request)
{
	QVariantList ServerList;
	foreach(CEd2kServer* pServer, theCore->m_MuleManager->GetServerList()->GetServers())
	{
		QVariantMap Server;

		Server["Url"] = pServer->GetUrl();
		Server["Name"] = pServer->GetName();
		Server["Version"] = pServer->GetVersion();
		Server["Description"] = pServer->GetDescription();
		
		Server["Status"] = pServer->GetStatusStr();
		Server["IsStatic"] = pServer->IsStatic();

		Server["UserCount"] = pServer->GetUserCount();
		Server["UserLimit"] = pServer->GetUserLimit();
		Server["LowIDCount"] = pServer->GetLowIDCount();

		Server["FileCount"] = pServer->GetFileCount();
		Server["HardLimit"] = pServer->GetHardLimit();
		Server["SoftLimit"] = pServer->GetSoftLimit();

		ServerList.append(Server);
	}
	RESPONSE("Servers", ServerList);
}

/**
* ServerAction
*
* Request: 
*	{
*		"Action":	String; Action to perform
*	}
*
* Response:
*	{
*		"Result":	bool;	test result
*	}
*/
QVariantMap CCoreServer::ServerAction(QVariantMap Request)
{
	bool bOK = true;
	QString Action = Request["Action"].toString();
	QString Url = Request["Url"].toString();
	if(Action == "RemoveServer")
	{
		if(CEd2kServer* pServer = theCore->m_MuleManager->GetServerList()->FindServer(Url))
			theCore->m_MuleManager->GetServerList()->RemoveServer(pServer);
	}
	else if(Action == "AddServer")
	{
		if(CEd2kServer* pServer = theCore->m_MuleManager->GetServerList()->FindServer(Url, true))
			pServer->SetStatic(true);
	}
	else if(Action == "SetStaticServer")
	{
		if(CEd2kServer* pServer = theCore->m_MuleManager->GetServerList()->FindServer(Url))
			pServer->SetStatic(Request["Static"].toBool());
	}
	else if(Action == "ConnectServer")
	{
		if(CEd2kServer* pServer = theCore->m_MuleManager->GetServerList()->FindServer(Url))
			QTimer::singleShot(0, pServer, SLOT(Connect()));
	}
	else if(Action == "DisconnectServer")
	{
		if(CEd2kServer* pServer = theCore->m_MuleManager->GetServerList()->FindServer(Url))
			QTimer::singleShot(0, pServer, SLOT(Disconnect()));
	}
	else
		bOK = false;
	SMPL_RESPONSE(bOK ? "ok" : "failed");
}

/**
* HosterList get hoster list
*
* Request: 
*	{
*	}
*
* Response:
*	{
*		"Hosters":
*		[{
*			"HostName":	String; Name
*			"Version":	int;	Version
*			"Logins":
*			[{
*				"UserName":	String; User Name
*				"Status":	String; Status
*			}
*			,...]
*			"Status":	string;	Status
*		}
*		,...]
*	}
*/
QVariantMap CCoreServer::ServiceList(QVariantMap Request)
{
	QVariantList ServiceList;
#ifndef NO_HOSTERS
	foreach(CWebScriptPtr pScript, theCore->m_WebManager->GetAllScripts())
	{
		if(pScript->IsLib())
			continue;
		QString HostName = pScript->GetScriptName();

		QVariantMap Hoster;
		Hoster["HostName"] = HostName;
		Hoster["Version"] = pScript->GetScriptVersion();
		Hoster["APIs"] = pScript->GetAPIs();
		Hoster["AnonUpload"] = pScript->HasAnonUpload();

		QVariantList Logins;
		foreach(StrPair UnPw, theCore->m_LoginManager->GetAllLogins(HostName))
		{
			QVariantMap Login;
			Login["UserName"] = UnPw.first;
			QString sLogin = UnPw.first + "@" + HostName.toLower();
			Login["Status"] = theCore->m_LoginManager->GetLoginStatus(sLogin);
			Login["Enabled"] = !theCore->m_LoginManager->GetOption(sLogin + "/Disabled").toBool();
			Login["Free"] = theCore->m_LoginManager->IsFree(sLogin);
			Logins.append(Login);
		}
		if(!Logins.isEmpty())
			Hoster["Logins"] = Logins;

		Hoster["Status"] = theCore->m_ConnectorManager->IsServerFailed(HostName) ? "Failed" : "Good";

		ServiceList.append(Hoster);
	}
#endif
	RESPONSE("Services", ServiceList);
}

/**
* GetHoster
*
* Request: 
*	{
*		
*	}
*
* Response:
*	{
*		
*	}
*/
QVariantMap CCoreServer::GetService(QVariantMap Request)
{
	QVariantMap Response;
#ifndef NO_HOSTERS
	QString HostName = Request["HostName"].toString();
	if(Request.contains("UserName"))
	{
		QString UserName = Request["UserName"].toString();
		QString sLogin = UserName + "@" + HostName.toLower();

		Response["Password"] = theCore->m_LoginManager->GetPassword(sLogin);
		Response["Enabled"] = !theCore->m_LoginManager->GetOption(sLogin + "/Disabled").toBool();
		Response["Free"] = theCore->m_LoginManager->IsFree(sLogin);
		Response["Upload"] = theCore->m_LoginManager->GetOption(sLogin + "/Upload");
		Response["Status"] = theCore->m_LoginManager->GetLoginStatus(sLogin);

		QVariantMap Options;
		foreach(const QString& Key, theCore->m_LoginManager->GetOptions(sLogin + "/"))
			Options[Key] = theCore->m_LoginManager->GetOption(sLogin + "/" + Key);
		Response["Properties"] = Options;
	}
	else if(CWebScriptPtr pScript = theCore->m_WebManager->GetScriptByName(HostName))
	{
		Response["AnonUpload"] = pScript->HasAnonUpload();
		Response["Upload"] = theCore->m_WebManager->GetOption(HostName + "/Upload");
		Response["Proxy"] = theCore->m_WebManager->GetOption(HostName + "/Proxy");

		QVariantMap Options;
		foreach(const QString& Key, theCore->m_WebManager->GetOptions(HostName + "/"))
			Options[Key] = theCore->m_WebManager->GetOption(HostName +  "/" + Key);
		Response["Properties"] = Options;

		if(Request["Logins"].toBool())
		{
			QVariantList Logins;
			foreach(StrPair UnPw, theCore->m_LoginManager->GetAllLogins(HostName))
			{
				QVariantMap Login;
				Login["UserName"] = UnPw.first;
				QString sLogin = UnPw.first + "@" + HostName.toLower();
				Login["Status"] = theCore->m_LoginManager->GetLoginStatus(sLogin);
				Login["Enabled"] = !theCore->m_LoginManager->GetOption(sLogin + "/Disabled").toBool();
				Login["Free"] = theCore->m_LoginManager->IsFree(sLogin);
				Logins.append(Login);
			}
			Response["Logins"] = Logins;
		}
	}
#endif
	return Response;
}

/**
* SetHoster
*
* Request: 
*	{
*		
*	}
*
* Response:
*	{
*		
*	}
*/
QVariantMap CCoreServer::SetService(QVariantMap Request)
{
#ifndef NO_HOSTERS
	QString HostName = Request["HostName"].toString();
	if(Request.contains("UserName"))
	{
		QString UserName = Request["UserName"].toString();
		if(UserName.isEmpty())
		{
			SMPL_RESPONSE("Error");
		}

		QString sLogin = UserName + "@" + HostName.toLower();

		if(Request.contains("Password"))
			theCore->m_LoginManager->SetLogin(sLogin, Request["Password"].toString(), Request["Free"].toBool());

		theCore->m_LoginManager->SetOption(sLogin + "/Disabled", !Request["Enabled"].toBool());
		theCore->m_LoginManager->SetOption(sLogin + "/Upload", Request["Upload"]);

		QVariantMap Options = Request["Properties"].toMap();
		foreach(const QString& Key, Options.uniqueKeys())
			theCore->m_LoginManager->SetOption(sLogin + "/" + Key, Options[Key]);
	}
	else
	{
		if(Request.contains("Upload"))
			theCore->m_WebManager->SetOption(HostName +  "/Upload", Request["Upload"]);

		if(Request.contains("Proxy"))
			theCore->m_WebManager->SetOption(HostName +  "/Proxy", Request["Proxy"]);

		QVariantMap Options = Request["Properties"].toMap();
		foreach(const QString& Key, Options.uniqueKeys())
			theCore->m_WebManager->SetOption(HostName + "/" + Key, Options[Key]);
	}
#endif
	SMPL_RESPONSE("ok");
}

/**
* HosterAction
*
* Request: 
*	{
*		"Action":	String; Action to perform
*	}
*
* Response:
*	{
*		"Result":	bool;	test result
*	}
*/
QVariantMap CCoreServer::ServiceAction(QVariantMap Request)
{
	QVariantMap Response;
#ifndef NO_HOSTERS
	bool bOK = false;
	QString HostName = Request["HostName"].toString();
	QString Action = Request["Action"].toString();
	if(Action == "LoadIcon")
	{
		if(CWebScriptPtr pScript = theCore->m_WebManager->GetScriptByName(HostName))
		{
#ifdef _DEBUG
			if(pScript->GetIcon().isEmpty())
			{
				QFile IconFile("../../NeoGUI/war/images/flags/hosters/" + HostName + ".png");
				if(IconFile.open(QFile::ReadOnly))
					Response["Icon"] = IconFile.readAll();
			}
			else
#endif
				Response["Icon"] = pScript->GetIcon();
			bOK = true;
		}
	}
	else if(Action == "SetAccount")
	{
		QString sLogin = Request["UserName"].toString() + "@" + HostName.toLower();
		theCore->m_LoginManager->SetLogin(sLogin, Request["Password"].toString());
	}
	else if(Action == "ClearAccount")
	{
		QString sLogin = Request["UserName"].toString() + "@" + HostName.toLower();
		theCore->m_LoginManager->ClearLogin(sLogin);
	}
	else if(Action == "CheckAccount")
	{
		QString sLogin = Request["UserName"].toString() + "@" + HostName.toLower();
		theCore->m_LoginManager->Checkout(sLogin);
	}
	Response["Result"] = bOK ? "ok" : "failed";
#endif
	return Response;
}

/**
* WebTaskList hets all running and or queued http tasks
*
* Request: 
*	{
*		"GrabberID":	uint64; optional FileGrabber ID to get only tasks for the grabing operation
*	}
*
* Response:
*	{
*		"Tasks":
*		[{
*			"ID":		uint64; ID
*			"Url":		String; Task Url
*			"Status":	String; Task Status;
*		}
*		,...]
*	}
*/
QVariantMap CCoreServer::WebTaskList(QVariantMap Request)
{
	QVariantMap Response;
#ifndef NO_HOSTERS
	QList<CWebTask*> TaskList = Request.contains("GrabberID") 
		? theCore->m_FileGrabber->GetLinkGrabber()->GetTasks(Request["GrabberID"].toULongLong()) 
		: theCore->m_WebManager->GetAllTasks();

	QVariantList Tasks;
	foreach(CWebTask* pTask, TaskList)
	{
		QVariantMap Task;
		Task["ID"] = (uint64)pTask;
		Task["Url"] = pTask->GetUrl();
		Task["Status"] = pTask->GetStatusStr();
		Task["Entry"] = pTask->GetEntryPoint();
		Tasks.append(Task);
	}
	Response["Tasks"] = Tasks;
#endif
	return Response;
}

/**
* GrabLinks
*
* Request: 
*	{
*		"Links":	[String; Link, ...] / "Links":	String;	Links
*		"FileName":	ByteArray; 
*		"FileData":	ByteArray; 
*	}
*
* Response:
*	{
*		"ID":		uint64; unique Grabber ID
*	}
*/
QVariantMap CCoreServer::GrabLinks(QVariantMap Request)
{
	uint64 GrabberID = 0;
	if(Request.contains("Links"))
	{
		QStringList Links;
		if(Request["Links"].canConvert(QVariant::List))
			Links = Request["Links"].toStringList();
		else
			Links = theCore->m_FileGrabber->SplitUris(Request["Links"].toString());
		if(Request["Direct"].toBool())
		{
			QMultiMap<QString, QString> LinkList;
			foreach(QString sUri, Links)
			{	
				int Pos = sUri.lastIndexOf("magnet:?");
				sUri = sUri.mid(Pos > 0 ? Pos : 0);
				QString Scheme = QUrl(sUri).scheme();
				if(Scheme.indexOf(QRegExp("(http|https|ftp)"), Qt::CaseInsensitive) == 0)
					LinkList.insert("http", sUri);
				else if(sUri.left(7).compare("ed2k://", Qt::CaseInsensitive) == 0) // ed2k link
					LinkList.insert("ed2k", sUri);
				else if(Scheme.indexOf("magnet", Qt::CaseInsensitive) == 0) // magnet link
					LinkList.insert("Magnet", sUri);
				else if(Scheme.indexOf(QRegExp("(jd|jdlist|dlc)"), Qt::CaseInsensitive) == 0) 
					LinkList.insert("JD", sUri);
			}

			foreach(const QString& Key, LinkList.uniqueKeys())
			{
				if(Key == "http")
				{
					QVariantMap Aux;
					Aux["Links"] = QStringList(LinkList.values("http"));
					QVariantMap Props;
					Props["GrabName"] = true;
					Aux["Properties"] = Props;
					AddFile(Aux);
				}
				else if(Key == "JD")
					theCore->m_FileGrabber->GrabUris(LinkList.values("JD"));
				else
				{
					foreach(const QString& Link, LinkList.values(Key))
					{
						QVariantMap Aux;
						Aux[Key] = Link;
						AddFile(Aux);
					}
				}
			}
		}
		else
			GrabberID = theCore->m_FileGrabber->GrabUris(Links);
	}
	else if(Request.contains("FileName"))
	{
		QString FileName = Request["FileName"].toString();
		if(Split2(FileName, ".", true).second.compare("Torrent", Qt::CaseInsensitive) == 0)
			GrabberID = theCore->m_TorrentManager->GrabTorrent(Request["FileData"].toByteArray(), FileName);
		else if(Split2(FileName, ".", true).second.compare("emulecollection", Qt::CaseInsensitive) == 0)
			GrabberID = theCore->m_MuleManager->GrabCollection(Request["FileData"].toByteArray(), FileName);
#ifndef NO_HOSTERS
		else
			GrabberID = theCore->m_FileGrabber->GetContainerDecoder()->GrabbContainer(Request["FileData"].toByteArray(), FileName);
#endif
	}
	RESPONSE("ID",GrabberID);
}

/**
* MakeLink returns neo magnet links for a given file
*
* Request: 
*	{
*		"ID":		uint64; FileID, ...
*		"Links":	String; "None"/"Torrent"/"Archive"
*		"Encoding":	String; "Plaintext"/"Compressed"/"Encrypted"
*	}
*
* Response:
*	{
*		"Link":		String; Link
*	}
*/
QVariantMap CCoreServer::MakeLink(QVariantMap Request)
{
	CFileGrabber::ELinks Links = CFileGrabber::eNone;
	if(Request.contains("Links"))
	{
		QString sHosts = Request["Links"].toString();
		if(sHosts == "None")
			Links = CFileGrabber::eNone;
		else if(sHosts == "Archive")
			Links = CFileGrabber::eArchive;
	}

	CFileGrabber::EEncoding Encoding = CFileGrabber::ePlaintext;
	if(Request.contains("Encoding"))
	{
		QString sEncoding = Request["Encoding"].toString();
		if(sEncoding == "Plaintext")
			Encoding = CFileGrabber::ePlaintext;
		else if(sEncoding == "Compressed")
			Encoding = CFileGrabber::eCompressed;
		else if(sEncoding == "Encrypted")
			Encoding = CFileGrabber::eEncrypted;
		else if(sEncoding == "ed2k")
			Encoding = CFileGrabber::eed2k;
		else if(sEncoding == "Magnet")
			Encoding = CFileGrabber::eMagnet;
		else if(sEncoding == "Torrent")
			Encoding = CFileGrabber::eTorrent;
		else if(sEncoding == "eMuleCollection")
			Encoding = CFileGrabber::eMuleCollection;
	}

	QVariantMap Response;
	if(CFile* pFile = CFileList::GetFile(Request["ID"].toULongLong()))
	{
		if(Encoding == CFileGrabber::eTorrent)
		{
			if(CTorrent* pTorrent = pFile->GetTopTorrent())
			{
				Response["FileName"] = pFile->GetFileName() + ".torrent";
				Response["FileData"] = pTorrent->GetInfo()->SaveTorrentFile();
			}
			else
				Response["Error"] = "NoTorrent";
		}
		else if(Encoding == CFileGrabber::eMuleCollection)
		{
			CMuleCollection Collection;
			if(Collection.Import(pFile))
			{
				Response["FileName"] = pFile->GetFileName() + ".emulecollection";
				Response["FileData"] = Collection.SaveToData();
			}
			else
				Response["Error"] = "NoCollection";
		}
		else
		{
			QString sUri = theCore->m_FileGrabber->GetUri(pFile, Links, Encoding);
			if(sUri.isEmpty())
				Response["Error"] = "NoHash";
			else
			{
				Response["Torrents"] = pFile->GetTorrents().count();
				Response["Link"] = sUri;
			}
		}
	}
	else
		Response["Error"] = "FileNotFound";
	return Response;
}

/**
* GrabberList returns a list of running grabber tasks
*
* Request: 
*	{
*	}
*
* Response:	
*	{
*		"Tasks":
*		[{
*			"ID":			uint64; Grab ID
*			"Uris":			StringList; Scheduled Uris
*
*			"TasksPending":	int; 
*			"TasksFailed":	int;
*
*			"IndexCount":	int;
*			"FileCount":	int;
*		}
*		,...]
*	}
*
*/
QVariantMap CCoreServer::GrabberList(QVariantMap Request)
{
	//uint64 ID = Request["ID"].toULongLong();

	QVariantList Tasks;
#ifndef NO_HOSTERS
	foreach(uint64 GrabberID, theCore->m_FileGrabber->GetTaskIDs())
	{
		QVariantMap Task;
		Task["ID"] = GrabberID;
		Task["Uris"] = theCore->m_FileGrabber->GetTaskUris(GrabberID);

		if(int Count = theCore->m_FileGrabber->GetLinkGrabber()->TasksPending(GrabberID))
			Task["TasksPending"] = Count;
		if(int Count = theCore->m_FileGrabber->GetLinkGrabber()->TasksPending(GrabberID, true))
			Task["TasksFailed"] = Count;

		if(int Count = theCore->m_FileGrabber->CountFiles(GrabberID))
			Task["FileCount"] = Count;

		Tasks.append(Task);
	}
#endif
	RESPONSE("Tasks",Tasks);
}

/**
* GrabberAction perform a action with the grabber
*
* Request: 
*	{
*		"ID":			uint64; optional Grabber Task ID to get one aprticular task with details
*		"Action":		String; "Remove", "Cleanup" cleanup up grabber
*	}
*
* Response:	
*	{
*		"Result":
*	}
*
*/
QVariantMap CCoreServer::GrabberAction(QVariantMap Request)
{
	uint64 GrabberID = Request["ID"].toULongLong();

	if(Request["Action"] == "Remove")
		theCore->m_FileGrabber->Clear(true, GrabberID);
	else if(Request["Action"] == "Cleanup")
		theCore->m_FileGrabber->Clear(false, GrabberID);
	SMPL_RESPONSE("ok");
}

#ifdef CRAWLER
/**
* CrawlerList returns a list of crawling sites
*
* Request: 
*	{
*	}
*
* Response:	
*	{
*	}
*
*/
QVariantMap CCoreServer::CrawlerList(QVariantMap Request)
{
	QMap<QString, QVariantMap> Browse;
	foreach(const QVariant& vBrows, Request["Browse"].toList())
	{
		QVariantMap Brows = vBrows.toMap();
		Browse.insert(Brows["SiteName"].toString(), Brows);
	}

	QVariantList Sites;
	foreach(CCrawlingSite* pSite, theCore->m_CrawlingManager->GetSites())
	{
		QVariantMap Site;
		Site["SiteName"] = pSite->GetFileName();
		Site["SiteEntry"] = pSite->GetProperty("SiteEntry");

		Site["CrawledEntries"] = pSite->GetCrawledEntries();
		Site["TotalEntries"] = pSite->GetTotalEntries();
		Site["CrawledSites"] = pSite->GetCrawledSites();
		Site["TotalSites"] = pSite->GetTotalSites();
		Site["ActiveTasks"] = pSite->GetActiveTasks();
		Site["SecForTasks"] = pSite->GetSecForTasks();

		Site["IndexedFiles"] = pSite->GetBlaster()->GetFileCount();
		Site["IndexedKeywords"] = pSite->GetBlaster()->GetKeywordCount();
		Site["ProcessedEntries"] = pSite->GetBlaster()->GetProcessedEntries();

		Site["ActiveKeywords"] = pSite->GetBlaster()->GetKeywords()->GetPublishments();
		Site["WaitingKeywords"] = pSite->GetBlaster()->GetKeywords()->GetTotalTasks();
		Site["FinishedKeywords"] = pSite->GetBlaster()->GetKeywords()->GetFinishedTasks();

		Site["ActiveRatings"] = pSite->GetBlaster()->GetRatings()->GetPublishments();
		Site["WaitingRatings"] = pSite->GetBlaster()->GetRatings()->GetTotalTasks();
		Site["FinishedRatings"] = pSite->GetBlaster()->GetRatings()->GetFinishedTasks();

		Site["ActiveLinks"] = pSite->GetBlaster()->GetLinks()->GetPublishments();
		Site["WaitingLinks"] = pSite->GetBlaster()->GetLinks()->GetTotalTasks();
		Site["FinishedLinks"] = pSite->GetBlaster()->GetLinks()->GetFinishedTasks();

		Site["LastIndexTime"] = pSite->GetProperty("LastIndexTime");
		Site["LastCrawledCount"] = pSite->GetProperty("LastCrawledCount");

		if(Browse.contains(pSite->GetFileName()))
		{
			QVariantMap Brows = Browse[pSite->GetFileName()];

			int TotalAvailable = 0;
			Site["Entries"] = pSite->GetEntries(Brows["Offset"].toInt(), Brows["MaxCount"].toInt(), Brows["Filter"].toString(), Brows["Sort"].toString(), &TotalAvailable);
			Site["FilteredEntries"] = TotalAvailable;
		}

		
		Sites.append(Site);
	}
	RESPONSE("Sites",Sites);
}

/**
* CrawlerAction perform a action with the crawler
*
* Request: 
*	{
*	}
*
* Response:	
*	{
*	}
*
*/
QVariantMap CCoreServer::CrawlerAction(QVariantMap Request)
{
	bool bRet = true;
	if(Request["Action"] == "AddSite")
		bRet = theCore->m_CrawlingManager->AddSite(Request["SiteName"].toString(), Request["SiteEntry"].toString());
	else if(Request["Action"] == "RemoveSite")
		theCore->m_CrawlingManager->RemoveSite(Request["SiteName"].toString());
	else
	{
		CCrawlingSite* pSite = theCore->m_CrawlingManager->GetSite(Request["SiteName"].toString());
		if(pSite == NULL)
			bRet = false;
		else if(Request["Action"] == "IndexSite")
			bRet = pSite->CrawlIndex(Request["SiteEntry"].toString(), Request.value("CrawlingDepth",-1).toInt());
		else if(Request["Action"] == "CrawlSite")
			bRet = pSite->CrawlEntries();
		else if(Request["Action"] == "StopTasks")
			pSite->StopCrawling();
		else if(Request["Action"] == "BlastKad")
		{
			pSite->GetBlaster()->SyncEntries();
			pSite->SetProperty("LastIndexTime", GetTime());
		}
	}

	SMPL_RESPONSE(bRet ? "ok" : "fail");
}

/**
* GetCrawler returns Crawler settings
*
* Request: 
*	{
*	}
*
* Response:
*	{
*	}
*
*/
QVariantMap CCoreServer::GetCrawler(QVariantMap Request)
{
	CCrawlingSite* pSite = theCore->m_CrawlingManager->GetSite(Request["SiteName"].toString());
	if(pSite == NULL)
		SMPL_RESPONSE("fail");

	QVariantMap Response;
	if(Request.contains("Options"))
	{
		QVariantMap Options;
		foreach(const QString& Name, Request["Options"].toStringList())
			Options[Name] = pSite->GetProperty(Name);
		Response["Options"] = Options;
	}
	else
	{
		QVariantMap Properties;
		foreach(const QString& Name, pSite->GetAllProperties())
			Properties[Name] = pSite->GetProperty(Name);
		Response["Properties"] = Properties;
	}
	return Response;
}

/**
* SetCrawler sets Crawler settings
*
* Request: 
*	{
*	}
*
* Response:
*	{
*		"Result":	String; "ok"
*	}
*/
QVariantMap CCoreServer::SetCrawler(QVariantMap Request)
{
	CCrawlingSite* pSite = theCore->m_CrawlingManager->GetSite(Request["SiteName"].toString());
	if(pSite == NULL)
		SMPL_RESPONSE("fail");

	if(Request.contains("Options"))
	{
		QVariantMap Options = Request["Options"].toMap();
		foreach(const QString& Name, Options.keys())
			pSite->SetProperty(Name, Options.value(Name));
	}
	else
	{
		QVariantMap Properties = Request["Properties"].toMap();
		foreach(const QString& Name, Properties.keys())
			pSite->SetProperty(Name, Properties.value(Name));
	}
	SMPL_RESPONSE("ok");
}
#endif

/**
* SearchList returns a list of searches
*
* Request: 
*	{
*	}
*
* Response:	
*	{
*		"Searches":
*		[{
*			"ID":			uint64; Search ID
*			"Expression":	String; Search String
*			"SearchNet":	String; "NeoKad"/"MuleKad" Search expression
*			"Status":		String; "Running"/"Finished"/"Failed"
*		}
*		,...]
*	}
*
*/

QVariantMap CCoreServer::SearchList(QVariantMap Request)
{
	QVariantList Searches;
	foreach(uint64 ID, theCore->m_SearchManager->GetSearchIDs())
	{
		CSearch* pSearch = theCore->m_SearchManager->GetSearch(ID);

		QVariantMap Search;
		Search["ID"] = ID;
		Search["SearchNet"] = CSearch::GetSearchNetStr(pSearch->GetSearchNet());
		Search["Expression"] = pSearch->GetExpression();
		QVariantMap Criteria;
		foreach(const QString& Name, pSearch->GetAllCriterias())
			Criteria[Name] = pSearch->GetCriteria(Name);
		Search["Criteria"] = Criteria;
		if(pSearch->IsRunning())
			Search["Status"] = "Running";
		else if(pSearch->HasError())
			Search["Status"] = "Error: " + pSearch->GetError();
		else
			Search["Status"] = "Finished";
		Search["Count"] = pSearch->GetResultCount();
	
		Searches.append(Search);
	}
	RESPONSE("Searches",Searches);
}

/**
* StartSearch StartSearch
*
* Request: 
*	{
*		"SearchNet":	String; "NeoKad"/"Torrent"/"MuleKad" Search expression
*		"Expression":	String; Search expression
*		"Criteria":	
*		{
*			"Name":			String; value
*			...
*		}
*	}
*
* Response:	
*	{
*		"ID":			uint64; SearchID
*	}
*
*/

QVariantMap CCoreServer::StartSearch(QVariantMap Request)
{
	uint64 ID = 0;
	if(Request.contains("ID"))
	{
		ID = Request["ID"].toULongLong();
		if(!theCore->m_SearchManager->StartSearch(ID))
			ID = 0;
	}
	else
	{
		ESearchNet SearchNet = eInvalid;
		if(Request["SearchNet"] == "NeoKad")
			SearchNet = eNeoKad;
		else if(Request["SearchNet"] == "MuleKad")
			SearchNet = eMuleKad;
		else if(Request["SearchNet"] == "Ed2kServer")
			SearchNet = eEd2kServer;
		else if(Request["SearchNet"] == "WebSearch")
			SearchNet = eWebSearch;
		else //if(Request["SearchNet"] == "SmartAgent")
			SearchNet = eSmartAgent;

		if(!Request.contains("Hash"))
			ID = theCore->m_SearchManager->StartSearch(SearchNet,Request["Expression"].toString(),Request["Criteria"].toMap());
		else if(SearchNet == eNeoKad)
		{
			QVariantMap Criteria;
			Criteria["Hash"] = Request["Hash"].toByteArray();
			Criteria["Type"] = Request["Type"].toByteArray();
			ID = theCore->m_SearchManager->StartSearch(eNeoKad, "", Criteria);
		}
	}
	RESPONSE("ID",ID);
}

/**
* StopSearch stop/close a search
*
* Request: 
*	{
*		"ID":			uint64; SearchID
*	}
*
* Response:	
*	{
*	}
*
*/
QVariantMap CCoreServer::StopSearch(QVariantMap Request)
{
	uint64 ID = Request["ID"].toULongLong();

	theCore->m_SearchManager->StopSearch(ID);

	SMPL_RESPONSE("ok");
}

/**
* DiscoverContent start new content discovery or resume a halted one
*
* Request: 
*	{
*		Expression:		string; Search String / keywords
*		Category:		string; Series/Movie/Anime
*		//SubCategory:	string; for anime Movie/OVA/TV
*		//Quality:		string; 480p/720p/1080p
*		Genre:			string; All/...
*		Type:			string; btih|arch|ed2k|neo
*		Language:		string; DE/EN/...
*		Sorting:		string; sort by
*		Order:			string; asc/desc
*	}
*
* Response:	
*	{
*		ID:				uint64; discovery ID;
*	}
*
*/
QVariantMap CCoreServer::DiscoverContent(QVariantMap Request)
{
	uint64 ID = theCore->m_SearchManager->DiscoverContent(Request);

	RESPONSE("ID",ID);
}

/**
* FetchContents get discovered content on thel fly
*
* Request: 
*	{
*		ID:				uint64; discovery ID;
*	}
*
* Response:	
*	{
*	}
*
*/
QVariantMap CCoreServer::FetchContents(QVariantMap Request)
{
	uint64 SearchID = Request["ID"].toULongLong();

	CSearchAgent* pAgent = qobject_cast<CSearchAgent*>(theCore->m_SearchManager->GetSearch(SearchID));
	if(!pAgent)
		SMPL_RESPONSE("Error");

	QList<CCollection*> Collections = pAgent->GetCollections();

	QVariantMap Response;

	bool bPrope = Request["Probe"].toBool();
	
	int Limit = Request["Limit"].toInt();
	
	int NewCounter = 0;
	int PendingCount = 0;

	SContentStateCache* pMapState = NULL;
	QMap<uint64, SCachedContent*> CacheMap;
	if(Request.contains("Token"))
	{
		uint64 Token = Request["Token"].toULongLong();
		Token = ((Token & 0x000F000000000000) == 0x0004000000000000) ? Token & 0x0000FFFFFFFFFFFF : 0; // check if tocken is valid and remove flag
		pMapState = (SContentStateCache*)m_StatusCache.value(Token);
		if(!pMapState)
		{
			pMapState = new SContentStateCache;

			do Token = (GetRand64() & 0x0000FFFFFFFFFFFF);
			while (Token && m_StatusCache.contains(Token));

			m_StatusCache.insert(Token, pMapState);
		}
		pMapState->LastUpdate = GetCurTick();
		CacheMap = pMapState->Map;
		Response["Token"] = Token | 0x0004000000000000; // Content flag
	}

	QVariantList CollectionList;
	foreach(CCollection* pCollection, Collections)
	{
		uint64 ID = pCollection->GetID();

		QString Img = pCollection->GetDetails()->GetProperty(PROP_COVERURL).toString();
		if(Img.left(4).compare("http", Qt::CaseInsensitive) != 0)
			continue; // check if cover is valid

		if(pCollection->IsCollecting())
		{
			PendingCount++;
			continue;
		}

		SCachedContent* pState = CacheMap.take(ID);

		if(!pState)
		{
			if(Limit && NewCounter >= Limit)
				break;

			NewCounter++; // count newly added collections
		}

		if(bPrope)
			continue;

		if(!pState && pMapState)
		{
			pState = new SCachedContent();
			ASSERT(!pMapState->Map.contains(ID));
			pMapState->Map.insert(ID, pState);
		}
		else // no updates for now
			continue;
		//QVariantMap* pUpdate = NULL;

		QVariantMap Collection;
		Collection["ID"] = ID;
		Collection["Name"] = pCollection->GetName();
		Collection["Description"] = pCollection->GetDetails()->GetProperty(PROP_DESCRIPTION);
		Collection["CoverUrl"] = Img;
		Collection["Rating"] = pCollection->GetDetails()->GetProperty(PROP_RATING);
		Collection["ID_imdb"] = pCollection->GetDetails()->GetID("imdb");
		
		//if(pUpdate)
		//{
		//	(*pUpdate)["ID"] = ID;
		//	CollectionList.append(*pUpdate);
		//	delete pUpdate;
		//}
		CollectionList.append(Collection);
	}

	if(!NewCounter && Request["More"].toBool())
		theCore->m_SearchManager->FindMore(SearchID);

	if(bPrope)
	{
		Response["IsRunning"] = pAgent->IsRunning();
		Response["PendingCount"] = PendingCount;
		Response["NewCount"] = NewCounter;
	}
	else
	{
		/*for(QMap<uint64, SCachedContent*>::iterator I = CacheMap.begin(); I != CacheMap.end(); I++)
		{
			QVariantMap Update;
			Update["ID"] = I.key();
			Update["CollectionStatus"] = "Deprecated";
			CollectionList.append(Update);
			if(pMapState)
				delete pMapState->Map.take(I.key());
		}*/

		Response["Collections"] = CollectionList;
	}

	return Response;
}

/**
* GetContent get one particular found result
*
* Request: 
*	{
*	}
*
* Response:	
*	{
*	}
*
*/
QVariantMap CCoreServer::GetContent(QVariantMap Request)
{
	QVariantMap Response;

	if(CCollection* pCollection = CCollection::GetCollection(Request["ID"].toULongLong()))
	{
		bool bIsCrawling = false;
		Response = pCollection->GetContent(bIsCrawling);
		Response["IsCrawling"] = bIsCrawling;
	}
	else
		Response["Error"] = "Collection Not Found";

	return Response;
}

/**
* GetStream pick a stream to play out of a multi file
*
* Request: 
*	{
*	}
*
* Response:	
*	{
*	}
*
*/
QVariantMap CCoreServer::GetStream(QVariantMap Request)
{
	QVariantMap Result;
	Result["ID"] = Request["ID"];

	if(CFile* pFile = CFileList::GetFile(Request["ID"].toULongLong()))
	{
		uint64 StreamID = pFile->GetFileID();

		uint64 uMaxSize = 0;
		foreach(uint64 FileID, pFile->GetSubFiles())
		{
			if(CFile* pSubFile = CFileList::GetFile(FileID))
			{
				if(pSubFile->GetFileSize() > uMaxSize)
				{
					uMaxSize = pSubFile->GetFileSize();
					StreamID = pSubFile->GetFileID();
				}
			}
		}

		Result["StreamID"] = StreamID;
	}
	else
		Result["FileStatus"] = "File Not Found";

	return Result;
}

/**
* GetCaptchas 
*
* Request: 
*	{
*		"Mode":		"Full"/"Info"
*	}
*
* Response:	
*	{
*		"Captchas": 
*		[{
*			"Text":			Text Query
*			"Image":		ImageID
*			"ID":			CaptchaID
*		},...]
*	}
*
*/
QVariantMap CCoreServer::GetCaptchas(QVariantMap Request)
{
	QVariantList Captchas;
#ifndef NO_HOSTERS
	foreach(uint64 ID, theCore->m_CaptchaManager->GetCaptchas())
	{
		QVariantMap Captcha;
		Captcha["ID"] = ID;

		QByteArray Picture;
		QString Ext;
		QString Text;
		QString Info;
		theCore->m_CaptchaManager->GetCaptcha(ID, Picture, Ext, Text, Info);
		if(Request["Mode"] == "Full")
			Captcha["Data"] = Picture;
		Captcha["Ext"] = Ext;
		Captcha["Text"] = Text;
		Captcha["Info"] = Info;
		Captcha["Image"] = ID;

		Captchas.append(Captcha);
	}
#endif
	RESPONSE("Captchas",Captchas);
}

/**
* SetSolution 
*
* Request: 
*	{
*		"Solution":		Solution
*		"ID":			ID
*	}
*
* Response:	
*	{
*	}
*
*/
QVariantMap CCoreServer::SetSolution(QVariantMap Request)
{
#ifndef NO_HOSTERS
	theCore->m_CaptchaManager->CaptchaSolved(Request["ID"].toULongLong(), Request["Solution"].toString());
#endif
	SMPL_RESPONSE("ok");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Helper Section
//

UINT CCoreServer::GetFileType(CFile* pFile)
{
#ifndef NO_HOSTERS
	if(pFile->IsRawArchive())
		return SCachedFile::eArchive;
#endif
	if(pFile->IsMultiFile())
		return SCachedFile::eMultiFile;
	return SCachedFile::eFile;
}		
		
QString CCoreServer::FileTypeToStr(UINT Type) 
{
	switch(Type)
	{
		case SCachedFile::eFile:			return "File";
		case SCachedFile::eArchive:			return "Archive";
		case SCachedFile::eCollection:		return "Collection";
		case SCachedFile::eMultiFile:		return "MultiFile";
		default:							return "Unknown";
	}
};

UINT CCoreServer::GetFileState(CFile* pFile)
{
	if(pFile->IsRemoved())
		return SCachedFile::eRemoved;
	if(pFile->IsHalted())
		return SCachedFile::eHalted;
	if(pFile->IsPending())
		return SCachedFile::ePending;
	if(pFile->IsDuplicate())
		return SCachedFile::eDuplicate;
	if(pFile->IsPaused(true))
		return SCachedFile::ePaused;
	if(pFile->IsStarted())
		return SCachedFile::eStarted;
	return SCachedFile::eStopped;
}

QString CCoreServer::FileStateToStr(UINT State) 
{
	switch(State)
	{
		case SCachedFile::eRemoved:			return "Removed";
		case SCachedFile::eHalted:			return "Halted";
		case SCachedFile::ePending:			return "Pending";
		case SCachedFile::eDuplicate:		return "Duplicate";
		case SCachedFile::eStarted:			return "Started";
		case SCachedFile::ePaused:			return "Paused";
		case SCachedFile::eStopped:			return "Stopped";
		default:							return "Unknown";
	}
};

UINT CCoreServer::GetFileStatus(CFile* pFile)
{
	if(pFile->HasError())
		return SCachedFile::eError;
	if(pFile->MetaDataMissing() 
#ifndef NO_HOSTERS
		&& !pFile->IsRawArchive()
#endif
		)
		return SCachedFile::eEmpty;
	if(pFile->IsComplete(true))
		return SCachedFile::eComplete;
	return SCachedFile::eIncomplete;
}

QString CCoreServer::FileStatusToStr(UINT Status) 
{
	switch(Status)
	{
		case SCachedFile::eError:			return "Error";
		case SCachedFile::eEmpty:			return "Empty";
		case SCachedFile::eIncomplete:		return "Incomplete";
		case SCachedFile::eComplete:		return "Complete";
		default:							return "Unknown";
	}
};

UINT CCoreServer::GetFileJobs(CFile* pFile)
{
	UINT Jobs = SCachedFile::eNone;
	if(pFile->IsAllocating())
		Jobs |= SCachedFile::eAllocating;
	if(pFile->IsHashing())
		Jobs |= SCachedFile::eHashing;
#ifndef NO_HOSTERS
	if(theCore->m_ArchiveDownloader->IsExtracting(pFile))
		Jobs |= SCachedFile::eExtracting;
	if(theCore->m_ArchiveUploader->IsPacking(pFile))
		Jobs |= SCachedFile::ePacking;
#endif
	if(pFile->GetDetails()->IsSearching())
		Jobs |= SCachedFile::eSearching;
	return Jobs;
}

QStringList CCoreServer::FileJobsToStr(UINT Jobs) 
{
	QStringList JobList;
	if(Jobs & SCachedFile::eAllocating)
		JobList.append("Allocating");
	if(Jobs & SCachedFile::eHashing)
		JobList.append("Hashing");
	if(Jobs & SCachedFile::eExtracting)
		JobList.append("Extracting");
	if(Jobs & SCachedFile::ePacking)
		JobList.append("Packing");
	if(Jobs & SCachedFile::eSearching)
		JobList.append("Searching");
	return JobList;
};

/////////

UINT CCoreServer::GetTransferStatus(CTransfer* pTransfer)
{
	if(pTransfer->HasError())
		return SCachedTransfer::eError;
	if(pTransfer->IsConnected())
		return SCachedTransfer::eConnected;
	if(pTransfer->IsChecked())
		return SCachedTransfer::eChecked;
	return SCachedTransfer::eUnchecked;
}

QString CCoreServer::TransferStatusToStr(UINT Status)
{
	switch(Status)
	{
		case SCachedTransfer::eUnchecked:	return "Unchecked";
		case SCachedTransfer::eConnected:	return "Connected";
		case SCachedTransfer::eChecked:		return "Checked";
		case SCachedTransfer::eError:		return "Error";
		default:							return "Unknown";
	}
}

UINT CCoreServer::GetUploadState(CTransfer* pTransfer)
{
	if(pTransfer->IsActiveUpload())
		return SCachedTransfer::eTansfering;
	if(pTransfer->IsWaitingUpload())
		return SCachedTransfer::eWaiting;
	if(pTransfer->IsUpload())
		return SCachedTransfer::eIdle;
	return SCachedTransfer::eNone;
}

UINT CCoreServer::GetDownloadState(CTransfer* pTransfer)
{
	if(pTransfer->IsActiveDownload())
		return SCachedTransfer::eTansfering;
	if(pTransfer->IsWaitingDownload())
		return SCachedTransfer::eWaiting;
	if(pTransfer->IsInteresting())
		return SCachedTransfer::ePending;
	if(pTransfer->IsDownload())
		return SCachedTransfer::eIdle;
	return SCachedTransfer::eNone;
}

QString CCoreServer::TransferStateToStr(UINT State)
{
	switch(State)
	{
		case SCachedTransfer::eIdle:		return "Idle";
		case SCachedTransfer::ePending:		return "Pending";
		case SCachedTransfer::eWaiting:		return "Waiting";
		case SCachedTransfer::eTansfering:	return "Tansfering";
		default:							return "None";
	}
}

/////////

QVariantList CCoreServer::DumpLog(const QList<CLog::SLine>& Log, uint64 uLast)
{
	int Index = -1;
	if(uLast)
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
		//if(LOG_MOD('w') & Log.at(i)->m_uFlag)
		//	continue; // Note: this log lines are for the web debuger only

		QVariantMap LogEntry;
		LogEntry["ID"] = Log.at(i).uID;
		LogEntry["Flag"] = Log.at(i).uFlag;
		LogEntry["Stamp"] = (quint64)Log.at(i).uStamp;
		LogEntry["Line"] = Log.at(i).Line;
		Entries.append(LogEntry);
	}
	return Entries;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Test Section
//

QVariantMap CCoreServer::Test(QVariantMap Request)
{
	/*SNeoEntity Neo;
	Neo.EntityID = QByteArray::fromHex("6120f08a5cb1f3bf");
	Neo.TargetID = QByteArray::fromHex("f68f88df41a2fc030cab05dade58dd42");

	CNeoSession* Session = theCore->m_NeoManager->OpenSession(Neo);
	CNeoClient* pClient = new CNeoClient(Session);
	connect(Session, SIGNAL(Connected()), pClient, SLOT(OnConnected()));
	theCore->m_NeoManager->AddConnection(pClient, true);*/
	return QVariantMap();
}
