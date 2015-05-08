#include "GlobalHeader.h"
#include "KadPublisher.h"
#include "../../../NeoCore.h"
#include "../../../FileTransfer/NeoShare/NeoKad.h"
#include "../../../FileTransfer/NeoShare/NeoManager.h"
#include "../../../Interface/InterfaceManager.h"
#include "KeywordPublisher.h"
#include "HashPublisher.h"
#include "RatingPublisher.h"
#include "SourcePublisher.h"
#ifndef NO_HOSTERS
#include "LinkPublisher.h"
#endif
#include "../../../../Framework/Cryptography/HashFunction.h"
#include "../../../../Framework/OtherFunctions.h"
#include "../../../FileList/File.h"
#include "../../../FileList/FileDetails.h"
#include "../../../FileList/FileStats.h"
#include "../../../FileList/FileManager.h"
#ifndef NO_HOSTERS
#include "../../../FileTransfer/HosterTransfer/HosterLink.h"
#include "../../../FileTransfer/HosterTransfer/WebManager.h"
#endif
#include "../../../FileTransfer/BitTorrent/TorrentManager.h"
#include "../../../FileTransfer/BitTorrent/Torrent.h"
#include "../../../FileTransfer/BitTorrent/TorrentInfo.h"
#include "../../../FileTransfer/ed2kMule/MuleManager.h"
#include "../../../FileTransfer/NeoShare/NeoEntity.h"
#include "../../../FileTransfer/BitTorrent/TorrentPeer.h"
#include "../../../FileTransfer/ed2kMule/MuleSource.h"
#include "../../../../Framework/Archive/Archive.h"

CKadAbstract::CKadAbstract(QObject* qObject)
: QObjectEx(qObject) 
{
	m_KeywordPublisher = new CKeywordPublisher(this);
	m_HashPublisher = new CHashPublisher(this);
	m_RatingPublisher = new CRatingPublisher(this);
	m_SourcePublisher = new CSourcePublisher(this);
#ifndef NO_HOSTERS
	m_LinkPublisher = new CLinkPublisher(this);
#endif
}

CKadAbstract::~CKadAbstract() 
{
	qDeleteAll(m_Files);
	qDeleteAll(m_Keywords);
}

void CKadAbstract::Process(UINT Tick)
{
	m_KeywordPublisher->Process(Tick);
	m_HashPublisher->Process(Tick);
	m_RatingPublisher->Process(Tick);
	m_SourcePublisher->Process(Tick);
#ifndef NO_HOSTERS
	m_LinkPublisher->Process(Tick);
#endif
}

void CKadAbstract::AddFile(SKadFile* pFile)
{
	ASSERT(!m_Files.contains(pFile->FileID));
	m_Files.insert(pFile->FileID, pFile);

	ASSERT(pFile->Keywords.isEmpty());
	QStringList Keywords = GetFileInfo(pFile->FileID).FileName.toLower().split(g_kwrdSpliter, QString::SkipEmptyParts);
	foreach(const QString& Keyword, Keywords)
	{
		if(pFile->Keywords.contains(Keyword))
			continue;
		pFile->Keywords.insert(Keyword, GetKwrdInitStats(pFile->FileID));
		SKadKeyword*& pKeyword = m_Keywords[Keyword];
		if(!pKeyword)
			pKeyword = new SKadKeyword(Keyword);
		ASSERT(!pKeyword->Files.contains(pFile));
		pKeyword->Files.append(pFile);
	}
}

void CKadAbstract::RemoveFile(uint64 FileID)
{
	SKadFile* pFile = m_Files.take(FileID);
	ASSERT(pFile);

	foreach(const QString& Keyword, pFile->Keywords.keys())
	{
		SKadKeyword* pKeyword = m_Keywords[Keyword];
		ASSERT(pKeyword && pKeyword->Files.contains(pFile));
		pKeyword->Files.removeOne(pFile);
		if(pKeyword->Files.isEmpty())
			delete m_Keywords.take(Keyword);
	}
	pFile->Keywords.clear();
}

int CKadAbstract::MaxLookups()
{
	return theCore->Cfg()->GetInt("NeoShare/KadPublishmentVolume");
}

QByteArray CKadAbstract::StartLookup(const QVariant& Request)
{
	ASSERT(Request.isValid());
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "StartLookup", Request).toMap();
	QByteArray LookupID = Response["LookupID"].toByteArray();
	if(LookupID.isEmpty())
		LogLine(LOG_ERROR | LOG_DEBUG, tr("Failed to start lookup, error: %1").arg(Response["Error"].toString()));	
	return LookupID;
}

void CKadAbstract::StopLookup(const QByteArray& LookupID)
{
	QVariantMap Request;
	Request["LookupID"] = LookupID;
	theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "StopLookup", Request).toMap();
}

bool CKadAbstract::HandleLookup(const QByteArray& LookupID, time_t& ExpirationTime, int& StoreCount)
{
	ExpirationTime = 0;
	StoreCount = 0;

	QVariantMap Request;
	Request["LookupID"] = LookupID;
	Request["AutoStop"] = true; // stop if the lookup is finished
	QVariantMap Response = theCore->m_Interfaces->RemoteProcedureCall("NeoKad", "QueryLookup", Request).toMap();

	//QVariantMap LookupInfo = Response["Info"].toMap();

	foreach(const QVariant& vResult, Response["Results"].toList())
	{
		QVariantMap Result = vResult.toMap();
		//Result["ID"]
		QVariantMap Return = Result["Return"].toMap();

		// Result["Function"].toString()
		// Note: assotiated entries are currently all published with the same expiration time and should be threaded equaly by the remote node
		time_t CurExpirationTime = Return["Expiration"].toULongLong();
		int CurStoreCount = Return["StoreCount"].toUInt();
		if(ExpirationTime == 0 || ExpirationTime > CurExpirationTime)
			ExpirationTime = CurExpirationTime;
		if(StoreCount == 0 || StoreCount > CurStoreCount)
			StoreCount = CurStoreCount;
	}

	return Response["Staus"] == "Finished";
}

uint64 CKadAbstract::MkPubID(uint64 FileID)
{
	// The pub ID should always be the same for a given client publishing a given file, other than that it should be random

	QByteArray Temp = theCore->m_NeoManager->GetKad()->GetStoreKey();
	Temp += QByteArray::number(FileID);
	QByteArray Hash = CHashFunction::Hash(Temp, CHashFunction::eSHA256);
	uint64 PubID;
	CAbstractKey::Fold((byte*)Hash.data(), Hash.size(), (byte*)(&PubID), KEY_64BIT);
	return PubID;	
}

QByteArray CKadAbstract::MkTargetID(const QByteArray& Hash)
{
	QByteArray TargetID;
	TargetID.resize(KEY_128BIT);
	CAbstractKey::Fold((byte*)Hash.data(), Hash.size(), (byte*)TargetID.data(), KEY_128BIT);
	return TargetID;
}

QString CKadAbstract::MkTopFileKeyword()
{
	int Year; 
	int Week = QDate::currentDate().weekNumber(&Year);
	return QString("topfiles%1cw%2").arg(Year).arg(Week);
}

QVariantMap CKadAbstract::GetHashMap(uint64 ID)
{
	// Note: in the new design a hash map entry can eider be a single item, or a list of items
	QVariantMap HashMap;
	QMultiMap<EFileHashType, CFileHashPtr> AllHashes = GetHashes(ID);
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

QMultiMap<EFileHashType, CFileHashPtr> CKadAbstract::ReadHashMap(const QVariantMap& HashMap, uint64 uFileSize)
{
	QMultiMap<EFileHashType, CFileHashPtr> Hashes;
	foreach(const QString& sType, HashMap.uniqueKeys())
	{
		EFileHashType Type = CFileHash::Str2HashType(sType);
		const QVariant& vEntry = HashMap[sType];
		if(vEntry.canConvert(QVariant::List)) // new format allowing multiple entries
		{
			foreach(const QVariant& vSubEntry, vEntry.toList())
			{
				if(CFileHashPtr pHash = CFileHashPtr(CFileHash::FromArray(vSubEntry.toByteArray(), Type, uFileSize)))
					Hashes.insert(Type, pHash);
			}
		}
		else if(CFileHashPtr pHash = CFileHashPtr(CFileHash::FromArray(vEntry.toByteArray(), Type, uFileSize)))
			Hashes.insert(Type, pHash);
	}
	return Hashes;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//

CKadPublisher::CKadPublisher(QObject* qObject)
 : CKadAbstract(qObject)
{
	m_LastTopPublishment = 0;
}

void CKadPublisher::Process(UINT Tick)
{
	QMap<uint64, QPointer<CFile> > FileList = m_FileList;
	foreach(CFile* pFile, theCore->m_FileManager->GetFiles())
	{
		if(pFile->IsRemoved() || pFile->MetaDataMissing())
			continue;

		// do not publish private torrents to NeoKad
		if(CTorrent* pTorrent = pFile->GetTopTorrent())
		{
			if(pTorrent->GetInfo()->IsPrivate())
				continue;
		}

		uint64 FileID = pFile->GetFileID();
		CFile* pTest = FileList.take(FileID);
		ASSERT(!pTest || pTest == pFile);
		if(!pTest)
		{
			m_FileList.insert(FileID, pFile); // Cache files for fast acces, dont use CFileList::GetFile(FileID) for each proeprty
			AddFile(new SKadFile(FileID));
		}
	}
	foreach(uint64 FileID, FileList.keys())
	{
		m_FileList.remove(FileID);
		RemoveFile(FileID);
	}

	CKadAbstract::Process(Tick);


	if(!m_TopLookupID.isEmpty())
	{
		time_t ExpirationTime = 0;
		int StoreCount = 0;
		if(HandleLookup(m_TopLookupID, ExpirationTime, StoreCount))
		{
			m_TopLookupID.clear();
			m_LastTopPublishment = GetTime();
		}
	}
	else if(!m_LastTopPublishment || GetTime() - m_LastTopPublishment > DAY2S(1))
	{
		QList<uint64> Files; // Chose what files to put into our dayly topfile announcement
		foreach(CFile* pFile, theCore->m_FileManager->GetFiles())
		{
			if(pFile->IsRemoved() || pFile->IsPending() || pFile->MetaDataMissing() || pFile->IsDuplicate())
				continue;

			if(qobject_cast<CSharedPartMap*>(pFile->GetPartMap()) != NULL)
				continue; // do not put subfiles into topfile list
#ifndef NO_HOSTERS
			if(pFile->GetStats()->HasHosters())
				Files.append(pFile->GetFileID());
#endif
		}
		QVariant Request = m_KeywordPublisher->PublishEntrys(MkTopFileKeyword(), Files);
		if(Request.isValid()) // is there anythign to do?
			m_TopLookupID = StartLookup(Request);
	}
}

void CKadPublisher::ResetFile(CFile* pFile)
{
	pFile->SetProperty("KeywordExpiration", QVariant());

	pFile->SetProperty("HasheExpiration", QVariant());

	pFile->SetProperty("RatingExpiration", QVariant());

	pFile->SetProperty("EntityExpiration", QVariant());
	pFile->SetProperty("SourceExpiration", QVariant());
	pFile->SetProperty("PeerExpiration", QVariant());

	pFile->SetProperty("LinkExpiration", QVariant());

	if(m_FileList.remove(pFile->GetFileID()))
		RemoveFile(pFile->GetFileID());
	// Will be readed and reevaluated in process
}

CFileHash* CKadPublisher::SelectTopHash(CFile* pFile)
{
	CTorrent* pTorrent = pFile->GetTopTorrent();

	// do not publish private torrents to NeoKad
	if(pTorrent && pTorrent->GetInfo()->IsPrivate())
		return NULL;

	CFileHashPtr pFileHash = pFile->GetMasterHash();
	if(pFileHash && !pFile->GetHashPtrEx(pFileHash->GetType(), pFileHash->GetHash())) // this means the masterhash belongs to a parent file
	{
		EFileHashType HashPrio[3] = {HashNeo, HashXNeo, HashEd2k};
		for(int i=0; i<ARRSIZE(HashPrio); i++)
		{
			if(CFileHashPtr pHash = pFile->GetHashPtr(HashPrio[i]))
				return pHash.data();
		}

		if(pTorrent)
			return pTorrent->GetHash().data();

		foreach(CFileHashPtr pHash, pFile->GetHashes(HashArchive)) // shorter than a proper if :p
			return pHash.data();

		return NULL;
	}
	return pFileHash.data();
}

bool CKadPublisher::FindHash(CFile* pFile, bool bForce)
{
	if(!bForce && m_HashPublisher->GetSearchCount() >= theCore->Cfg()->GetInt("NeoShare/KadLookupVolume"))
		return false;
	if(CFileHash* pHash = pFile->GetHash(pFile->IsMultiFile() ? HashXNeo : HashNeo))
	{
		LogLine(LOG_DEBUG | LOG_INFO, tr("Searching for Hash Set for %1").arg(pFile->GetFileName()));
		return m_HashPublisher->Find(pFile->GetFileID(), pHash, "findHash", pFile->GetFileName());
	}
	return false;
}

bool CKadPublisher::FindIndex(CFile* pFile, bool bForce)
{
	ASSERT(pFile->IsIncomplete());
	if(!bForce && m_HashPublisher->GetSearchCount() >= theCore->Cfg()->GetInt("NeoShare/KadLookupVolume"))
		return false;
	if(CFileHash* pHash = pFile->GetHash(pFile->IsMultiFile() ? HashXNeo : HashNeo))
	{
		LogLine(LOG_DEBUG | LOG_INFO, tr("Searching for Index for %1").arg(pFile->GetFileName()));
		return m_HashPublisher->Find(pFile->GetFileID(), pHash, "findIndex", pFile->GetFileName());
	}
	return false;
}

bool CKadPublisher::FindAliases(CFile* pFile, bool bForce)
{
	//if(pFile->MetaDataMissing())
	//	return false;

	if(pFile->GetHashMap().isEmpty())
	{
		if(CTorrent* pTorrent = pFile->GetTopTorrent())
		{
			time_t uNow = GetTime();
			time_t uStart = pFile->GetProperty("StartTime").toDateTime().toTime_t();
			if(uNow < uStart + MIN2S(10) && pTorrent->GetInfo()->IsEmpty())
				return false;
		}
	}

	if(!bForce && m_HashPublisher->GetSearchCount() >= theCore->Cfg()->GetInt("NeoShare/KadLookupVolume"))
		return false;

	if(CFileHash* pHash = SelectTopHash(pFile))
	{
		LogLine(LOG_DEBUG | LOG_INFO, tr("Searching for Other Hashes for %1").arg(pFile->GetFileName()));
		return m_HashPublisher->Find(pFile->GetFileID(), pHash, "findAliases", pFile->GetFileName());
	}
	return false;
}

bool CKadPublisher::FindSources(CFile* pFile, bool bForce)
{
#ifndef NO_HOSTERS
	if(!bForce && Max(m_SourcePublisher->GetSearchCount(), m_LinkPublisher->GetSearchCount()) >= theCore->Cfg()->GetInt("NeoShare/KadLookupVolume"))
#else
	if(!bForce && m_SourcePublisher->GetSearchCount() >= theCore->Cfg()->GetInt("NeoShare/KadLookupVolume"))
#endif
		return false;

	bool bMulti = pFile->IsMultiFile();

	if(pFile->IsNeoShared() && theCore->Cfg()->GetBool("NeoShare/Enable") 
	 && pFile->GetStats()->GetTransferCount(eNeoShare) < theCore->Cfg()->GetInt("NeoShare/MaxEntities"))
	{
		if(CFileHash* pHash = pFile->GetHash(bMulti ? HashXNeo : HashNeo))
			m_SourcePublisher->Find(pFile->GetFileID(), pHash, "collectSources", pFile->GetFileName());
	}

	if(theCore->Cfg()->GetBool("BitTorrent/Enable")
	 && pFile->GetStats()->GetTransferCount(eBitTorrent) < theCore->Cfg()->GetInt("BitTorrent/MaxPeers") * 10)
	{
		foreach(CTorrent* pTorrent, pFile->GetTorrents())
		{
			// do not publish private torrents to NeoKad
			if(!pTorrent || pTorrent->GetInfo()->IsPrivate())
				continue;
			m_SourcePublisher->Find(pFile->GetFileID(), pTorrent->GetHash().data(), "collectSources", pFile->GetFileName());
		}
	}

	if(pFile->IsIncomplete())
	{
		if(pFile->IsEd2kShared() && theCore->Cfg()->GetBool("Ed2kMule/Enable")
		 && pFile->GetStats()->GetTransferCount(eEd2kMule) < theCore->Cfg()->GetInt("Ed2kMule/MaxSources"))
		{
			if(CFileHash* pHash = pFile->GetHash(HashEd2k))
				m_SourcePublisher->Find(pFile->GetFileID(), pHash, "collectSources", pFile->GetFileName());
		}
		
#ifndef NO_HOSTERS
		if(pFile->IsHosterDl() && theCore->Cfg()->GetBool("Hoster/Enable"))
		{
			foreach(CFileHashPtr pHash, pFile->GetHashes(HashArchive))
				m_LinkPublisher->Find(pFile->GetFileID(), pHash.data(), "findLinks", pFile->GetFileName());
		}
#endif
	}

#ifndef NO_HOSTERS
	if(pFile->IsHosterDl() && theCore->Cfg()->GetBool("Hoster/Enable") 
	 && (pFile->IsIncomplete() || theCore->Cfg()->GetString("HosterCache/CacheMode") != "Off")) // C-ToDo-Now: retrive pub date from link lookup and save it
	{
		if(CFileHash* pHash = pFile->GetHash(bMulti ? HashXNeo : HashNeo))
			m_LinkPublisher->Find(pFile->GetFileID(), pHash, "findLinks", pFile->GetFileName());
	}
#endif

	return true;
}

bool CKadPublisher::FindRating(CFile* pFile, bool bForce)
{
	if(!bForce && m_RatingPublisher->GetSearchCount() >= theCore->Cfg()->GetInt("NeoShare/KadLookupVolume"))
		return false;

	if(CFileHash* pHash = pFile->GetHash(pFile->IsMultiFile() ? HashXNeo : HashNeo))
		return m_RatingPublisher->Find(pFile->GetFileID(), pHash, "findRatings", pFile->GetFileName());
	return false;
}

bool CKadPublisher::IsFindingRating(CFile* pFile)
{
	return m_RatingPublisher->IsFinding(pFile->GetFileID(), "findRatings");
}

// Interface

QPair<time_t, int> CKadPublisher::GetKwrdInitStats(uint64 FileID)
{
	if(CFile* pFile = m_FileList.value(FileID))
		return QPair<time_t, int>(pFile->GetProperty("KeywordExpiration").toULongLong(), pFile->GetProperty("KeywordStoreCount").toULongLong());
	return QPair<time_t, int>(0, 0);
}

QMultiMap<EFileHashType, CFileHashPtr> CKadPublisher::IsOutdated(uint64 FileID, CKadPublisher::EIndex eIndex)
{
	CFile* pFile = m_FileList.value(FileID);
	QMultiMap<EFileHashType, CFileHashPtr> Hashes;
	if(!pFile)
	{
		ASSERT(0);
		return Hashes;
	}

	QMultiMap<EFileHashType, CFileHashPtr> FileHashes = GetHashes(FileID);
	EFileHashType NType = pFile->IsMultiFile() ? HashXNeo : HashNeo;

	time_t uNow = GetTime();
	switch(eIndex)
	{
		case eKeyword:
		{
			ASSERT(0);
			break;
		}
		case eHashes:
		{
			time_t ExpirationTime = pFile->GetProperty("HasheExpiration").toULongLong();
			if(uNow > ExpirationTime || ExpirationTime - uNow < HR2S(1))  // K-ToDo-Now: customise
			{
				if(FileHashes.contains(pFile->IsMultiFile() ? HashXNeo : HashNeo))  // publish hashes only if we have a neo has
					Hashes = FileHashes;
				// Note: if there is nothing to do we wil cancel later
				/*bool bMulti = pFile->IsMultiFile(); // we dont pubish multi torrents here untill thay are compelted and get a Neo Cross Hash
				CFileHash* pHash = pFile->GetHash(bMulti ? HashXNeo : HashNeo);
				if(Hashes.size() == 1 
				 && qobject_cast<CSharedPartMap*>(pFile->GetPartMap()) == NULL 
				 && !bMulti
				 && (!pHash || !pHash->IsComplete())
				 ) // if thats true we dint actually have any data to be published...
					Hashes.clear();*/
			}
			break;
		}
		case eRating:
		{
			if((pFile->HasProperty("Description") || pFile->HasProperty("Rating") || pFile->HasProperty("CoverUrl")) 
				&& FileHashes.contains(pFile->IsMultiFile() ? HashXNeo : HashNeo))
			{
				time_t ExpirationTime = pFile->GetProperty("RatingExpiration").toULongLong();
				if(uNow > ExpirationTime || ExpirationTime - uNow < HR2S(1))  // K-ToDo-Now: customise
					Hashes.insert(NType, FileHashes.value(NType));
			}
			break;
		}
		case eSources:
		{
			foreach(EFileHashType HashType, FileHashes.uniqueKeys())
			{
				switch(HashType)
				{
					case HashNeo:
					case HashXNeo:
					{
						if(pFile->IsNeoShared() && !theCore->m_NeoManager->GetStaticRoutes().isEmpty() && FileHashes.contains(pFile->IsMultiFile() ? HashXNeo : HashNeo))
						{
							time_t ExpirationTime = pFile->GetProperty("EntityExpiration").toULongLong();
							if(uNow > ExpirationTime || ExpirationTime - uNow < HR2S(1))  // K-ToDo-Now: customise
								Hashes.insert(NType, FileHashes.value(NType));
						}
						break;
					}
					case HashEd2k:
					{
						if(pFile->IsEd2kShared() && theCore->Cfg()->GetBool("Ed2kMule/Enable") && FileHashes.contains(HashEd2k))
						{
							time_t ExpirationTime = pFile->GetProperty("SourceExpiration").toULongLong();
							if(uNow > ExpirationTime || ExpirationTime - uNow < HR2S(1))  // K-ToDo-Now: customise
								Hashes.insert(HashEd2k, FileHashes.value(HashEd2k));
						}
						break;
					}
					case HashTorrent:
					{
						if(theCore->Cfg()->GetBool("BitTorrent/Enable") && FileHashes.contains(HashTorrent))
						{
							time_t ExpirationTime = pFile->GetProperty("PeerExpiration").toULongLong();
							if(uNow > ExpirationTime || ExpirationTime - uNow < HR2S(1))  // K-ToDo-Now: customise
							{
								foreach(CFileHashPtr pHash, FileHashes.values(HashTorrent))
									Hashes.insert(HashTorrent, pHash);
							}
						}
						break;
					}
				}
			}
			break;
		}
#ifndef NO_HOSTERS
		case eLinks:
		{
			time_t ExpirationTime = pFile->GetProperty("LinkExpiration").toULongLong();
			if(uNow > ExpirationTime || ExpirationTime - uNow < HR2S(1))  // K-ToDo-Now: customise
			{
				int ArchiveCount = 0;
				int PartCount = 0;
				foreach(CTransfer* pTransfer, pFile->GetTransfers())
				{
					if(!pTransfer->IsDownload())
						continue;
					CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer);
					if(!pHosterLink)
						continue;
					if(pHosterLink->GetPartMap() == NULL)
						ArchiveCount++;
					else
						PartCount++;
				}
				if(ArchiveCount > 0 && FileHashes.contains(HashArchive))
				{
					foreach(CFileHashPtr pHash, FileHashes.values(HashArchive))
						Hashes.insert(HashArchive, pHash);
				}
				if(PartCount > 0 && FileHashes.contains(NType))
					Hashes.insert(NType, FileHashes.value(NType));
			}
			break;
		}
#endif
	}

	return Hashes;
}

void CKadPublisher::Update(uint64 FileID, CKadPublisher::EIndex eIndex, time_t ExpirationTime, int StoreCount, const QMultiMap<EFileHashType, CFileHashPtr>& Hashes)
{
	CFile* pFile = m_FileList.value(FileID);
	if(!pFile)
		return;

	switch(eIndex)
	{
		case eKeyword:
		{
            pFile->SetProperty("KeywordExpiration", (uint64)ExpirationTime);
			pFile->SetProperty("KeywordStoreCount", StoreCount);
			break;
		}
		case eHashes:
		{
            pFile->SetProperty("HasheExpiration", (uint64)ExpirationTime);
			pFile->SetProperty("HasheStoreCount", StoreCount);
			break;
		}
		case eRating:
		{
            pFile->SetProperty("RatingExpiration", (uint64)ExpirationTime);
			pFile->SetProperty("RatingStoreCount", StoreCount);
			break;
		}
		case eSources:
		{
			foreach(EFileHashType HashType, Hashes.uniqueKeys())
			{
				switch(HashType)
				{
					case HashNeo:
					case HashXNeo:
                        pFile->SetProperty("EntityExpiration", (uint64)ExpirationTime);
						pFile->SetProperty("EntityStoreCount", StoreCount);
						break;
					case HashEd2k:
                        pFile->SetProperty("SourceExpiration", (uint64)ExpirationTime);
						pFile->SetProperty("SourceStoreCount", StoreCount);
						break;
					case HashTorrent:
                        pFile->SetProperty("PeerExpiration", (uint64)ExpirationTime);
						pFile->SetProperty("PeerStoreCount", StoreCount);
						break;
				}
			}
			break;
		}
#ifndef NO_HOSTERS
		case eLinks:
		{
            pFile->SetProperty("LinkExpiration", (uint64)ExpirationTime);
			pFile->SetProperty("LinkStoreCount", StoreCount);
			break;
		}
#endif
	}
}

SFileInto CKadPublisher::GetFileInfo(uint64 FileID, uint64 ParentID)
{
	SFileInto FileInfo;

	CFile* pFile = m_FileList.value(FileID);
	if(!pFile)
	{
		ASSERT(0);
		return FileInfo;
	}
	
	FileInfo.FileName = pFile->GetFileName();
	FileInfo.FileSize = pFile->GetFileSize();
	if(CFile* pParentFile = ParentID ? m_FileList.value(ParentID) : NULL)
	{
		QStringList Dir = pFile->GetFileDir().split("/",QString::SkipEmptyParts); // Parent/dir1/dir2/
		if(Dir.count() >= 1) // more than just parent
		{
			Dir.removeFirst(); // name of parent
			FileInfo.FileDirectory = Dir.join("/"); //dir1/dir2
		}
	}
		
	FileInfo.HashMap = GetHashMap(FileID);
	return FileInfo;
}

bool CKadPublisher::IsComplete(uint64 FileID)
{
	CFile* pFile = m_FileList.value(FileID);
	return pFile ? pFile->IsComplete() : false;
}

QVariantMap CKadPublisher::GetRating(uint64 FileID)
{
	QVariantMap Rating;
	if(CFile* pFile = m_FileList.value(FileID))
	{
		Rating["FileName"] = pFile->GetFileName();
		Rating["Rating"] = pFile->GetProperty("Rating").toInt();
		Rating["Description"] = pFile->GetProperty("Description").toString();
		if(pFile->HasProperty("CoverUrl"))
			Rating["CoverUrl"] = pFile->GetProperty("CoverUrl").toString();
	}
	return Rating;
}

void CKadPublisher::AddRatings(uint64 FileID, const QVariantList& Ratings, bool bDone)
{
	if(CFile* pFile = CFileList::GetFile(FileID))
	{
		foreach(const QVariant& vRating, Ratings)
		{
			QVariantMap Rating = vRating.toMap();

			QVariantMap Note;
			Note["Title"] = Rating["FN"];
			Note["Rating"] = Rating["FR"];
			Note["Description"] = Rating["FD"];
			if(Rating.contains("CU"))
				Note["CoverUrl"] = Rating["CU"];

			QVariant vPID = Rating["PID"];
			QByteArray PID;
			if(vPID.type() == QVariant::ByteArray)
				PID = vPID.toByteArray();
			else
			{
				uint64 uPID = vPID.toULongLong();
				PID = QByteArray((char*)&uPID,sizeof(uPID));
			}

			pFile->GetDetails()->Add("neo://" + PID.toHex(), Note);
		}

		if(bDone)
			pFile->SetProperty("NeoLastFindNotes", (uint64)GetTime());
	}
}

#ifndef NO_HOSTERS
QStringList CKadPublisher::GetLinks(uint64 FileID, CFileHashPtr pHash)
{
	CFile* pFile = m_FileList.value(FileID);
	QStringList LinkList;
	if(pFile)
	{
		ASSERT(pHash->GetType() == HashArchive || pHash->GetType() == HashXNeo || pHash->GetType() == HashNeo);
		bool bEncrypted = (theCore->Cfg()->GetBool("Hoster/ProtectLinks") || pFile->GetProperty("ProtectLinks").toBool());

		CHashFunction Hash(CAbstractKey::eSHA1);
		foreach(CTransfer* pTransfer, pFile->GetTransfers())
		{
			if(!pTransfer->IsDownload())
				continue;
			CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer);
			if(!pHosterLink)
				continue;

			// we publish eider part links under HashNeo/NashXNeo or Archive links under HashArchive
			if(pHash->GetType() == HashArchive) // for archive we have to check if its the right archive set
			{
				QString Enc;

				SArcInfo ArcInfo = GetArcInfo(pHosterLink->GetFileName());
				Enc = ArcInfo.FileName.toLower();
				Enc.remove(QRegExp("[^A-Za-z0-9]"));
				if(!ArcInfo.ArchiveExt.isEmpty())
					Enc += QString(".%1").arg(ArcInfo.ArchiveExt);
				if(ArcInfo.PartNumber != -1)
					Enc += ".multi";

				if(Enc.isEmpty())
					continue;

				Hash.Reset();
				Hash.Add(Enc.toUtf8());
				Hash.Finish();
				if(pHash->GetHash().left(Hash.GetSize()) != Hash.ToByteArray())
					continue;
			}
			else if(pHosterLink->GetPartMap() == NULL) // all part links are fot the same hash
				continue; 

			QString Url;
			if(bEncrypted || pHosterLink->GetProtection())
				Url = CHosterLink::EncryptLinks(QStringList(pHosterLink->GetUrl()));
			else
				Url = pHosterLink->GetUrl();
			LinkList.append(Url);
		}
	}
	return LinkList;
}

void CKadPublisher::AddLinks(uint64 FileID, const QStringList& Urls)
{
	if(CFile* pFile = CFileList::GetFile(FileID))
	{
		foreach(const QString& Url, Urls)
			theCore->m_WebManager->AddToFile(pFile, Url, eNeo, CHosterLink::eTemporary);
	}
}
#endif

void CKadPublisher::AddSources(uint64 FileID, const QMultiMap<QString, SNeoEntity>& Neos, const QList<SMuleSource>& Mules, const QMultiMap<QByteArray, STorrentPeer>& Peers)
{
	if(CFile* pFile = CFileList::GetFile(FileID))
	{
		foreach(const QString& Hash, Neos.uniqueKeys())
		{
			if(CFileHashPtr pHash = CFileHashPtr(CFileHash::FromString(Hash.toLatin1())))
			{
				foreach(const SNeoEntity& Neo, Neos.values(Hash))
					theCore->m_NeoManager->AddToFile(pFile, pHash.data(), Neo, eNeo);
			}
		}
		foreach(const SMuleSource& Mule, Mules)
			theCore->m_MuleManager->AddToFile(pFile, Mule, eNeo);
		foreach(const QByteArray& InfoHash, Peers.uniqueKeys())
		{
			if(CTorrent* pTorent = pFile->GetTorrent(InfoHash))
			{
				foreach(const STorrentPeer& Peer, Peers.values(InfoHash))
					theCore->m_TorrentManager->AddToFile(pTorent, Peer, eNeo);
			}
		}
	}
}

QMultiMap<EFileHashType, CFileHashPtr> CKadPublisher::GetHashes(uint64 FileID)
{
	QMultiMap<EFileHashType, CFileHashPtr> AllHashes;
	if(CFile* pFile = CFileList::GetFile(FileID))
	{
		foreach(CFileHashPtr pHash, pFile->GetAllHashes(true))
		{
			if(!pHash->IsValid()) // if its all 0000... ignore it
				continue;

			AllHashes.insert(pHash->GetType(), pHash);
		}
	}
	return AllHashes;
}

void CKadPublisher::AddAuxHashes(uint64 FileID, uint64 uFileSize, const QVariantMap& HashMap)
{
	if(CFile* pFile = CFileList::GetFile(FileID))
	{
		theCore->m_NeoManager->AddAuxHashes(pFile, ReadHashMap(HashMap, uFileSize).values());
	}
}

QList<uint64> CKadPublisher::GetSubFiles(uint64 FileID)
{
	CFile* pFile = m_FileList.value(FileID);
	QList<uint64> SubFiles;
	if(CJoinedPartMap* pParts = pFile ? qobject_cast<CJoinedPartMap*>(pFile->GetPartMap()) : NULL)
	{
		QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();
		for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin();)
		{
			SPartMapLink* pLink = *(--I);
			SubFiles.append(pLink->ID);
		}
	}
	return SubFiles;
}

QList<uint64> CKadPublisher::GetParentFiles(uint64 FileID)
{
	CFile* pFile = m_FileList.value(FileID);
	QList<uint64> ParentFiles;
	if(CSharedPartMap* pParts = pFile ? qobject_cast<CSharedPartMap*>(pFile->GetPartMap()) : NULL)
	{		
		foreach(SPartMapLink* pLink, pParts->GetLinks())
			ParentFiles.append(pLink->ID);
	}
	return ParentFiles;
}

void CKadPublisher::SetupIndex(uint64 FileID, const QString& FileName, uint64 uFileSize, const QList<SFileInto>& SubFiles)
{
	if(CFile* pFile = CFileList::GetFile(FileID))
	{
		QList<CNeoManager::SSubFile> Files;
		for(int i=0; i < SubFiles.count(); i++)
		{
			SFileInto FileInfo = SubFiles[i];

			CNeoManager::SSubFile File;
			File.FileName = FileInfo.FileName;
			File.FileSize = FileInfo.FileSize;
			File.FileDirectory = FileInfo.FileDirectory;
			File.FileHashes = ReadHashMap(FileInfo.HashMap, FileInfo.FileSize).values();
			Files.append(File);
		}
		
		theCore->m_NeoManager->InstallMetaData(pFile, uFileSize, Files);
	}
}

//
