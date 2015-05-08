#include "GlobalHeader.h"
#include "NeoKad.h"
#include "../../NeoCore.h"
#include "../../Interface/InterfaceManager.h"
#include "../../FileSearch/Search.h"
#include "../../FileSearch/SearchManager.h"
#include "../../FileList/FileManager.h"
#include "../../FileList/File.h"
#include "../../FileList/FileDetails.h"
#include "../../FileList/FileStats.h"
#include "../../FileTransfer/BitTorrent/TorrentManager.h"
#include "../../FileTransfer/BitTorrent/Torrent.h"
#include "../../FileTransfer/ed2kMule/MuleManager.h"
#include "../../../Framework/Cryptography/HashFunction.h"
#include "../../FileList/Hashing/FileHashTree.h"
#include "../../FileList/Hashing/FileHashTreeEx.h"
#include "../../FileList/Hashing/FileHashSet.h"
#include "../../../Framework/Xml.h"
#include "NeoManager.h"
#include "NeoKad/KadPublisher.h"
#include "../HashInspector.h"
#include "../../Networking/SocketThread.h"
#include "../../../MiniUPnP/MiniUPnP.h"

//QRegExp g_kwrdSpliter("[" + QRegExp::escape("\t\r\n !\"&/()=?{}[]+*~#\',;.:_<>|\\") + "\\-]");
QRegExp g_kwrdSpliter("[\t\r\n \\!\\\"\\&\\/\\(\\)\\=\\?\\{\\}\\[\\]\\+\\*\\~\\#\\\'\\,\\;\\.\\:\\-\\_\\<\\>\\|\\\\]");
//QRegExp g_kwrdSpliter("^[0-9a-zA-Z]"); // everything else splits

#ifndef _DEBUG
#define AUTH
#endif

CNeoKad::CNeoKad(QObject* qObject)
 : QObjectEx(qObject), m_Scripts(":/KadScripts/Index.inf", QSettings::IniFormat)
{
	m_Scripts.sync();

	m_KadStatus = eDisconnected;

	m_UpRate = 0;
	m_DownRate = 0;

	m_FWStatus = 0;
	m_FWStatusV6 = 0;

	m_uLastLog = 0;

	QByteArray StoreKey = theCore->Cfg()->GetBlob("NeoKad/StoreKey");
	if(StoreKey.isEmpty() || !m_PrivateKey.SetKey(StoreKey))
	{
		m_PrivateKey.SetAlgorithm(CAbstractKey::eECP);
		m_PrivateKey.GenerateKey("secp256r1"); // P-256

		theCore->Cfg()->SetBlob("NeoKad/StoreKey", m_PrivateKey.ToByteArray());
	}

	m_KadPublisher = new CKadPublisher(this);

	theCore->m_Interfaces->EstablishInterface("NeoKad");
}

CNeoKad::~CNeoKad()
{
	foreach(SFile* pFileRef, m_Files)
		delete pFileRef;
	m_Files.clear();
}

bool CNeoKad::IsEnabled() const
{
	return theCore->m_Interfaces->IsInterfaceEstablished("NeoKad");
}

void CNeoKad::Process(UINT Tick)
{
	if(Tick & EPerSec)
	{
		if(theCore->Cfg()->GetBool("Log/Merge"))
			SyncLog();
	}

	QVariantMap Request;
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "QueryState", Request).toMap();

	if(Response["Result"] == "Disconnected")
	{
		if(m_KadStatus != eDisconnected)
		{
			m_KadID.clear();
			m_KadStatus = eDisconnected;
		}
		
		StartKad();
		return;
	}

	if(!theCore->m_Network->IsConnectable())
		return; // VPN is down deny connecting

	m_Port = Response["Port"].toUInt();
	CAddress IPv4(Response["IPv4"].toString());
	CAddress IPv6(Response["IPv6"].toString());

	m_UpRate = Response["UpRate"].toInt();
	m_DownRate = Response["DownRate"].toInt();

	if(m_Port != theCore->Cfg()->GetInt("NeoKad/Port")
		|| IPv4 != theCore->m_Network->GetIPv4() || IPv6 != theCore->m_Network->GetIPv6())
	{
		StopKad();

		QVariantMap Request;
		Request["Port"] = theCore->Cfg()->GetInt("NeoKad/Port");
		Request["IPv4"] = theCore->m_Network->GetIPv4().ToQString();
		Request["IPv6"] = theCore->m_Network->GetIPv6().ToQString();
		QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "SetupSockets", Request).toMap();	

		if(theCore->Cfg()->GetBool("Bandwidth/UseUPnP"))
		{
			int Port = 0;
			if(theCore->m_MiniUPnP->GetStaus("NeoKad", &Port) == -1 || Port != theCore->Cfg()->GetInt("NeoKad/Port"))
				theCore->m_MiniUPnP->StartForwarding("NeoKad", theCore->Cfg()->GetInt("NeoKad/Port"), "UDP");
		}
		else
		{
			if(theCore->m_MiniUPnP->GetStaus("NeoKad") != -1)
				theCore->m_MiniUPnP->StopForwarding("NeoKad");
		}

		StartKad();
		return;
	}

	if(Response["Result"] == "Connected")
	{
		if(m_KadStatus != eConnected)
		{
			m_KadID = Response["KadID"].toByteArray();
			QByteArray KadID = m_KadID;
			std::reverse(KadID.begin(), KadID.end());
			LogLine(LOG_SUCCESS, tr("NeoKad Connected, ID: %1").arg(QString(KadID.toHex()).toUpper()));
			SyncScripts(Response["Scripts"].toList());
			m_KadStatus = eConnected;
		}
		
		/*Response["FWStatusV4"]
		Response["AddressV6"]
		Response["FWStatusV6"]*/

		m_Address = CAddress(Split2(Split2(Response["AddressV4"].toString(),"://").second, ":").first); // utp://1.2.3.4:5/
		if(Response["FWStatusV4"] == "Open")		m_FWStatus = 1;
		else if(Response["FWStatusV4"] == "NATed")	m_FWStatus = 2;
		else										m_FWStatus = 3;

		m_AddressV6 = CAddress(Split2(Split2(Response["AddressV6"].toString(),"://").second, ":").first);
		if(Response["FWStatusV6"] == "Open")		m_FWStatusV6 = 1;
		else if(Response["FWStatusV6"] == "NATed")	m_FWStatusV6 = 2;
		else										m_FWStatusV6 = 3;
	}
	else //if(Response["Result"] == "Connecting")
	{
		m_KadStatus = eConnecting;
		return;
	}

	if(Tick & EPerSec)
	{
		SyncFiles();
		foreach(CAbstractSearch* pSearch, m_RunningSearches)
			SyncSearch(pSearch);
		m_KadPublisher->Process(Tick);
	}
}

bool CNeoKad::IsFirewalled(CAddress::EAF eAF, bool bNAT) const
{
	if(eAF == CAddress::IPv6)
		return m_FWStatusV6 == (bNAT ? 2 : 1);
	return m_FWStatus == (bNAT ? 2 : 1);
}

CAddress CNeoKad::GetAddress(CAddress::EAF eAF) const
{
	if(eAF == CAddress::IPv6)
		return m_AddressV6;
	return m_Address;
}

void CNeoKad::SyncFiles()
{
	QMap<uint64, SFile*> Files = m_Files;

	time_t uNow = GetCurTick();
	foreach(CFile* pFile, CFileList::GetAllFiles())
	{
		if(!pFile->IsStarted())
			continue;

		bool bMulti = pFile->IsMultiFile();
		uint64 FileID = pFile->GetFileID();
		SFile* pFileRef = Files.value(FileID);
		if(pFileRef == NULL)
		{
			pFileRef = new SFile;
			m_Files.insert(FileID, pFileRef);
			pFileRef->FileID = FileID;
		}
		Files[FileID] = NULL;
		
		if(pFileRef->NextSourceSearch < GetCurTick() && m_KadPublisher->FindSources(pFile))
			pFileRef->NextSourceSearch = uNow + SEC2MS(theCore->Cfg()->GetInt("NeoShare/KadLookupInterval"));

		if(pFile->IsIncomplete())
		{
			QMap<EFileHashType, CFileHashPtr>& HashMap = pFile->GetHashMap();
			// if one of the distinct hashes is missing auto obtain it
			if(((!HashMap.contains(bMulti ? HashXNeo : HashNeo) || !HashMap.contains(HashEd2k) || !HashMap.contains(HashTorrent)) && pFileRef->NextAliasSearch < GetCurTick()) && m_KadPublisher->FindAliases(pFile))
				pFileRef->NextAliasSearch = uNow + SEC2MS(theCore->Cfg()->GetInt("NeoShare/KadLookupInterval"));
		}

		if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFile->GetHash(bMulti ? HashXNeo : HashNeo)))
		{
			if(!pHashTree->IsComplete())
			{
				if(pFileRef->NextHashSearch < GetCurTick() && m_KadPublisher->FindHash(pFile))
					pFileRef->NextHashSearch = uNow + SEC2MS(theCore->Cfg()->GetInt("NeoShare/KadLookupInterval"));
			}

			if(bMulti && pFile->IsIncomplete() && pFile->GetInspector()->GetIndexSource() != HashXNeo)
			{
				if(pFileRef->NextIndexSearch < GetCurTick() && m_KadPublisher->FindIndex(pFile))
					pFileRef->NextIndexSearch = uNow + SEC2MS(theCore->Cfg()->GetInt("NeoShare/KadLookupInterval"));
			}
		}
	}

	for(QMap<uint64, SFile*>::iterator I = Files.begin(); I != Files.end();I++)
	{
		SFile* pFileRef = I.value();
		if(!pFileRef)
			continue;
		m_Files.remove(I.key());

		delete pFileRef;
	}
}

void CNeoKad::ResetPub(CFile* pFile)
{
	m_KadPublisher->ResetFile(pFile);
}

bool CNeoKad::FindSources(CFile* pFile)
{
	if(!IsConnected())
		return false;
	return m_KadPublisher->FindSources(pFile, true);
}

bool CNeoKad::FindIndex(CFile* pFile)
{
	if(!pFile->IsIncomplete())
		return false;
	if(!IsConnected())
	{
		LogLine(LOG_ERROR | LOG_DEBUG, tr("Neo Kad Not Connected"));
		return false;
	}
	return m_KadPublisher->FindIndex(pFile, true);
}

bool CNeoKad::FindRating(CFile* pFile)
{
	if(!IsConnected())
	{
		LogLine(LOG_ERROR | LOG_DEBUG, tr("Neo Kad Not Connected"));
		return false;
	}
	return m_KadPublisher->FindRating(pFile, true);
}

bool CNeoKad::IsFindingRating(CFile* pFile)
{
	return m_KadPublisher->IsFindingRating(pFile);
}

void CNeoKad::StartKad()
{
	LogLine(LOG_INFO, tr("Connecting NeoKad"));

	m_KadStatus = eConnecting;

	QVariantMap Request;
	theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "Connect", Request);
}

void CNeoKad::StopKad()
{
	m_KadID.clear();
	m_KadStatus = eDisconnected;

	QVariantMap Request;
	theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "Disconnect", Request);
}

void CNeoKad::SyncScripts(const QVariantList ScriptList)
{
	QMap<QByteArray, QVariantMap> KnownScripts;
	foreach(const QVariant vScript, ScriptList)
	{
		QVariantMap Script = vScript.toMap();
		KnownScripts[Script["CodeID"].toByteArray()] = Script;
	}

	foreach(const QString& Name, m_Scripts.childGroups())
	{
		QByteArray CodeID = QByteArray::fromHex(m_Scripts.value(Name + "/CodeID").toByteArray());
		QVariantMap Script = KnownScripts.value(CodeID);
#ifdef AUTH
		if(Script["Version"].toUInt() < m_Scripts.value(Name + "/Version").toUInt())
#endif
		{
			LogLine(LOG_DEBUG | LOG_INFO, tr("Installing script %1 in NeoKad").arg(m_Scripts.value(Name + "/Name").toString()));
			QVariantMap Request;
			Request["CodeID"] = CodeID;
			QFile SourceFile(":/KadScripts/" + Name + ".js");
			SourceFile.open(QFile::ReadOnly);
			Request["Source"] = SourceFile.readAll();

			// Note: we can not modify authenticated script on the fly without reauthentication, so in debug mode we dont authenticate them
#ifdef AUTH
			Request["Authentication"] = QByteArray::fromBase64(m_Scripts.value(Name + "/Authentication").toByteArray());
#endif

			QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "InstallScript", Request).toMap();
			if(Response["Result"] != "ok")
				LogLine(LOG_DEBUG | LOG_ERROR, tr("Installing script %1 failed!").arg(m_Scripts.value(Name + "/Name").toString()));
		}
	}
}

void CNeoKad::SyncLog()
{
	QVariantMap Request;
	Request["LastID"] = m_uLastLog;
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "GetLog", Request).toMap();
	QVariantList Lines = Response["Lines"].toList();
	if(Lines.isEmpty())
		return;
	QVariantMap LogEntry;
	for(int i = 0; i < Lines.size(); i ++)
	{
		LogEntry = Lines.at(i).toMap();
		AddLogLine((time_t)LogEntry["Stamp"].toULongLong(), LogEntry["Flag"].toUInt() | LOG_XMOD('n') | LOG_DEBUG, LogEntry["Line"]);
	}
	m_uLastLog = LogEntry["ID"].toULongLong();
}

void CNeoKad::StartSearch(CAbstractSearch* pSearch)
{
	ASSERT(!pSearch->IsRunning());

	if(!IsConnected())
	{
		pSearch->SetError("Neo Kad Not Connected");
		return;
	}

	QByteArray Hash;
	if(pSearch->IsAliasSearch())
		Hash = pSearch->GetCriteria("Hash").toByteArray();
	else
	{
		QString Keyword;
		// "bla blup" <- tis exact sequence of words
		// bla/blup <- the word bla or blup
		// bl* <- wildcard 
		// -blup must not be contained
		// bla must be contained
		QStringList Expressions = pSearch->GetExpression().split(QRegExp(" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)"), QString::SkipEmptyParts); // split keeping "bla blup" intact
		if(Expressions.isEmpty()) // topfile search
			Keyword = m_KadPublisher->MkTopFileKeyword();
		else
		{
			foreach(const QString& Expression, Expressions)
			{
				if(!Expression.contains(g_kwrdSpliter))
				{
					Keyword = Expression.toLower();
					break;
				}
			}
		}

		if(Keyword.isEmpty())
		{
			pSearch->SetError("Invalid Expression");
			LogLine(LOG_ERROR, tr("Failed to start keyword lookup, at least one regular keyword must be specifyed"));
			return;
		}

		Hash = CHashFunction::Hash(Keyword.toUtf8(), CHashFunction::eSHA256);
	}

	QVariantMap Request = GetLookupCfg();
	Request["TargetID"] = CKadPublisher::MkTargetID(Hash);
	Request["CodeID"] = pSearch->IsAliasSearch() ? GetCodeID("FileRepository") : GetCodeID("KeywordIndex");

	QVariantList Execute;
	QVariantMap Find;
	Find["Function"] = pSearch->IsAliasSearch() ? "findNestings" : "findFiles";
	//Find["ID"] =;
	QVariantMap Parameters;
	Parameters["EXP"] = pSearch->GetExpression();
	foreach(const QString& Name, pSearch->GetAllCriterias())
	{
		const QVariant& Value = pSearch->GetCriteria(Name);
		if(Name == "Type" && Value.toString() != "")
			Parameters["HF"] = Value;
		// else unsupported
	}
	Find["Parameters"] = Parameters;
	Execute.append(Find);
	Request["Execute"] = Execute;

	Request["GUIName"] = QString(pSearch->IsAliasSearch() ? "findNestings" : "findFiles") + ": " + pSearch->GetExpression(); // for GUI only

	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "StartLookup", Request).toMap();
	QByteArray LookupID = Response["LookupID"].toByteArray();
	if(!LookupID.isEmpty())
	{
		pSearch->SetStarted(LookupID);
		m_RunningSearches.insert(pSearch);
	}
	else if(Response.contains("Error"))
	{
		pSearch->SetError(Response["Error"].toString());
		LogLine(LOG_ERROR | LOG_DEBUG, tr("Failed to start keyword lookup, error: %1").arg(Response["Error"].toString()));
	}
	else
		pSearch->SetError("Internal Error");
}

void CNeoKad::SyncSearch(CAbstractSearch* pSearch)
{
	ASSERT(pSearch->IsRunning());

	QVariantMap Request;
	Request["LookupID"] = pSearch->GetSearchID();
	Request["AutoStop"] = true;
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "QueryLookup", Request).toMap();

	foreach(const QVariant& vResult, Response["Results"].toList())
	{
		QVariantMap Result = vResult.toMap();
		//Result["ID"]
		QVariantMap Return = Result["Return"].toMap();
	
		foreach(const QVariant& vFile, Return["FL"].toList())
			AddFileToSearch(vFile.toMap(), pSearch);
	}

	if(Response["Staus"] == "Finished")
	{
		pSearch->SetStopped();
		m_RunningSearches.remove(pSearch);
	}
}

void CNeoKad::AddFileToSearch(QVariantMap File, CAbstractSearch* pSearch)
{
	uint64 uFileSize = File["FS"].toULongLong();  // FileSize

	CFile* pFile = new CFile();

	EFileHashType MasterHash = HashUnknown;
	QVariantMap HashMap = File["HM"].toMap(); // HashMap
	foreach(CFileHashPtr pHash, CKadPublisher::ReadHashMap(HashMap, uFileSize))
	{
		if(pHash->GetType() < MasterHash) // Hash Values are sorted by thair priority
			MasterHash = pHash->GetType();
		pFile->AddHash(pHash);
	}
	// Note: we try to keep trace of the initial release ID
	//if(File.contains("RID"))
	//	pFile->SetProperty("ReleaseID", File["RID"]);

	QString Type = pSearch->GetCriteria("Type", "").toString();
	if(!Type.isEmpty()) // if it was a hashtype specific search the results must contain tha requested hash
	{
		if(Type == "btih")
			MasterHash = HashTorrent;
		else if(Type == "ed2k")
			MasterHash = HashEd2k;
		else if(Type == "arch") // Note: this is a hoster file without a actuall payload hash, we use an ArchHash as a arbitrary random ID
			MasterHash = HashArchive;
	}

	QVariantMap Details;

	Details["Title"] = File["FN"].toString();
	Details["Availability"] = File["AC"].toInt();

	pFile->GetDetails()->Add("neo://search_" + GetRand64Str(), Details);

	pFile->AddEmpty(MasterHash, File["FN"].toString(), uFileSize); // FileName

	pSearch->AddFoundFile(pFile);
}

void CNeoKad::StopSearch(CAbstractSearch* pSearch)
{
	ASSERT(pSearch->IsRunning());

	QVariantMap Request;
	Request["LookupID"] = pSearch->GetSearchID();
	theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "StopLookup", Request);

	pSearch->SetStopped();
	m_RunningSearches.remove(pSearch);
}

QVariantMap	CNeoKad::GetLookupCfg(EKadOp Op)
{
	int Anon = theCore->Cfg()->GetInt("NeoShare/Anonymity");

	int Jumps = 0;
	int Hops = 0;
	switch(Anon)
	{
	case 0: // atempt make own node the closest node (not random target area)
		break;
	case 1: // connect directly to closest nodes in random target area
		break;
	case 2: // publish with 1 hoop share with 1 hoop
		switch(Op) {
		case ePublish:
		case eRoute:
			Hops++;
		} break;
	case 3: // publish with 1 hoop share with 2 hoop 
		switch(Op) {
		case eRoute:
			Jumps++;
		case ePublish:
			Hops++;
		} break;
	case 4: // publish with 2 hoop share with 3 hoop and lookup with 1 hoop
	default:
		switch(Op) {
		case eRoute:
			Jumps++;
		case ePublish:
			Jumps++;
		case eLookup:
			Hops++;
		} break;
	}

	QVariantMap Request;
	Request["HopLimit"] = Hops + Jumps;
	Request["JumpCount"] = Jumps;

	if(Op != eRoute)
	{
		Request["Timeout"] = SEC2MS(60);
/*#ifdef _DEBUG
		switch(Op) {
		case ePublish:
			Request["SpreadCount"] = 4;
		case eLookup:
			Request["SpreadCount"] = 2;
		}
#else*/
		switch(Op) {
		case ePublish:
			Request["SpreadCount"] = 10;
		case eLookup:
			Request["SpreadCount"] = 5;
		}

//#endif

		Request["StoreKey"] = GetStoreKey();
	}
	return Request;
}

QByteArray CNeoKad::GetCodeID(const QString& Script)
{
	return QByteArray::fromHex(m_Scripts.value(Script + "/CodeID").toByteArray());
}

QString	CNeoKad::GetStatus(CFile* pFile, uint64* pNext) const
{
	QString Status;
	if(!IsEnabled())
		Status = "Disabled";
	else if(IsConnected())
	{
		Status = "Connected";

		if(pFile)
		{
			/*m_Publisher
			m_KeywordIndex
			m_FileRepository
			m_SourceTracker
			m_RatingAgent
			//m_SharingHub
			m_LinkSafe

			QDateTime LastPub = 
			time_t uLastPub = LastPub.isValid() ? LastPub.toTime_t() : 0;
			if(uLastPub == 0)
				Status += ", Unpublished";
			else 
				Status += ", Published";*/

			SFile* pFileRef;
			if(pNext && (pFileRef = m_Files.value(pFile->GetFileID())))
			{
				uint64 uNow = GetCurTick();
				if(pFileRef->NextSourceSearch > uNow)
					*pNext = pFileRef->NextSourceSearch - uNow;
			}
		}
	}
	else if(IsDisconnected())
		Status = "Disconnected";
	else
		Status = "Connecting";
	return Status;
}
