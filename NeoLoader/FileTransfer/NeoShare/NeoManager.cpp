#include "GlobalHeader.h"
#include "NeoManager.h"
#include "NeoClient.h"
#include "NeoEntity.h"
#include "NeoRoute.h"
#include "../../FileList/File.h"
#include "../../FileList/FileManager.h"
#include "../../NeoCore.h"
#include "../../NeoVersion.h"
#include "../../Interface/InterfaceManager.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"
#include "../HashInspector.h"
#include "../../FileList/Hashing/FileHashTreeEx.h"

CNeoManager::CNeoManager(QObject* qObject)
 : QObjectEx(qObject)
{
	m_Kademlia = new CNeoKad(this);

	m_UpLimit = new CBandwidthLimit(this);
	m_DownLimit = new CBandwidthLimit(this);

	m_TransferStats.UploadedTotal = theCore->Stats()->value("NeoShare/Uploaded").toULongLong();
	m_TransferStats.DownloadedTotal = theCore->Stats()->value("NeoShare/Downloaded").toULongLong();

	m_Version = GetNeoVersion();
}

CNeoManager::~CNeoManager()
{
	foreach(CNeoClient* pClient, m_Clients)
	{
		if(CNeoSession* pSession = pClient->GetSession())
			pSession->Dispose();
	}
}

void CNeoManager::Process(UINT Tick)
{
	if(!m_Kademlia->IsEnabled())
		return;

	m_Kademlia->Process(Tick);

	if((Tick & EPerSec) == 0)
		return;

	foreach(const QByteArray& MyEntityID, m_Routes.keys())
	{
		CNeoRoute* pRoute = m_Routes.value(MyEntityID);

		if(pRoute->GetTimeOut() < GetCurTick()) // if the route it idle for to long break it
			BreakRoute(pRoute->GetTargetID(), MyEntityID);
		else
		{
			QVariantMap Request;
			Request["MyEntityID"] = MyEntityID;
			QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "QueryRoute", Request).toMap();
			pRoute->SetDuration(Response["Duration"].toULongLong());
			if(Response["Status"] != "Established")
			{
				m_Routes.remove(MyEntityID);
				RouteBroken(MyEntityID);
				if(pRoute->IsStatic()) // if it was a static route reestablish
				{
					CPrivateKey* pEntityKey = new CPrivateKey;
					pEntityKey->SetKey(pRoute->GetEntityKey());
					SetupRoute(pRoute->GetTargetID(), pEntityKey, true);
				}
				delete pRoute;
			}
		}
	}

	int iPriority = theCore->Cfg()->GetInt("NeoShare/Priority");
	m_UpLimit->SetLimit(theCore->Cfg()->GetInt("NeoShare/Upload"));
	//m_UpLimit->SetPriority(iPriority);
	m_DownLimit->SetLimit(theCore->Cfg()->GetInt("NeoShare/Download"));
	m_DownLimit->SetPriority(iPriority);

	if ((Tick & E100PerSec) == 0)
	{
		theCore->Stats()->setValue("NeoShare/Uploaded", m_TransferStats.UploadedTotal);
		theCore->Stats()->setValue("NeoShare/Downloaded", m_TransferStats.DownloadedTotal);
	}

	if(m_Routes.isEmpty())
	{
		int LastDistance = theCore->Cfg()->GetInt("NeoKad/LastDistance");
		QByteArray TargetID = QByteArray::fromHex(theCore->Cfg()->GetString("NeoKad/TargetID").toLatin1());
		if(theCore->Cfg()->GetInt("NeoShare/Anonymity") > 0)
		{
			if(TargetID.size() != KEY_128BIT || LastDistance != 0)
			{
				TargetID = CAbstractKey(KEY_128BIT, true).ToByteArray();
				theCore->Cfg()->SetSetting("NeoKad/TargetID", QString(TargetID.toHex()));
				theCore->Cfg()->SetSetting("NeoKad/LastDistance", 0);
			}
		}
		else // try to make own node the closest node
		{
			QVariantMap Request;
			QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "MakeCloseTarget", Request).toMap();
			if(Response["Result"] == "ok")
			{
				int Distance = Response["Distance"].toInt();
				if ((LastDistance > Distance) // we must change target or else we wotn be closest to our selvs
				 || (LastDistance < Distance/2) // we should change the target to be as inconspicuous as possible
				 )
				{
					TargetID = Response["TargetID"].toByteArray();
					theCore->Cfg()->SetSetting("NeoKad/TargetID", QString(TargetID.toHex()));
					theCore->Cfg()->SetSetting("NeoKad/LastDistance", Distance);
				}
			}
		}

		if(!TargetID.isEmpty())
		{
			CPrivateKey* pEntityKey = NULL;
			QByteArray EntityKey = theCore->Cfg()->GetBlob("NeoKad/EntityKey");
			if(!EntityKey.isEmpty())
			{
				pEntityKey = new CPrivateKey;
				if(!pEntityKey->SetKey(EntityKey))
				{
					delete pEntityKey;
					pEntityKey = NULL;
				}
			}
			CNeoRoute* pRoute = SetupRoute(TargetID, pEntityKey, true);
			if(pRoute && !pEntityKey)
			{
				theCore->Cfg()->SetBlob("NeoKad/EntityKey", pRoute->GetEntityKey()->ToByteArray());
#ifdef _DEBUG
				theCore->Cfg()->setValue("NeoKad/EntityID", QString(pRoute->GetEntityID().toHex()));
#endif
			}
		}
	}

	foreach(CNeoClient* pClient, m_Clients)
	{
		pClient->Process(Tick);
	}

	// Incoming connections
	foreach(CNeoClient* pClient, m_Pending)
	{
		if(pClient->IsDisconnected())
		{
			RemoveConnection(pClient);
			delete pClient;
		}
	}

	// process metadata collecting
	foreach(uint64 FileID, m_MetadataExchange.keys())
	{
		CFile* pFile = CFileList::GetFile(FileID);
		if(!pFile || pFile->GetInspector()->GetIndexSource() == HashXNeo || !pFile->GetHash(HashXNeo))
			delete m_MetadataExchange.take(FileID);
		else
			TryInstallMetadata(pFile);
	}
}

void CNeoManager::Process()
{
	if(!m_Kademlia->IsEnabled())
		return;

	foreach(CNeoRoute* pRoute, m_Routes)
		pRoute->Process();
}

bool CNeoManager::IsItMe(const SNeoEntity& Neo)
{
	return m_Routes.contains(Neo.EntityID);
}

QByteArray CNeoManager::GetRouteTarget(const QByteArray& MyEntityID)
{
	if(CNeoRoute* pRoute = m_Routes.value(MyEntityID))
		return pRoute->GetTargetID();
	return "";
}

void CNeoManager::RouteBroken(const QByteArray& MyEntityID)
{
	foreach(CNeoClient* pClient, m_Clients)
	{
		if(pClient->GetNeo().MyEntityID == MyEntityID)
		{
			if(!pClient->IsDisconnected())
				pClient->Disconnect();
		}
	}
}

void CNeoManager::OnRouteBroken()
{
	CNeoRoute* pServer = qobject_cast<CNeoRoute*>(sender());
	RouteBroken(pServer->GetEntityID());
}

CNeoSession* CNeoManager::OpenSession(SNeoEntity& Neo)
{
	CNeoRoute* pRoute = m_Routes.value(Neo.MyEntityID);
	if(!pRoute)
	{
		if(Neo.IsHub || m_Routes.isEmpty())
			pRoute = SetupRoute(Neo.TargetID);
		else
			pRoute = m_Routes.values().at(qrand()%m_Routes.count());
		
		if(!pRoute)
			return NULL;
		Neo.MyEntityID = pRoute->GetEntityID();
	}

	QVariantMap Request;
	Request["MyEntityID"] = Neo.MyEntityID;
	Request["EntityID"] = Neo.EntityID;
	Request["TargetID"] = Neo.TargetID;

	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "OpenSession", Request).toMap();
	if(Response["Result"] != "ok")
		return NULL;

	return pRoute->NewSession(Neo.EntityID, Neo.TargetID, Response["SessionID"].toByteArray());
}

void CNeoManager::CloseSession(const SNeoEntity& Neo, CNeoSession* pSession)
{
	if(CNeoRoute* pRoute = m_Routes.value(Neo.MyEntityID))
		pRoute->DelSession(pSession);
	else
		pSession->Dispose();
}

CNeoRoute* CNeoManager::SetupRoute(const QByteArray& TargetID, CPrivateKey* pEntityKey, bool bStatic)
{
	if(pEntityKey)
	{
		foreach(CNeoRoute* pRoute, m_Routes)
		{
			if(pRoute->GetEntityKey()->CompareTo(pEntityKey))
			{
				delete pEntityKey;
				pEntityKey = NULL;
				if(TargetID == pRoute->GetTargetID())
				{
					LogLine(LOG_WARNING | LOG_DEBUG, tr("atempted to add an already known route"));
					return pRoute;
				}
				else
				{
					LogLine(LOG_ERROR | LOG_DEBUG, tr("atempted to add a route with an already used EntityKey for a different target ID"));
					ASSERT(0); // thats bad very bad!
					return NULL;
				}
			}
		}
	}

	QVariantMap Request = m_Kademlia->GetLookupCfg(CNeoKad::eRoute);
	Request["TargetID"] = TargetID;
	if(pEntityKey)
		Request["EntityKey"] = pEntityKey->ToByteArray();
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "SetupRoute", Request).toMap();

	QByteArray MyEntityID = Response["MyEntityID"].toByteArray();
	if(MyEntityID.isEmpty())
	{
		LogLine(LOG_ERROR | LOG_DEBUG, tr("Failed to establich kad route!"));
		delete pEntityKey;
		return NULL;
	}
	else
	{
		QByteArray TargetIDArr = TargetID;
		std::reverse(TargetIDArr.begin(), TargetIDArr.end());
		LogLine(LOG_INFO | LOG_DEBUG, tr("Establich kad route: %1@%2").arg(QString::fromLatin1(MyEntityID.toHex())).arg(QString::fromLatin1(TargetIDArr.toHex()).toUpper()));
	}
		
	if(Response.contains("EntityKey"))
	{
		ASSERT(pEntityKey == NULL);
		pEntityKey = new CPrivateKey;
		pEntityKey->SetKey(Response["EntityKey"].toByteArray());
	}
	ASSERT(pEntityKey != NULL);

	CNeoRoute* pRoute = new CNeoRoute(MyEntityID, TargetID, pEntityKey, bStatic, this);
	m_Routes.insert(MyEntityID, pRoute);

	connect(pRoute, SIGNAL(Connection(CNeoSession*)), this, SLOT(OnConnection(CNeoSession*)));
	connect(pRoute, SIGNAL(RouteBroken()), this, SLOT(OnRouteBroken()));

	return pRoute;
}

void CNeoManager::BreakRoute(const QByteArray& TargetID, const QByteArray& MyEntityID)
{
	RouteBroken(MyEntityID);
	if(CNeoRoute* pRoute = m_Routes.take(MyEntityID))
	{
		QVariantMap Request;
		Request["MyEntityID"] = MyEntityID;
		QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "BreakRoute", Request).toMap();

		QByteArray TargetIDArr = TargetID;
		std::reverse(TargetIDArr.begin(), TargetIDArr.end());
		LogLine(LOG_INFO | LOG_DEBUG, tr("Broken kad route: %1@%2").arg(QString::fromLatin1(MyEntityID.toHex())).arg(QString::fromLatin1(TargetIDArr.toHex()).toUpper()));

		delete pRoute;
	}
}

void CNeoManager::OnConnection(CNeoSession* pSession)
{
	CNeoClient* pClient = new CNeoClient(pSession, this);
	AddConnection(pClient, true);
}

void CNeoManager::AddConnection(CNeoClient* pClient, bool bPending)
{
	m_Clients.append(pClient); 
	if(bPending) 
		m_Pending.append(pClient);
	else{
		ASSERT(pClient->GetNeoEntity() != NULL);
	}
}

void CNeoManager::RemoveConnection(CNeoClient* pClient)	
{
	m_Clients.removeOne(pClient); 
	m_Pending.removeOne(pClient);
}

void CNeoManager::AddToFile(CFile* pFile, CFileHash* pHash, const SNeoEntity& Neo, EFoundBy FoundBy)
{
	bool bMulti = pFile->IsMultiFile();
	if(!pFile->IsStarted())
		return;

	if(IsItMe(Neo))
		return; // dont connect to ourselves

    foreach (CTransfer* pTransfer, pFile->GetTransfers())
	{
		if(CNeoEntity* pEntity = qobject_cast<CNeoEntity*>(pTransfer))
		{
			if(pEntity->GetNeo().EntityID == Neo.EntityID)
				return; // we already know this source
		}
	}

	CFileHashPtr pFleHash = pFile->GetHashPtrEx(pHash->GetType(), pHash->GetHash());
	ASSERT(pFleHash.data());
	CNeoEntity* pNewEntity = new CNeoEntity(pFleHash, Neo);
	pNewEntity->SetFoundBy(FoundBy);
	pFile->AddTransfer(pNewEntity);
}

bool CNeoManager::DispatchClient(CNeoClient* pClient, CFileHash* pHash)
{
	if(!m_Clients.contains(pClient))
		return false; // this one has ben

	CFile* pFile = theCore->m_FileManager->GetFileByHash(pHash);
	if(!pFile || !pFile->IsStarted())
		return false;
	
	if(IsItMe(pClient->GetNeo()))
		return false; // dont "connect" to ourselves

	CNeoEntity* pFoundEntity = NULL;
    foreach (CTransfer* pTransfer, pFile->GetTransfers())
	{
		if(CNeoEntity* pEntity = qobject_cast<CNeoEntity*>(pTransfer))
		{
			if(pClient->GetNeo().EntityID == pEntity->GetNeo().EntityID)
			{
				pFoundEntity = pEntity;
				break;
			}
		}
	}

	if(pFoundEntity && pFoundEntity->IsConnected()) // this checks only if there is a client
	{
//#ifdef _DEBUG
//		LogLine(LOG_DEBUG, tr("A Neo Entity connected to us while we ware still connected to it"));
//#endif
		if(pFoundEntity->GetClient()->IsConnected())
			return false; // if we really are connected deny the incomming connection
		pFoundEntity->DetacheClient();
	}

	if(!pFoundEntity)
	{
		CFileHashPtr pFleHash = pFile->GetHashPtrEx(pHash->GetType(), pHash->GetHash());
		ASSERT(pFleHash.data());
		pFoundEntity = new CNeoEntity(pFleHash);
		pFoundEntity->SetFoundBy(eSelf);
		pFile->AddTransfer(pFoundEntity);
	}
	m_Pending.removeOne(pClient);

	pFoundEntity->AttacheClient(pClient);
	return true;
}

QList<CNeoRoute*> CNeoManager::GetStaticRoutes()
{
	QList<CNeoRoute*> Routes;
	foreach(CNeoRoute* pRoute, m_Routes)
	{
		if(pRoute->IsStatic())
			Routes.append(pRoute);
	}
	return Routes;
}

void CNeoManager::OnBytesWritten(qint64 Bytes)
{
	m_TransferStats.AddUpload(Bytes);
	theCore->m_Network->OnBytesWritten(Bytes);
}

void CNeoManager::OnBytesReceived(qint64 Bytes)
{
	m_TransferStats.AddDownload(Bytes);
	theCore->m_Network->OnBytesReceived(Bytes);
}

void CNeoManager::InstallMetaData(CFile* pFile, uint64 uFileSize, const QList<SSubFile>& SubFiles)
{
	CFileHashPtr pHash = pFile->GetHashPtr(HashXNeo);
	CHashInspector* pInspector = pFile->GetInspector();
	if(!pInspector || pHash.isNull())
		return;

	if(pInspector->GetIndexSource() == HashXNeo)
		return; // we already have it

	CPartMap* pPartMap = pFile->GetPartMap();
	CJoinedPartMap* pParts = pPartMap ? qobject_cast<CJoinedPartMap*>(pPartMap) : NULL;
	
	bool bOpenIO = false;
	if((pPartMap == NULL && !pFile->IsComplete()) || (pParts && pParts->GetLinks().isEmpty()))
		bOpenIO = true;

	if(pPartMap && !(pParts && CompareSubFiles(pParts, SubFiles))) // pPartMap && !pParts means file is currently a single file
	{
		if(!pInspector->BadMetaData(pHash))
			return; // this aint the master hash 

		pParts = NULL;
	}

	pInspector->SetIndexSource(HashXNeo);

	CFileList* pList = pFile->GetList();

	if(pParts) // this case means we already have metadata and we passed the comparation
	{
		QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();
		ASSERT(SubFiles.count() == Links.count());
		int Counter = 0;
		for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin(); Counter++)
		{
			SPartMapLink* pLink = *(--I);
			const SSubFile& File = SubFiles[Counter];
			if(CFile* pSubFile = pList->GetFileByID(pLink->ID))
				AddAuxHashes(pSubFile, File.FileHashes);
		}
		return;
	}
	
	
	if(pFile->GetFileSize() != uFileSize)
		pFile->SetFileSize(uFileSize);

	pParts = new CJoinedPartMap(pFile->GetFileSize());

	CFileHashPtr pMasterHash = pFile->GetMasterHash();
	ASSERT(pMasterHash);

	uint64 Offset = 0;
	for(int i=0; i < SubFiles.count(); i++)
	{
		SSubFile File = SubFiles[i];

		QString Dir = pFile->GetFileDir();
		Dir += pFile->GetFileName() + "/";
		if(!File.FileDirectory.isEmpty())
			Dir += File.FileDirectory + "/";

		CFile* pSubFile = NULL;

		QList<CFile*> Files = pList->GetFilesByHash(File.FileHashes, true);
		while(!Files.isEmpty())
		{
			CFile* pFoundFile = Files.takeFirst();

			// this is a patologic case, multi file contains duplicates
			if(pFoundFile->GetParentFiles().contains(pFile->GetFileID()))
				continue;

			if(pSubFile->IsMultiFile())
			{
				ASSERT(0);
				continue;
			}
			if(pFoundFile->IsRemoved())
			{
				pFoundFile->SetFileDir(Dir);
				pFoundFile->UnRemove(pMasterHash);
				if(!pFoundFile->IsPending())
					pFoundFile->Resume();
			}

			pSubFile = pFoundFile;
			break;
		}

		if(!pSubFile)
		{
			pSubFile = new CFile();
			if(pFile->GetProperty("Temp").toBool())
				pSubFile->SetProperty("Temp", true);
			pSubFile->SetFileDir(Dir);

			foreach(CFileHashPtr pHash, File.FileHashes)
				pSubFile->AddHash(pHash);
		
			pSubFile->AddEmpty(pMasterHash->GetType(), File.FileName, File.FileSize, pFile->IsPending()); // FileName // FileSize

			pList->AddFile(pSubFile);

			if(!pSubFile->IsPending())
				pSubFile->Resume();
		}
		else
			AddAuxHashes(pSubFile, File.FileHashes);

		uint64 uBegin = Offset; 
		uint64 uEnd = Offset + File.FileSize;
		Offset += File.FileSize;

		CSharedPartMap* pSubParts = pSubFile->SharePartMap(); 
		ASSERT(pSubParts); // this must not fail - we did all checks above

		pParts->SetupLink(uBegin, uEnd, pSubFile->GetFileID());
		pSubParts->SetupLink(uBegin, uEnd, pFile->GetFileID());

		if(pFile->IsPaused(true))
			pSubFile->Pause();
		else if(pFile->IsStarted())
			pSubFile->Start();
	}

	pFile->SetPartMap(CPartMapPtr(pParts));

	// Note: if neo is the masterhash the IO is already opened
	if(bOpenIO && !pFile->IsPending())
		pFile->Resume();
}

bool CNeoManager::CompareSubFiles(CJoinedPartMap* pParts, const QList<SSubFile>& SubFiles)
{
	QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();
	if(SubFiles.count() != Links.count())
		return false; // wrong sub file count

	uint64 Offset = 0;
	int Counter = 0;
	for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin(); Counter++)
	{
		SPartMapLink* pLink = *(--I);
		SSubFile File = SubFiles[Counter];

		uint64 uBegin = Offset; 
		uint64 uEnd = Offset + File.FileSize;
		Offset += File.FileSize;
		if(pLink->uShareBegin != uBegin || pLink->uShareEnd != uEnd)
			return false; // wrong sub file size
	}
	return true;
}

void CNeoManager::AddAuxHashes(CFile* pFile, const QList<CFileHashPtr>& FileHashes)
{
	if(!pFile->IsIncomplete())
		return;

	CHashInspector* pInspector = pFile->GetInspector();
	ASSERT(pInspector);

	foreach(CFileHashPtr pHash, FileHashes)
	{
		if(pHash->GetTypeClass()  == (pFile->IsMultiFile() ? HashSingle : HashMulti)) // check if the hash type class is correct
			continue; // this should not happen unless we get fucked up results
		pInspector->AddAuxHash(pHash);
	}

	if(pFile->GetPartMap() == NULL)
	{
		// Note: if Hash inspector does not set the xhash imminetly we wil fuckup here badly!
		if(pFile->GetHash(HashXNeo) == NULL && pFile->GetFileSize()) // Note: Multi file part maps are being set once we have the metadata not befoure
			pFile->SetPartMap(CPartMapPtr(new CSynced<CPartMap>(pFile->GetFileSize())));
	}
}

QVariantMap CNeoManager::WriteHashMap(const QList<CFileHashPtr>& Hashes)
{
	QMultiMap<EFileHashType, CFileHashPtr> AllHashes;
	foreach(CFileHashPtr pHash, Hashes)
	{
		if(!pHash->IsValid()) // if its all 0000... ignore it
			continue;

		AllHashes.insert(pHash->GetType(), pHash);
	}
	QVariantMap HashMap;
	foreach(EFileHashType HashType, AllHashes.uniqueKeys())
	{
		QList<CFileHashPtr> CurHashes = AllHashes.values(HashType);
		if(CurHashes.size() > 1) // new format allowing multiple entries
		{
			QVariantList Hashes;
			foreach(CFileHashPtr pHash, CurHashes)
				Hashes.append(pHash->GetHash());
			HashMap[CFileHash::HashType2Str(HashType)] = Hashes;
		}
		else if(CurHashes.size() == 1)
			HashMap[CFileHash::HashType2Str(HashType)] = CurHashes.first()->GetHash();
	}
	return HashMap;
}

QList<CFileHashPtr> CNeoManager::ReadHashMap(const QVariantMap& HashMap, uint64 uFileSize)
{
	QList<CFileHashPtr> Hashes;
	foreach(const QString& sType, HashMap.uniqueKeys())
	{
		EFileHashType Type = CFileHash::Str2HashType(sType);
		const QVariant& vEntry = HashMap[sType];
		if(vEntry.canConvert(QVariant::List)) // new format allowing multiple entries
		{
			foreach(const QVariant& vSubEntry, vEntry.toList())
			{
				if(CFileHashPtr pHash = CFileHashPtr(CFileHash::FromArray(vSubEntry.toByteArray(), Type, uFileSize)))
					Hashes.append(pHash);
			}
		}
		else if(CFileHashPtr pHash = CFileHashPtr(CFileHash::FromArray(vEntry.toByteArray(), Type, uFileSize)))
			Hashes.append(pHash);
	}
	return Hashes;
}

QVariant CNeoManager::GetMetadataEntry(CFile* pFile, int Index)
{
	CFile* pSubFile = (Index >= 0 && Index < pFile->GetSubFiles().count()) ? pFile->GetList()->GetFileByID(pFile->GetSubFiles().at(Index)) : NULL;
	if(!pSubFile || !pSubFile->GetHash(HashNeo))
		return QVariantMap();

	QVariantMap File;
	//File["OF"] = Offset; // FileOffset
	File["FS"] = pSubFile->GetFileSize(); // FileSize

	File["FN"] = pSubFile->GetFileName(); // FileName
	QStringList Dir = pSubFile->GetFileDir().split("/",QString::SkipEmptyParts); // Parent/dir1/dir2/
	if(Dir.count() >= 1) // more than just parent
	{
		Dir.removeFirst(); // name of parent
		File["FD"] = Dir.join("/"); //dir1/dir2 // FileDirectory
	}
		
	File["HM"] = WriteHashMap(pSubFile->GetAllHashes(true));

	return File;
}

int CNeoManager::NextMetadataBlock(CFile* pFile, const QByteArray& ID)
{
	SMetadataExchange* &MetadataExchange = m_MetadataExchange[pFile->GetFileID()];

	if(!MetadataExchange)
		MetadataExchange = new SMetadataExchange;

	if(MetadataExchange->Metadata[ID].EntryCount == -1) // -1 means this one has rejected us
		return -1;

	uint64 EntryCount = MetadataExchange->Metadata[ID].EntryCount;
	if(!EntryCount) // if this client is asked for the first time get a majority consence for the expected size
	{
		QMap<uint64, int> Map;
		for(QMap<QByteArray, SMetadata>::iterator I = MetadataExchange->Metadata.begin(); I != MetadataExchange->Metadata.end(); I++)
		{
			if(I->EntryCount && I->EntryCount != -1)
				Map[I->EntryCount]++;
		}
		if(!Map.isEmpty())
		{
			QMap<uint64, int>::iterator I = (--Map.end()); // thats the entry with the highest count
			EntryCount = I.key();
		}
	}
	if(!EntryCount) EntryCount = 1;

	int BestCount = 0;
	int BestIndex = -1;
	for(int i=0; i < EntryCount; i++)
	{
		if(MetadataExchange->Metadata[ID].Entrys.contains(i))
			continue; // we have this one block from that one peer

		int CurCount = 0;
		for(QMap<QByteArray, SMetadata>::iterator I = MetadataExchange->Metadata.begin(); I != MetadataExchange->Metadata.end(); I++)
		{
			if(I->Entrys.contains(i))
				CurCount++;
		}
		if(BestIndex == -1 || CurCount < BestCount)
		{
			BestCount = CurCount;
			BestIndex = i;
		}
	}
	if(BestIndex != -1)
	{
		MetadataExchange->Metadata[ID].Entrys[BestIndex] = QVariantMap();
//#ifdef _DEBUG
//		qDebug() << "Meta Data Request block " << BestIndex << " out of " << EntryCount << " (" << MetadataSize << ")";
//#endif
	}
	return BestIndex;
}

void CNeoManager::AddMetadataBlock(CFile* pFile, int Index, const QVariant& Entry, int EntryCount, const QByteArray& ID)
{
	ASSERT(EntryCount);
	SMetadataExchange* MetadataExchange = m_MetadataExchange.value(pFile->GetFileID());
	if(!MetadataExchange)
		return;

//#ifdef _DEBUG
//		qDebug() << "Meta Data Got block " << Index;
//#endif

	if(!MetadataExchange->Metadata[ID].EntryCount)
		MetadataExchange->Metadata[ID].EntryCount = EntryCount;
		
	MetadataExchange->Metadata[ID].Entrys[Index] = Entry.toMap();

	MetadataExchange->NewData = true;
}

void CNeoManager::ResetMetadataBlock(CFile* pFile, int Index, const QByteArray& ID)
{
	SMetadataExchange* MetadataExchange = m_MetadataExchange.value(pFile->GetFileID());
	if(!MetadataExchange)
		return;

//#ifdef _DEBUG
//		qDebug() << "Meta Data denyed block " << Index;
//#endif

	MetadataExchange->Metadata[ID].EntryCount = -1; // dont ask this peer in future is rejected our request
	MetadataExchange->Metadata[ID].Entrys.clear();
}

QList<CNeoManager::SSubFile> CNeoManager::TryAssemblyMetadata(CFile* pFile)
{
	SMetadataExchange* MetadataExchange = m_MetadataExchange.value(pFile->GetFileID());
	ASSERT(MetadataExchange);

	CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(pFile->GetHash(HashXNeo));
	if(!pHashTreeEx || !pHashTreeEx->CanHashParts())
		return QList<SSubFile>(); // we need to wair until we have the hash complete 

	MetadataExchange->NewData = false;

	QVector<QByteArray> Peers = MetadataExchange->Metadata.keys().toVector();
	for(int i = 0; i < Peers.size(); i++)
	{
		const QByteArray& ID = Peers[i];
		SMetadata &Metadata = MetadataExchange->Metadata[ID];
		if(Metadata.EntryCount == 0 || Metadata.EntryCount == -1)
			continue;

		QVariantList Entrys;

		for(int Index = 0; Index < Metadata.EntryCount; Index++)
		{
			if(Metadata.Entrys.contains(Index) && !Metadata.Entrys[Index].isEmpty())
				Entrys.append(Metadata.Entrys[Index]);
			else
			{
				int j = 1;
				for(; j < Peers.size(); j++)
				{
					int k = (i + j) % Peers.size();
					if(MetadataExchange->Metadata[Peers[k]].Entrys.contains(Index) && !MetadataExchange->Metadata[Peers[k]].Entrys[Index].isEmpty())
					{
						Entrys.append(MetadataExchange->Metadata[Peers[k]].Entrys[Index]);
						break;
					}
				}
				if(j >= Peers.size()) // we need this block
					return QList<SSubFile>();
			}
		}

		QList<SSubFile> SubFiles;

		uint64 uTotalSize = 0;
		CBuffer MetaData;
		foreach(const QVariant vFile, Entrys)
		{
			QVariantMap File = vFile.toMap();

			SSubFile SubFile;
			SubFile.FileName = File["FN"].toString(); // FileName
			SubFile.FileSize = File["FS"].toULongLong(); // FileSize
			SubFile.FileDirectory = File["FD"].toString(); // FileDirectory
			SubFile.FileHashes = ReadHashMap(File["HM"].toMap(), SubFile.FileSize); // HashMap

			SubFiles.append(SubFile);

			uTotalSize += SubFile.FileSize;

			QByteArray FileHash;
			foreach(const CFileHashPtr& pHash, SubFile.FileHashes)
			{
				if(pHash->GetType() == HashNeo)
				{
					FileHash = pHash->GetHash();
					break;
				}
			}
			MetaData.WriteValue<uint64>(SubFile.FileSize);
			MetaData.WriteQData(FileHash);
		}
		if(pHashTreeEx->HashMetaData(MetaData.ToByteArray()) != pHashTreeEx->GetMetaHash())
		{
			LogLine(LOG_ERROR | LOG_DEBUG, tr("Recived invalid Neo Meta Data (ex)"));
			continue;
		}
		if(pFile->GetFileSize() != uTotalSize)
		{
			LogLine(LOG_ERROR | LOG_DEBUG, tr("Recived inconsistent Neo Meta Data (ex)"));
			continue;
		}
		
		return SubFiles;
	}
	return QList<SSubFile>();
}

bool CNeoManager::TryInstallMetadata(CFile* pFile)
{
	SMetadataExchange* MetadataExchange = m_MetadataExchange.value(pFile->GetFileID());
	if(MetadataExchange && MetadataExchange->NewData)
	{
		QList<SSubFile> SubFiles = TryAssemblyMetadata(pFile);
		if(!SubFiles.isEmpty())
		{
			InstallMetaData(pFile, pFile->GetFileSize(), SubFiles);

			delete MetadataExchange;
			m_MetadataExchange.remove(pFile->GetFileID());
			return true;
		}
	}
	return true;
}