#include "GlobalHeader.h"
#include "File.h"
#include "FileList.h"
#include "../FileTransfer/Transfer.h"
#include "PartMap.h"
#include "IOManager.h"
#include "./Hashing/FileHashTree.h"
#include "./Hashing/FileHashTreeEx.h"
#include "./Hashing/FileHashSet.h"
#include "./Hashing/UntrustedFileHash.h"
#include "../NeoCore.h"
#include "./Hashing/HashingThread.h"
#include "./Hashing/HashingJobs.h"
#include "../Networking/BandwidthControl/BandwidthLimit.h"
#include "../FileTransfer/BitTorrent/TorrentManager.h"
#include "../FileTransfer/BitTorrent/TorrentInfo.h"
#include "../FileTransfer/BitTorrent/Torrent.h"
#include "../FileTransfer/BitTorrent/TorrentPeer.h"
#include "../FileTransfer/FileGrabber.h"
#include "./FileManager.h"
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/ArchiveDownloader.h"
#include "../FileTransfer/HosterTransfer/HosterLink.h"
#include "../FileTransfer/HosterTransfer/ArchiveSet.h"
#endif
#include "../FileTransfer/HashInspector.h"
#include "../FileTransfer/PartDownloader.h"
#include "FileStats.h"
#include "../../Framework/OtherFunctions.h"
#include "FileDetails.h"
#include <QFutureWatcher>
#include <QtConcurrent>


CFile::CFile(QObject* qObject)
: QObjectEx(qObject)
{
	m_FileID = CFileList::AllocID();

	m_Downloader = NULL;

	m_Inspector = NULL;

	m_FileSize = 0;
	m_State = eStopped;
	m_Status = eNone;
	m_Halted = false;

	m_QueuePos = 0;

	m_UpLimit = NULL;
	m_DownLimit = NULL;

	m_UploadedBytes = 0;
	m_DownloadedBytes = 0;

	m_ActiveDownloads = 0;
	m_WaitingDownloads = 0;
	m_ActiveUploads = 0;
	m_WaitingUploads = 0;

	m_FileStats = new CFileStats(this);

	m_FileDetails = new CFileDetails(this);
	connect(m_FileDetails, SIGNAL(Update()), this, SIGNAL(Update()));
}

CFile::~CFile()
{
	Hold();
	Disable();

	CFileList::ReleaseID(m_FileID);
	m_Parts.clear();
	m_HashMap.clear();
}

void CFile::SetFileID(uint64 FileID)
{
	if(m_FileID != FileID)
	{
		CFileList::ReleaseID(m_FileID);
		m_FileID = CFileList::AllocID(FileID);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// File creation function
//

bool CFile::AddFromFile(const QString& FilePath)
{
	SetFilePath(FilePath);
	SetFileName(Split2(FilePath, "/", true).second);

	uint64 uFileSize = QFileInfo(FilePath).size();
	if(uFileSize == 0)
		return false;

	m_Status = eComplete;

	SetFileSize(uFileSize);
	SetProperty("LastModified", QFileInfo(FilePath).lastModified());
	
	if(theCore->Cfg()->GetBool("Ed2kMule/ShareDefault"))
		SetProperty("Ed2kShare", true);
	SetProperty("NeoShare", true);

	if(theCore->Cfg()->GetBool("Content/ShareNew"))
		m_State = eStarted;

	m_pMasterHash.clear();
	return true;
}

void CFile::SetFileName(const QString& FileName)
{
	m_FileName = FileName;

	SetProperty("Streamable", (Split2(m_FileName, ".", true).second.indexOf(QRegExp(theCore->Cfg()->GetString("Content/Streamable")), Qt::CaseInsensitive) == 0));

	SetProperty("KeywordExpiration", QVariant());
}

void CFile::GrabDescription()
{
	CLinkedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data());
	if(!pParts)
		return;

	QString Description;

	int PendingCount = 0;
	foreach(SPartMapLink* pLink, pParts->GetLinks())
	{
		CFile* pSubFile = GetList()->GetFileByID(pLink->ID);
		ASSERT(pSubFile); // we shouldnt arive at this point if the multi file is missing files

		if(GetFileExt(pSubFile->GetFileName()).compare("nfo",Qt::CaseInsensitive) == 0)
		{
#ifndef CRAWLER
			if(QFileInfo(pSubFile->GetFilePath()).size() > KB2B(100))
				continue;
#endif
			Description = ReadFileAsString(pSubFile->GetFilePath());
			break;
		}
	}

	if(!Description.isEmpty())
		SetProperty("Description", Description);
}

QList<CFileHashPtr> CFile::GetListForHashing(bool bEmpty)
{
	QList<CFileHashPtr> List;
	foreach(const CFileHashPtr& pHash, bEmpty ? m_HashMap.values() : GetAllHashes())
	{
		ASSERT(pHash->GetType() != HashArchive);
		CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(pHash.data());
		if(!pHashEx)
			continue; // this is an empty hash like an archive hash
		if(!bEmpty)
		{
			if(!pHash->IsValid())
				continue; // thats an invlaid hash, probably an untrusted one
			if(!pHashEx->IsLoaded())
				continue; // we cant use it :'(
		}
		else if(pHash->IsComplete())
			continue; // skip it, already completed
		List.append(pHash);
	}
	return List;
}

void CFile::MediaInfoRead(int Index)
{
	QFutureWatcher<QString>* pWatcher = (QFutureWatcher<QString>*)sender();

	m_FileDetails->MediaInfoRead(pWatcher->resultAt(Index));
}

void CFile::CalculateHashes(bool bAll)
{
	ASSERT(!IsPending() && !MetaDataMissing());
	ASSERT(IsMultiFile() || QFile::exists(m_FilePath));

	if (!IsMultiFile() && !m_FileDetails->HasDetails("mediainfo://"))
	{
		QFutureWatcher<QString>* pWatcher = new QFutureWatcher<QString>(this); // Note: the job will be canceled if the file will be deleted :D
		connect(pWatcher, SIGNAL(resultReadyAt(int)), this, SLOT(MediaInfoRead(int)));
		connect(pWatcher, SIGNAL(finished()), pWatcher, SLOT(deleteLater()));
		pWatcher->setFuture(QtConcurrent::run(CFileDetails::ReadMediaInfo, m_FilePath));
	}

	CleanUpHashes();

	QStringList NewHashes;
	if(IsMultiFile())
	{
		if(!m_HashMap.contains(HashXNeo))
		{
			m_HashMap.insert(HashXNeo, CFileHashPtr(new CFileHashTreeEx(HashXNeo, GetFileSize())));
			NewHashes.append(CFileHash::HashType2Str(HashXNeo));
		}
	}
	else
	{
		if(!m_HashMap.contains(HashNeo))
		{
			m_HashMap.insert(HashNeo, CFileHashPtr(new CFileHashTree(HashNeo, GetFileSize())));
			NewHashes.append(CFileHash::HashType2Str(HashNeo));
		}
		if(!m_HashMap.contains(HashEd2k))
		{
			m_HashMap.insert(HashEd2k, CFileHashPtr(new CFileHashSet(HashEd2k, GetFileSize())));
			NewHashes.append(CFileHash::HashType2Str(HashEd2k));
		}
		if(IsEd2kShared() && !m_HashMap.contains(HashMule))
		{
			m_HashMap.insert(HashMule, CFileHashPtr(new CFileHashTree(HashMule, GetFileSize())));
			NewHashes.append(CFileHash::HashType2Str(HashMule));
		}
		//if(!m_HashMap.contains(HashTigerTree))
		//{
		//	m_HashMap.insert(HashTigerTree, CFileHashPtr(new CFileHashTree(HashTigerTree, GetFileSize()))); // for TTH - dont use its really slow
		//	NewHashes.append(CFileHash::HashType2Str(HashTigerTree));
		//}
	}

	QList<CFileHashPtr> List;
	foreach(const CFileHashPtr& pHash, m_HashMap)
	{
		CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(pHash.data());
		ASSERT(pHashEx);
		if(!bAll && pHash->IsComplete())
			continue; // skip it, already completed
		List.append(pHash);
	}

	if(!List.isEmpty())
	{
		QStringList Hashes;
		foreach(CFileHashPtr pHash, List)
			Hashes.append(CFileHash::HashType2Str(pHash->GetType()));

		LogLine(LOG_INFO, tr("File %1 has ben queued for hashing: %2").arg(m_FileName).arg(Hashes.join(", ")) + (!NewHashes.isEmpty() ? tr(" (added %1)").arg(NewHashes.join(", ")) : ""));

		CHashingJobPtr pHashingJob = CHashingJobPtr(new CHashFileJob(GetFileID(), List));
		connect(pHashingJob.data(), SIGNAL(Finished()), this, SLOT(OnFileHashed()));
		theCore->m_Hashing->AddHashingJob(pHashingJob);
	}
}

void CFile::CleanUpHashes()
{
	// Note: sometimes when starting out with undefined hash types HashTorrent or HashArchive we may get invalitd types in
	if(!IsMultiFile())
	{
		if(m_HashMap.contains(HashXNeo))
			DelHash(m_HashMap.value(HashXNeo));
	}
	else
	{
		if(m_HashMap.contains(HashNeo))
			DelHash(m_HashMap.value(HashNeo));
		if(m_HashMap.contains(HashEd2k))
			DelHash(m_HashMap.value(HashEd2k));
		if(m_HashMap.contains(HashMule))
			DelHash(m_HashMap.value(HashMule));
		//if(m_HashMap.contains(HashTigerTree))
		//	DelHash(m_HashMap.value(HashTigerTree));
	}
}

void CFile::AddEmpty(EFileHashType MasterHash, const QString& FileName, uint64 uFileSize, bool bPending)
{	
	ASSERT(uFileSize > 0 || (MasterHash == HashTorrent || MasterHash == HashArchive)); // only torrents and archives can have a size 0

	SetFileName(FileName);
	SetFileSize(uFileSize);

	ASSERT(m_pMasterHash.isNull());
	if(MasterHash == HashArchive)
		m_pMasterHash = CFileHashPtr(new CFileHash(HashArchive)); // set an dummy master it will be replaced by archive downloader
	else if(MasterHash == HashTorrent)
	{
		if(CTorrent* pTorrent = GetTopTorrent())
			m_pMasterHash = pTorrent->GetHash();
	}
	else if(MasterHash != HashUnknown || SelectMasterHash())
		m_pMasterHash = GetHashPtr(MasterHash);
		
	if(m_pMasterHash.isNull())
		m_pMasterHash = CFileHashPtr(new CFileHash(MasterHash));
	else
		MasterHash = m_pMasterHash->GetType();

	// Note: the master hash may be HashTorrent but may not have been set thats a torrent sub file
#ifndef NO_HOSTERS
	if(MasterHash != HashTorrent && !IsRawArchive())
#else
	if(MasterHash != HashTorrent)
#endif
	{
		if(!m_HashMap.contains(HashXNeo) && m_FileSize) // if this is a single file we can initialize a partmap without metadata
			SetPartMap(CPartMapPtr(new CSynced<CPartMap>(m_FileSize)));
	}

	if(bPending)
		m_Status = ePending; 
	else
		m_Status = eIncomplete; 
	InitEmpty();
}

void CFile::InitEmpty()
{
	if(!IsPending())
		SetFilePath();

	ASSERT(m_Inspector == NULL);
	m_Inspector = new CHashInspector(this);
}

bool CFile::AddNewMultiFile(const QString& FileName, const QList<uint64>& SubFiles)
{
#ifdef CRAWLER
	QString Release;
#endif
	bool bIncomplete = false;
	uint64 uFileSize = 0;
	QList<CFile*> FileList;
	foreach(uint64 SubFileID, SubFiles)
	{
		CFile* pSubFile = GetList()->GetFileByID(SubFileID);
		if(!pSubFile)
			LogLine(LOG_ERROR, tr("Attempted to add an nonexisting file to a multifile"));
		else
		{
			if(!pSubFile->IsComplete())
			{
				if(pSubFile->IsRemoved())
					pSubFile->Start();

				bIncomplete = true;
			}
			
			if(pSubFile->IsMultiFile())
				LogLine(LOG_ERROR, tr("Sub File %1 can not be added to a an other multi file").arg(pSubFile->GetFileName()));
			else
			{
#ifdef CRAWLER
				if(pSubFile->GetFileName().compare("nfo.txt",Qt::CaseInsensitive) == 0) 
				{
					Release = ReadFileAsString(pSubFile->GetFilePath());
					continue;
				}
#endif

				if(FileList.contains(pSubFile))
					LogLine(LOG_ERROR, tr("Sub File %1 can not be added twice to a multifile").arg(pSubFile->GetFileName()));
				else
				{
					uFileSize += pSubFile->GetFileSize();
					FileList.append(pSubFile);
				}
			}
		}
	}
#ifdef CRAWLER
	if(!Release.isEmpty())
		SetProperty("Release", Release);
#endif

	if(FileList.isEmpty())
	{
		LogLine(LOG_ERROR, tr("New Multi file %1 can not contain no files").arg(FileName));
		return false;
	}

	SetFileName(FileName);
	SetFilePath(true);

	SetFileSize(uFileSize);

	CJoinedPartMap* pParts = new CJoinedPartMap(uFileSize);
	if(!bIncomplete)
		pParts->SetRange(0, -1, Part::Verified);

	uint64 FileID = GetFileID();
	ASSERT(FileID);

	uint64 uBegin = 0;
	uint64 uEnd = 0;
	foreach(CFile* pSubFile, FileList)
	{
		CSharedPartMap* pSubParts = pSubFile->SharePartMap();

		uEnd = uBegin + pSubFile->GetFileSize();
		pParts->SetupLink(uBegin, uEnd, pSubFile->GetFileID());
		pSubParts->SetupLink(uBegin, uEnd, FileID);

		uBegin = uEnd;
	}

	SetPartMap(CPartMapPtr(pParts));

	GrabDescription();

	if(bIncomplete)
	{
		m_Status = eIncomplete;
		m_pMasterHash = CFileHashPtr(new CFileHash(HashXNeo));
	}
	else
		m_Status = eComplete;

	SetProperty("NeoShare", true);

	return true;
}

void CFile::SetFilePath(const QString& FilePath)
{
	ASSERT(m_Status != ePending);

	m_FilePath = FilePath;

	// cleanup // and /// and etc...
	QString tmpFilePath;
	do
	{
		tmpFilePath = m_FilePath;
		m_FilePath = tmpFilePath.replace("//", "/");
	}
	while(tmpFilePath != m_FilePath);

	QStringList Shared = theCore->Cfg()->GetStringList("Content/Shared");
	Shared.append(theCore->GetIncomingDir(false));
	Shared.append(theCore->GetTempDir(false));
	QString Root;
	QString RelativePath = GetRelativeSharedPath(m_FilePath, Shared, Root);
	RelativePath.truncate(RelativePath.lastIndexOf("/") + 1); // remove file name leave /
	SetFileDir(RelativePath);
}

void CFile::SetFilePath(bool bComplete)
{
	int Unifyed = theCore->Cfg()->GetInt("Content/UnifyedDirs");

	QString FilePath = (bComplete || Unifyed != 0) ? theCore->GetIncomingDir() : theCore->GetTempDir();
	FilePath += m_FileDir;
	if(!IsMultiFile())
	{
		StrPair NameExt = Split2(m_FileName, ".", true);
		if(NameExt.first.isEmpty())
		{
			NameExt.first = NameExt.second;
			NameExt.second = "";
		}
		else
			NameExt.second = "." + NameExt.second;

		if(!bComplete && Unifyed != 2)
			NameExt.second += ".temp";

		QString Num;
		for(int i=0;;)
		{
			QString TestPath = FilePath + NameExt.first + Num + NameExt.second;
			if(!QFile::exists(TestPath))
				break;
			if(m_FilePath == TestPath) // if we are already there its ok to keep the name
				break;
			Num = QString("(%1)").arg(++i);
		}
		FilePath += NameExt.first + Num + NameExt.second;
	}
	else
	{
		QString FileName = m_FileName; // Note: the storrend file name must not be FileSystem safe
		FileName.replace(QRegExp("[:*?<>|\"\\/]"), "_");
		FilePath += FileName;
	}
	SetFilePath(FilePath);
}

void CFile::SetPending(bool bPending)
{
	ASSERT(bPending != (m_Status == ePending));
	if(bPending) 
		m_Status = ePending; 
	else if(m_Status == ePending)
	{
		m_Status = eIncomplete;
		SetFilePath();
	}
}

bool CFile::IsStarted()
{
	if(HasError() || IsRemoved() || IsHalted()) 
		return false;
	return m_State != eStopped;
}

bool CFile::IsPaused(bool bIgnore)
{
#ifndef NO_HOSTERS
	// Note: raw archive does not need metadata and those is not compulsory paused
	if(!bIgnore && (IsPending() || (MetaDataMissing() && !IsRawArchive())))
#else
	if(!bIgnore && (IsPending() || MetaDataMissing()))
#endif
		return true;
	return m_State == ePaused;
}

bool CFile::IsIncomplete()
{
	return !m_pMasterHash.isNull();
}

bool CFile::IsComplete(bool bAux)
{
	if(!IsIncomplete() && !IsRemoved())
		return true; // is really complete
	if(bAux)
		return m_Status == eComplete;
	return false;
}

bool CFile::IsHashing()
{
	if(theCore->m_Hashing->IsHashing(GetFileID()))
		return true;
	if(CFileHashPtr pHash = m_HashMap.value(IsMultiFile() ? HashXNeo : HashNeo))
		return !pHash->IsValid();
	return false;
}

void CFile::SetFileSize(const uint64 FileSize)
{
	if(m_FileSize == FileSize)
		return;

	ASSERT(!m_FileSize || m_FileSize == FileSize);
	m_FileSize = FileSize;
	if(m_FileSize != 0)
	{
		m_FileStats->SetupAvailMap();
		foreach(const CFileHashPtr& pHash, m_HashMap)
		{
			if(!pHash->inherits("CFileHashEx"))
			{
				CFileHashPtr pFileHash = CFileHashPtr(CFileHash::New(pHash->GetType(), m_FileSize));
				pFileHash->SetHash(pHash->GetHash());
				AddHash(pFileHash);
			}
		}
	}
}

uint64 CFile::GetFileSizeEx()
{
	uint64 uFileSize = m_FileSize;
	if(uFileSize == 0)
		uFileSize = GetProperty("EstimatedSize").toULongLong();
	return uFileSize;
}

void CFile::SetPartMap(CPartMapPtr pMap)
{
	ASSERT(m_FileSize == pMap->GetSize()); 

#ifdef _DEBUG
	if(CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
	{
		if(!pParts->HasLinks())
		{
			ASSERT(0);
			return;
		}
	}
#endif

	m_Parts = pMap;
	if(m_Downloader)
		connect(m_Parts.data(), SIGNAL(Change(bool)), m_Downloader, SLOT(OnChange(bool)));

	emit MetadataLoaded();

	UpdateSelectionPriority();
}

bool CFile::AddTorrentFromFile(const QByteArray& Torrent)
{
	ASSERT(GetPartMap() == NULL);

	// InitEmpty();
	ASSERT(m_Inspector == NULL);
	m_Inspector = new CHashInspector(this);

	CTorrent* pTorrent = new CTorrent(this);
	if(!pTorrent->AddTorrentFromFile(Torrent))
	{
		delete pTorrent;
		return false;
	}
	ASSERT(!m_Torrents.contains(pTorrent->GetInfoHash()));
	m_Torrents[pTorrent->GetInfoHash()] = pTorrent;

	return true;
}

bool CFile::MakeTorrent(uint64 uPieceLength, bool bMerkle, const QString& Name, bool bPrivate)
{
	if(m_Torrents.contains(EMPTY_INFO_HASH))
	{
		LogLine(LOG_ERROR, tr("File %1 is already making a new torrent").arg(GetFileName()));
		return false;
	}

	CTorrent* pTorrent = new CTorrent(this);
	if(!pTorrent->MakeTorrent(uPieceLength, bMerkle, Name, bPrivate))
	{
		delete pTorrent;
		return false;
	}
	m_Torrents.insert(EMPTY_INFO_HASH, pTorrent);
	
	QList<CFileHashPtr> List;
	List.append(pTorrent->GetHash());
	
	CHashingJobPtr pHashingJob = CHashingJobPtr(new CHashFileJob(GetFileID(), List));
	connect(pHashingJob.data(), SIGNAL(Finished()), pTorrent, SLOT(OnFileHashed()));
	theCore->m_Hashing->AddHashingJob(pHashingJob);

	SetProperty("Torrent", 2);

	return true;
}

bool CFile::AddTorrent(const QByteArray &TorrentData)
{
	CTorrent* pTorrent = new CTorrent(this);
	if(!pTorrent->ImportTorrent(TorrentData))
	{
		delete pTorrent;
		return false;
	}

	if(GetProperty("Description").toString().isEmpty())
		SetProperty("Description", pTorrent->GetInfo()->GetProperty("Description"));

	if(IsIncomplete())
	{
		if(m_Torrents.contains(pTorrent->GetHash()->GetHash()))
		{
			LogLine(LOG_ERROR, tr("File %1 is already has that torrent").arg(GetFileName()));
			delete pTorrent;
			return false;
		}
		
		m_Torrents[pTorrent->GetHash()->GetHash()] = pTorrent;
	}
	else
	{
		if(m_Torrents.contains(EMPTY_INFO_HASH))
		{
			LogLine(LOG_ERROR, tr("File %1 is already making a new torrent").arg(GetFileName()));
			delete pTorrent;
			return false;
		}

		m_Torrents.insert(EMPTY_INFO_HASH, pTorrent);
	
		QList<CFileHashPtr> List;
		List.append(pTorrent->GetHash());
	
		CHashingJobPtr pHashingJob = CHashingJobPtr(new CHashFileJob(GetFileID(), List));
		connect(pHashingJob.data(), SIGNAL(Finished()), pTorrent, SLOT(OnFileHashed()));
		theCore->m_Hashing->AddHashingJob(pHashingJob);
	}

	return true;
}

void CFile::TorrentHashed(CTorrent* pTorrent, bool bSuccess)
{
	if(bSuccess && m_Torrents.contains(pTorrent->GetInfoHash()))
	{
		LogLine(LOG_SUCCESS, tr("File %1 already has exactly a identic torrent").arg(GetFileName()));
		bSuccess = false;
	}

	for(QMap<QByteArray, CTorrent*>::iterator I = m_Torrents.begin(); I != m_Torrents.end(); I++)
	{
		if(I.value() == pTorrent)
		{
			m_Torrents.erase(I);
			if(!bSuccess)
				delete pTorrent;
			else
			{
				m_Torrents.insert(pTorrent->GetInfoHash(), pTorrent);

				LogLine(LOG_SUCCESS, tr("Torrent %1 added").arg(pTorrent->GetInfo()->GetTorrentName()));
			}
			break;		
		}
	}
}

CTorrent* CFile::GetTorrent(const QByteArray& InfoHash)
{
	return m_Torrents.value(InfoHash);
}

CTorrent* CFile::GetTopTorrent()
{
	if(m_Torrents.isEmpty())
		return NULL;

	// If torrent is the master hash we take it
	if(m_pMasterHash && m_pMasterHash->GetType() == HashTorrent)
	{
		if(CTorrent* pTorrent = m_Torrents.value(m_pMasterHash->GetHash()))
			return pTorrent;
	}

	// else we look for the torrent with the most peers
	QMap<QByteArray, int> Torrents;
	foreach(CTransfer* pTransfer, m_Transfers)
	{
		if(CTorrentPeer* pPeer = qobject_cast<CTorrentPeer*>(pTransfer))
			Torrents[pPeer->GetTorrent()->GetInfoHash()]++;
	}
	QByteArray BestInfoHash;
	int BestCount = -1;
	foreach(const QByteArray& InfoHash, Torrents.keys())
	{
		if(BestCount < Torrents.value(InfoHash))
		{
			BestCount = Torrents.value(InfoHash);
			BestInfoHash = InfoHash;
		}
	}
	return BestCount == -1 ? m_Torrents.values().first() : m_Torrents.value(BestInfoHash);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// File Processing
//

void CFile::Process(UINT Tick)
{
	QMap<ETransferType, CFileStats::STemp> TempMap;
	for(QSet<CTransfer*>::iterator I = m_Transfers.begin(); I != m_Transfers.end();)
	{
		CTransfer* pTransfer = *I;
		if(!pTransfer->Process(Tick)) // error failed remove
		{
			pTransfer->Hold(true);
			m_FileStats->RemoveTransfer(pTransfer);
			delete pTransfer;
			I = m_Transfers.erase(I);
			continue;
		}
		I++;

		ETransferType eType = pTransfer->GetType();
		if(pTransfer->IsConnected())
			TempMap[eType].Connected++;
		if(!pTransfer->HasError())
		{
			TempMap[eType].All++;

			if(pTransfer->IsChecked())
				TempMap[eType].Checked++;

			if(pTransfer->IsSeed())
				TempMap[eType].Complete++;
		}
	}
	m_FileStats->UpdateTempMap(TempMap);

	if(Tick & EPerSec)
	{
#ifndef NO_HOSTERS
		if(!IsArchive() && !IsPending() && !MetaDataMissing())
#else
		if(!IsPending() && !MetaDataMissing())
#endif
		{
			//if(IsIncomplete())
			//	m_FileStats->Process(Tick);
			//if(m_Downloader)
			//	m_Downloader->Process(Tick);
			if(m_Inspector)
				m_Inspector->Process(Tick);
		}

		if(m_Status == eComplete || m_Status == eIncomplete)
		{
			bool bComplete = SubFilesComplete();
			if(bComplete && m_Status == eIncomplete)
				SetProperty("CompletedTime", QDateTime::fromTime_t(GetTime()));
			m_Status = bComplete ? eComplete : eIncomplete;
		}
	}
}

bool CFile::SubFilesComplete()
{
	CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data());
	if(!pParts) // if its not a multifile (or its empty) it can only be really compelte or not
		return !IsIncomplete();

	CFileList* pList = GetList();
	QMap<uint64,SPartMapLink*> Links = pParts->GetLinks();
	for(QMap<uint64,SPartMapLink*>::iterator I = Links.begin(); I != Links.end(); I++)
	{
		if(CFile* pSubFile = pList->GetFileByID((*I)->ID))
		{
			if(pSubFile->IsIncomplete() && !pSubFile->IsHalted())
				return false;
		}
	}
	return true;
}

void CFile::OnDataWriten(uint64 Offset, uint64 Length, bool bOk, void* Aux)
{
	if(!m_Parts)
		return;

	CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(m_Parts.data());
	// Note: hashing/caching flags should only be set by the single/shared maps and not by the joined once
	uint64 uBegin = Offset;
	uint64 uEnd = Offset + Length;

	// cear cached flag
	if(pJoinedParts)
		pJoinedParts->SetSharedRange(uBegin, uEnd, Part::Cached, CPartMap::eClr);
	else
		m_Parts->SetRange(uBegin, uEnd, Part::Cached, CPartMap::eClr);

	ASSERT((m_Parts->GetRange(uBegin, uEnd) & (Part::Available | Part::Verified)) == 0);

	if(!bOk)
		return;

	// clear hashing flags as new data must be newly hashed
	if(pJoinedParts)
		pJoinedParts->SetSharedRange(uBegin, uEnd, Part::Verified | Part::Corrupt, CPartMap::eClr);
	else
		m_Parts->SetRange(uBegin, uEnd, Part::Verified | Part::Corrupt, CPartMap::eClr); 

	// Set Available
	if(pJoinedParts)
		pJoinedParts->SetSharedRange(uBegin, uEnd, Part::Available, CPartMap::eAdd);
	else
		m_Parts->SetRange(uBegin, uEnd, Part::Available, CPartMap::eAdd);

	// Reset all hashing results for this range
	if(m_Inspector)
		m_Inspector->ResetRange(uBegin, uEnd);
}

void CFile::OnAllocation(uint64 Progress, bool Finished)
{
	if(m_Parts && Progress)
		m_Parts->SetRange(0, Progress, Part::Allocated, CPartMap::eAdd);
}

void CFile::OnFileHashed()
{
	if(CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
	{
		QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();

		CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(GetHashPtr(HashXNeo).data());
		ASSERT(pHashTreeEx);
		ASSERT(pHashTreeEx->CanHashParts());
		
		CBuffer MetaData;
		for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin();)
		{
			SPartMapLink* pLink = *(--I);

			CFile* pSubFile = GetList()->GetFileByID(pLink->ID);
			if(!pSubFile)
			{
				SetError("Sub File Missing");
				return;
			}

			// Note: hashinf is odne in the order it was scheduled, those hashiung of the multi file finishes always after all sub files have been hashed
			CFileHash* pFileHash = pSubFile->GetHash(HashNeo);
			if(!pFileHash || !pFileHash->IsComplete()) 
			{
				SetError("Sub File Incomplete");
				return;
			}

			MetaData.WriteValue<uint64>(pSubFile->GetFileSize());
			MetaData.WriteQData(pFileHash->GetHash());
		}
		pHashTreeEx->SetMetaHash(pHashTreeEx->HashMetaData(MetaData.ToByteArray()));

		theCore->m_Hashing->SaveHash(pHashTreeEx);
	}

	// in case of we missed somethign resave the hashes
	foreach(const CFileHashPtr& pHash, m_HashMap)
		theCore->m_Hashing->SaveHash(pHash.data());

	ASSERT(IsComplete());
	LogLine(LOG_SUCCESS, tr("File %1 has been being hashed").arg(m_FileName));

	SetProperty("HasheExpiration", QVariant());

	if(CheckDuplicate())
		Remove(true); // this will delete this
}

#ifndef NO_HOSTERS
bool CFile::CompleteFromSolid(const QString& FileName)
{
	if(!IsIncomplete())
	{
		ASSERT(0);
		return false;
	}

	QString FilePath = theCore->GetTempDir() + QString("%1_").arg(GetFileID()) + FileName;

	if(m_Parts.isNull()) // that wil be a RawArchive
	{
		if(!AddFromFile(FilePath))
			return false;

		SetFileName(FileName);

		Resume(); // OpenIO

		OnCompleteFile();
	}
	else // that will be a part file
	{
		if(QFileInfo(FilePath).size() != m_Parts->GetSize())
		{
			LogLine(LOG_ERROR, tr("File %1 could not be downloaded in solid, link wrong").arg(GetFileName()));
			return false;
		}

		CloseIO();

		if(!SafeRemove(m_FilePath))
			LogLine(LOG_ERROR, tr("File %1 could not be removed").arg(m_FilePath));

		m_FilePath = FilePath;

	
		if(CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
		{
			pJoinedParts->SetSharedRange(0, -1, Part::Cached | Part::Verified | Part::Allocated, CPartMap::eClr);
			pJoinedParts->SetSharedRange(0, -1, Part::Available | Part::Allocated, CPartMap::eAdd);
		}
		else
		{
			m_Parts->SetRange(0, -1, Part::Cached | Part::Verified | Part::Allocated, CPartMap::eClr);
			m_Parts->SetRange(0, -1, Part::Available | Part::Allocated, CPartMap::eAdd);
		}

		OpenIO();
	}
	return true;
}
#endif

void CFile::OnCompleteFile()
{
	if(theCore->m_IOManager->HasIO(GetFileID()) && !GetProperty("Temp").toBool())
	{
		// Move IO
		QString FilePath = m_FilePath;
		SetFilePath(true);
		if(m_FilePath != FilePath)
		{
			uint64 uNow = GetCurTick();
			if(!theCore->m_IOManager->MoveIO(GetFileID(), m_FilePath))
			{
				SetFilePath(FilePath);
				SetError(tr("File couldn't be moved"));
				return;
			}
			uint64 uDuration = GetCurTick() - uNow;
			if(uDuration > SEC2MS(5))
				LogLine(LOG_WARNING, tr("File Move took to long (%1 seconds), temp and incomming directories must be on the same physical drive").arg(uDuration/1000));
		}
	}


	// cleanup part map
	CLinkedPartMap* pLinkedParts = qobject_cast<CLinkedPartMap*>(m_Parts.data());
	if(!pLinkedParts || pLinkedParts->GetLinks().isEmpty()) // keep linked part maps unless the links are gone
		m_Parts.clear(); 


	if(IsMultiFile() && !HasProperty("Description"))
		GrabDescription();

	// synchromize Downloaded Bytes 
	m_DownloadedBytes = GetFileSize();

	SetProperty("CompletedTime", QDateTime::fromTime_t(GetTime()));
	m_pMasterHash.clear();
	//SetProperty("RatioActive", true);


	delete m_Downloader;
	m_Downloader = NULL;

	delete m_Inspector;
	m_Inspector = NULL;

	CancelRequest(0, GetFileSize());

	// Cleanup torrent map keep only used torrents on file completion
	for(QMap<QByteArray, CTorrent*>::iterator I = m_Torrents.begin(); I != m_Torrents.end();)
	{
		if(!I.value() || I.value()->GetInfo()->IsEmpty())
			I = m_Torrents.erase(I);
		else
			I++;
	}


	emit FileVerified();

	if(theCore->m_IOManager->HasIO(GetFileID()))
	{
		// calculate all hashes we are interested in
		ProtectIO();
		if(theCore->Cfg()->GetBool("Content/CalculateHashes"))
			CalculateHashes(true);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// File Operations
//

void CFile::Start(bool bPaused, bool bOnError)
{
	bool bReOpen = false;
	if(HasError())
	{
		ClearError();
		bReOpen = !theCore->m_IOManager->HasIO(GetFileID());

		// check if dile is not longer compelte?
		if(m_pMasterHash.isNull() && !SubFilesComplete())
			SelectMasterHash();
	}

	if(IsRemoved())
	{
		bool bComplete = false;
		if (CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
		{
			bComplete = true;

			QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();
			for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin();)
			{
				SPartMapLink* pLink = *(--I);

				CFile* pSubFile = GetList()->GetFileByID(pLink->ID);
				if(!pSubFile)
				{
					LogLine(LOG_ERROR, tr("Multi File %1 is missing sub file!").arg(GetFileName()));
					SetError("SubFile Missing");
					return;
				}

				if(pSubFile->IsRemoved())
					pSubFile->Start();

				if(!pSubFile->IsComplete())
					bComplete = false;
			}
		}

		bReOpen = true;
		SetProperty("LastModified", QVariant());

		if(!bOnError)
		{
			m_DownloadedBytes = 0;
			m_UploadedBytes = 0;

			m_Status = eIncomplete;

			InitEmpty();

			PurgeFile();
		}

		if(bComplete)
		{
			m_Status = eComplete;

			m_pMasterHash.clear();
		}
		else
		{
			m_Status = eIncomplete;

			// if the file was removed and is now restarted it is empty those we mark it as incomplete by setting the Master hash for downlaod
			if(m_pMasterHash.isNull())
				SelectMasterHash();
		}

		if(!bOnError)
		{
			if(!m_Parts)
			{
				if(m_FileSize) // we assume this is a single file as multi files keep the partmap permanently
					SetPartMap(CPartMapPtr(new CSynced<CPartMap>(m_FileSize)));
			}
			else
				m_Parts->SetRange(0, -1, Part::Available | Part::Verified, CPartMap::eClr);

			SetFilePath();
		}
	}

	if(!bOnError) // if we resume from an error dont change the state
		m_State = bPaused ? ePaused : eStarted;

	if(!IsHalted() && m_State != eStopped)
		Enable();

#ifndef NO_HOSTERS
	if(bReOpen && !IsPending() && !MetaDataMissing() && !IsRawArchive())
#else
	if(bReOpen && !IsPending() && !MetaDataMissing())
#endif
		OpenIO(); // Note: this does StartAllocate if file is started
	else if(theCore->m_IOManager->HasIO(GetFileID()) && m_State == eStarted && IsIncomplete()) 
		StartAllocate(); 

#ifndef NO_HOSTERS
	ASSERT(IsPending() || MetaDataMissing() || theCore->m_IOManager->HasIO(GetFileID()) || IsRawArchive());
#else
	ASSERT(IsPending() || MetaDataMissing() || theCore->m_IOManager->HasIO(GetFileID()));
#endif

	UpdateSelectionPriority();
}

void CFile::StartAllocate()
{
	ASSERT(m_Parts);
	uint64 uPreallocation = theCore->Cfg()->GetUInt64("Content/Preallocation");
	if(uPreallocation != 0 && theCore->m_IOManager->GetSize(GetFileID()) < m_FileSize && m_FileSize >= uPreallocation)
	{
		theCore->m_IOManager->Allocate(GetFileID(), m_FileSize, this);
		if(m_Parts)
			m_Parts->SetRange(0, -1, Part::Allocated, CPartMap::eClr);
	}
	else
	{
		if(m_Parts)
			m_Parts->SetRange(0, -1, Part::Allocated, CPartMap::eAdd);
	}
}

void CFile::Stop(bool bOnError)
{
	if(!bOnError)
		m_State = eStopped;

	if(!IsHalted())
	{
		Hold();
		Disable();
	}

	if(bOnError)
		CloseIO();
}

void CFile::SetHalted(bool bSet)
{
	if(m_Halted == bSet)
		return;

	if(bSet && IsComplete())
	{
		LogLine(LOG_WARNING, tr("Complete Files can't be Halted"));
		return;
	}

	m_Halted = bSet;

	if(m_Halted)
	{
		Hold();
		Disable();
	}
	else
	{
		
#ifndef NO_HOSTERS
		ASSERT(IsPending() || MetaDataMissing() || theCore->m_IOManager->HasIO(GetFileID()) || IsRawArchive());
#else
		ASSERT(IsPending() || MetaDataMissing() || theCore->m_IOManager->HasIO(GetFileID()));
#endif

		Enable();
	}
}

void CFile::Enable()
{
	ASSERT(m_Status != eNone);

	SetProperty("StartTime", QDateTime::fromTime_t(GetTime()));

	if(CSharedPartMap* pParts = qobject_cast<CSharedPartMap*>(m_Parts.data()))
	{
		pParts->SetRange(0, -1, Part::Disabled, CPartMap::eClr);
		pParts->NotifyChange(); // notify aboult a substantial change to the part map
	}

	if(m_UpLimit || m_DownLimit)
		return;

	m_UpLimit = new CBandwidthLimit(this);
	m_DownLimit = new CBandwidthLimit(this);
	UpdateBC();

	if(IsIncomplete())
	{
		if(!m_Inspector)
			m_Inspector = new CHashInspector(this);
		m_Downloader = new CPartDownloader(this);
		if(m_Parts)
			connect(m_Parts.data(), SIGNAL(Change(bool)), m_Downloader, SLOT(OnChange(bool)));
	}
}

void CFile::UpdateBC()
{
	int BCPriority = 100;
	if(int iPriority = GetProperty("Priority", 0).toInt())
	{
			 if(iPriority <= 1)		BCPriority = BW_PRIO_LOWEST;
		else if(iPriority <= 3)		BCPriority = BW_PRIO_LOWEST * 2;
		else if(iPriority == 5)		BCPriority = BW_PRIO_NORMAL;
		else if(iPriority >= 9)		BCPriority = BW_PRIO_HIGHEST / 2;
		else if(iPriority >= 7)		BCPriority = BW_PRIO_HIGHEST;
	}

	if(m_UpLimit)
	{
		m_UpLimit->SetLimit(GetProperty("Upload", -1).toInt());
		//m_UpLimit->SetPriority(BCPriority);
	}
	if(m_DownLimit)
	{
		ASSERT(BCPriority >= 0);
		m_DownLimit->SetLimit(GetProperty("Download", -1).toInt());
		m_DownLimit->SetPriority(BCPriority);
	}
}

void CFile::Disable()
{
    SetProperty("LastActiveTime", (uint64)GetActiveTime());
	SetProperty("StartTime", QVariant());

	if(CSharedPartMap* pParts = qobject_cast<CSharedPartMap*>(m_Parts.data()))
	{
		pParts->SetRange(0, -1, Part::Disabled, CPartMap::eAdd);
		pParts->NotifyChange(); // notify aboult a substantial change to the part map
	}

	if(!m_UpLimit && !m_DownLimit)
		return;

	delete m_UpLimit;
	m_UpLimit = NULL;
	delete m_DownLimit;
	m_DownLimit = NULL;

	if(m_Downloader)
	{
		delete m_Downloader;
		m_Downloader = NULL;
	}
}

time_t CFile::GetActiveTime()
{
	time_t uActiveTime = GetProperty("LastActiveTime").toULongLong();

	QDateTime StartTime = GetProperty("StartTime").toDateTime();
	time_t uStartTime = StartTime.isValid() ? StartTime.toTime_t() : 0;
	if(uStartTime)
	{
		time_t uNow = GetTime();
		if(uStartTime < uNow)
			uActiveTime += (uNow - uStartTime);
	}

	return uActiveTime;
}

void CFile::Hold(bool bDelete)
{
	m_ActiveDownloads = 0;
	m_WaitingDownloads = 0;
	m_ActiveUploads = 0;
	m_WaitingUploads = 0;

	foreach(CTransfer* pTransfer, m_Transfers)
		pTransfer->Hold(bDelete);

	if(m_Parts)
	{
		//m_Parts->SetRange(0, -1, Part::Marked | Part::Scheduled | Part::Requested, CPartMap::eClr);
		SPartRange PartRange;
		PartRange.uScheduled = -1;
		PartRange.uRequested = -1;
		m_Parts->SetRange(0, -1, PartRange, CPartMap::eClr);
	}
}

void CFile::UnRemove(CFileHashPtr pFileHash)
{
	ASSERT(IsRemoved());

	m_pMasterHash = pFileHash;
	m_Status = pFileHash ? eComplete : eIncomplete;

	if(IsIncomplete())
	{
		SetProperty("LastModified", QVariant());
		SetFilePath();
	}

	if(!IsMultiFile() && theCore->Cfg()->GetBool("Ed2kMule/ShareDefault"))
		SetProperty("Ed2kShare", true);
	SetProperty("NeoShare", true);

	m_UploadedBytes = 0;
	m_DownloadedBytes = 0;

	if(m_Parts)
	{
		if(IsIncomplete())
			m_Parts->SetRange(0, -1, Part::Available | Part::Verified, CPartMap::eClr);
		else
			m_Parts->SetRange(0, -1, Part::Available | Part::Verified, CPartMap::eAdd);
	}
}

void CFile::Resume()
{
	ASSERT(!IsPending() && !MetaDataMissing());

	// Restore Links
	if (CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
	{
		uint64 FileID = GetFileID();
		ASSERT(FileID);

		CFileList* pList = GetList();

		int InCompleteCount = 0;

		int Counter = 0;
		QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();
		for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin(); Counter++)
		{
			SPartMapLink* pLink = *(--I);

			CFile* pSubFile = pList->GetFileByID(pLink->ID);
			CSharedPartMap* pSubParts = pSubFile ? qobject_cast<CSharedPartMap*>(pSubFile->GetPartMap()) : NULL;
			if(pSubFile && pSubParts == NULL)
			{
				LogLine(LOG_WARNING, tr("Multi File %1 has a sub file %2 that isn't linked").arg(m_FileName).arg(pSubFile->GetFileName()));
				if(pSubFile->IsMultiFile())
					pSubFile = NULL;
			}
			
			//
			if(!pSubFile)
			{
				QString FileName = "Missing File";

				pSubFile = new CFile();
				if(GetProperty("Temp").toBool())
					pSubFile->SetProperty("Temp", true);

				if(CTorrent* pTorrent = GetTopTorrent())
				{
					FileName = pTorrent->GetSubFileName(Counter);
					pSubFile->SetFileDir(pTorrent->GetSubFilePath(Counter));
				}
				pSubFile->AddEmpty(HashUnknown, FileName, pLink->uShareEnd - pLink->uShareBegin, false);
			}
	
			if(!pSubParts)
			{
				pSubParts = pSubFile->SharePartMap();
				ASSERT(pSubParts);
				pSubFile->SetPartMap(CPartMapPtr(pSubParts));

				pParts->SetupLink(pLink->uShareBegin, pLink->uShareEnd, pSubFile->GetFileID());
				pSubParts->SetupLink(pLink->uShareBegin, pLink->uShareEnd, GetFileID());
			}

			if(!pSubFile)
			{
				pList->AddFile(pSubFile);
				pSubFile->Resume();

				pSubFile->SetHalted(true);
			}
			//

			pParts->EnableLink(pLink->ID, pSubFile->GetPartMapPtr());			
			pSubParts->EnableLink(FileID, GetPartMapPtr());

			if(pSubFile->IsIncomplete())
				InCompleteCount++;
		}

		if(IsComplete() && InCompleteCount > 0)
		{
			LogLine(LOG_WARNING, tr("Multi File %1 is not longer completed").arg(m_FileName));
			SelectMasterHash();
		}

		if(InCompleteCount > 0)
		{
			for(int i=0; i < Links.size(); i++)
			{
				SPartMapLink* pLink = *(--Links.end() - i); // Note: the map we pul the values form is reversly sorted by file offset

				CFile* pSubFile = GetList()->GetFileByID(pLink->ID);
				ASSERT(pSubFile);
				if(pSubFile && pSubFile->IsIncomplete() && (!pSubFile->GetMasterHash() || !pSubFile->GetMasterHash()->IsValid()))
				{
					CFileHashPtr pHash = CFileHashPtr(new CFileHash(m_pMasterHash->GetType()));
					pHash->SetHash(m_pMasterHash->GetHash());
					pSubFile->SetMasterHash(pHash);
				}
			}
		}
	}

	// Note: archive files dont have any IO
#ifndef NO_HOSTERS
	if(!(IsRawArchive() && !IsMultiFile()) && !IsRemoved() && !MetaDataMissing())
#else
	if(!IsRemoved() && !MetaDataMissing())
#endif
		OpenIO();

	if(m_State != eStopped && !IsHalted() && !IsRemoved() && !HasError())
		Enable(); 
}

bool CFile::OpenIO()
{
	ASSERT(!IsRemoved());
	ASSERT(!IsPending() && !MetaDataMissing());
	ASSERT(m_FileSize > 0);

	foreach(CFileHashPtr pHash, m_HashMap)
	{
		if(CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(pHash.data()))
		{
			if(pHash->IsValid() && !pHashEx->IsLoaded())
				theCore->m_Hashing->LoadHash(pHash.data());
		}
	}

	int iModified = 0;
	if (CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
	{
		uint64 FileID = GetFileID();
		ASSERT(FileID);

		QList<CIOManager::SMultiIO> SubFiles;
		QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();
		for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin();)
		{
			SPartMapLink* pLink = *(--I);

			CFile* pSubFile = GetList()->GetFileByID(pLink->ID);
			if(!pSubFile || pSubFile->IsRemoved())
			{
				LogLine(LOG_ERROR, tr("Multi File %1 is missing sub file %2!").arg(GetFileName()).arg(pSubFile ? pSubFile->GetFileName() : tr("Unknown File")));
				SetError("SubFile Missing");
				return false;
			}

			if(!IsComplete() && QFile::exists(m_FilePath) && HasProperty("LastModified") && pSubFile->CheckModification(true))
				iModified ++;

			SubFiles.append(CIOManager::SMultiIO(pLink->uShareEnd - pLink->uShareBegin, pLink->ID));
		}
		theCore->m_IOManager->InstallIO(GetFileID(), m_FilePath, &SubFiles);
	}
	else
	{
		if(!QFile::exists(m_FilePath) && HasProperty("LastModified"))
		{
			ASSERT(IsIncomplete()); // Note: we should not have called resume on a missing file
			LogLine(LOG_WARNING, tr("Incomplete File %1 has ben deleted and is being recreated").arg(m_FileName));
			PurgeFile();
			SetProperty("LastModified", QVariant());
		}
		else if(CheckModification(true))
			iModified = 1;
		
		theCore->m_IOManager->InstallIO(GetFileID(), m_FilePath);

		if(m_State == eStarted && IsIncomplete())
			StartAllocate();
	}

	if(IsComplete())
	{
		ProtectIO();
		if(theCore->Cfg()->GetBool("Content/CalculateHashes"))
			CalculateHashes(); // this adds missing hashes and issues hashing of incomplete onse
	}
	else if(iModified > 0)
	{
		LogLine(LOG_WARNING, tr("Incomplete File %1 was modifyed and is being rehashed").arg(m_FileName));
		Rehash();
	}
	return true;
}

void CFile::ProtectIO(bool bSetReadOnly)
{
	theCore->m_IOManager->ProtectIO(GetFileID(), bSetReadOnly);
}

void CFile::PurgeFile()
{
	if(m_Parts)
		m_Parts->SetRange(0, -1, Part::NotAvailable);

	// clear all status
	foreach(const CFileHashPtr& pHash, GetAllHashes())
	{
		if(CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(pHash.data()))
			pHashEx->ResetStatus();
	}
}

void CFile::Suspend()
{
	Hold();
	CloseIO();
	Disable();
}

void CFile::CloseIO()
{
	if(!theCore->m_IOManager->HasIO(GetFileID()))
		return;
	
	theCore->m_IOManager->UninstallIO(GetFileID());
	if(QFile::exists(m_FilePath))
		SetProperty("LastModified", QFileInfo(m_FilePath).lastModified());

	foreach(CFileHashPtr pHash, m_HashMap)
	{
		if(CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(pHash.data()))
			pHashEx->Unload();
	}
}

bool CFile::CheckModification(bool bDateOnly)
{
	QFileInfo FileInfo(m_FilePath);
	int IgnoreLastModified = theCore->Cfg()->GetInt("Content/IgnoreLastModified");
	if((IgnoreLastModified == 0 || (IgnoreLastModified == 2 && IsIncomplete())) && HasProperty("LastModified"))
	{
		time_t LastKnown = GetProperty("LastModified").toDateTime().toTime_t();
		time_t LastModified = FileInfo.lastModified().toTime_t();
		if(LastKnown != LastModified)
			return true;
	}

	QFile File(m_FilePath);
	File.open(QFile::ReadOnly);
	uint64 uFileSize = File.size(); // Note QFileInfo fails on *.lnk files
	if(!bDateOnly && m_FileSize != uFileSize)
		return true;

	return false;
}

bool CFile::CheckDeleted()
{
#ifndef NO_HOSTERS
	if(IsRawArchive())
		return false;
#endif

	if(CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
	{
		// Note: all files of a multi file mist be deleted to considder it deleted
		CFileList* pList = GetList();
		foreach(SPartMapLink* pLink, pParts->GetLinks())
		{
			if(CFile* pSubFile = pList->GetFileByID(pLink->ID))
			{
				if(QFile::exists(pSubFile->GetFilePath()))
					return false;
			}
		}
	}
	else if(QFile::exists(m_FilePath))
		return false;
	return true;
}

void CFile::Rehash()
{
	ASSERT(IsIncomplete());

	ASSERT(m_Parts);
	m_Parts->SetRange(0, -1, Part::Cached | Part::Verified, CPartMap::eClr);

	ASSERT(m_Inspector);
	m_Inspector->StartValidation(true);
}

bool CFile::Remove(bool bDelete, bool bDeleteComplete, bool bForce, bool bCleanUp)
{
#ifndef NO_HOSTERS
	if(IsDuplicate() || IsRawArchive())
#else
	if(IsDuplicate())
#endif
		bDelete = true;

	CFileList* pList = GetList();

	bool bAliveMultiFiles = false;
	QList<CFile*> MultiFileList;
	if(CSharedPartMap* pSharedParts = qobject_cast<CSharedPartMap*>(m_Parts.data()))
	{	
		foreach(SPartMapLink* pLink, pSharedParts->GetLinks())
		{
			if(CFile* pMultiFile = pList->GetFileByID(pLink->ID))
			{
				MultiFileList.append(pMultiFile);
				if(!pMultiFile->IsRemoved())
					bAliveMultiFiles = true;
			}
		}
	}

	// SubFiles cant be phsically removed
	if(bDelete && !bForce && !MultiFileList.isEmpty())
	{
		if(!bAliveMultiFiles)
		{
			foreach(CFile* pMultiFile, MultiFileList)
				pMultiFile->Remove(true);
			MultiFileList.clear();
			bAliveMultiFiles = false;
		}
		else
		{
			bDelete = false;
			QStringList Nams;
			foreach(CFile* pMultiFile, MultiFileList)
				Nams.append(pMultiFile->GetFileName());
			LogLine(LOG_WARNING, tr("Sub files can not be removed, remove multifile(s) first: '%1'").arg(Nams.join("', '")));
		}
	}

	if(pList && !IsDuplicate() && !IsRemoved() && MultiFileList.isEmpty())
	{
		foreach(CFile* pFoundFile, pList->FindDuplicates(this))
		{
			if(pFoundFile->m_Status == eDuplicate)
			{
				pFoundFile->m_Status = eComplete;
				m_Status = eDuplicate;
				bDelete = true;
				break;
			}
		}
	}

	// set file to removed
	if(m_Status != eRemoved)
	{
		Hold(true);

		if(m_Status != ePending && !m_FilePath.isEmpty())
		{
			CloseIO();
			if(QFile::exists(m_FilePath) && !IsMultiFile() && (bDeleteComplete || !IsComplete())) // delete only incomplete files or if user asked for it
			{
				if(!SafeRemove(m_FilePath))
					LogLine(LOG_ERROR, tr("File %1 could not be removed").arg(m_FilePath));
			}
#ifndef NO_HOSTERS
			CArchiveDownloader::PurgeTemp(this);
#endif
		}
		else
			bCleanUp = false;

		m_Status = eRemoved;
		Disable();
	}
	else
		bCleanUp = false;

	// CleanUp PartMap and Links
	if(!m_Parts.isNull())
	{
		uint64 ID = GetFileID();

		// Cleanup SubFiles
		if(CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
		{
			foreach(SPartMapLink* pLink, pJoinedParts->GetLinks())
			{
				CFile* pSubFile = pList->GetFileByID(pLink->ID); // remove a sub file that are incomplete, and dont have a master hash
				if(pSubFile && pSubFile->IsIncomplete() && !pSubFile->CompareHash(pSubFile->GetMasterHash().data()))
					pSubFile->Remove(bDelete, false, true, false); // force delete must be set as the part maps are yet not unlinked
			}
		}

		// mark multi files as broken
		if(CSharedPartMap* pSharedParts = qobject_cast<CSharedPartMap*>(m_Parts.data()))
		{
			foreach(SPartMapLink* pLink, pSharedParts->GetLinks())
			{
				CFile* pMultiFile = pList->GetFileByID(pLink->ID);
				if(pMultiFile && !pMultiFile->IsRemoved()) // if the multifile is not a removed one, set it into an error state and close the IO
				{
					pMultiFile->LogLine(LOG_ERROR, tr("SubFile %1 has been removed from Multi File %2!").arg(GetFileName()).arg(pMultiFile->GetFileName()));
					pMultiFile->SetError("SubFile Missing");
					pMultiFile->CloseIO();
				}
			}
		}

		if(bDelete) // if we are fully removing this file we must break all links
		{
			if(CLinkedPartMap* pLinkedParts = qobject_cast<CLinkedPartMap*>(m_Parts.data())) // if the file is being deleted break of the links
			{
				foreach(SPartMapLink* pLink, pLinkedParts->GetLinks())
				{
					if(CLinkedPartMap* pParts = qobject_cast<CLinkedPartMap*>(pLink->pMap.data()))
						pParts->BreakLink(ID);
					pLinkedParts->BreakLink(pLink->ID);
				}
			}
			m_Parts.clear();
		}
		else
			m_Parts->SetRange(0, -1, Part::NotAvailable); // cleanup
	}

	if(bCleanUp)
	{
		QStringList Shared = theCore->Cfg()->GetStringList("Content/Shared");
		Shared.append(theCore->GetIncomingDir(false));
		Shared.append(theCore->GetTempDir(false));
		QString Root;
		QString Relative = GetRelativeSharedPath(m_FilePath, Shared, Root);
		QString Path = Root + Split2(Relative, "/").first;
		DeleteDir(Path, true);
	}

	if(m_Downloader)
	{
		delete m_Downloader;
		m_Downloader = NULL;
	}

	if(m_Inspector)
	{
		delete m_Inspector;
		m_Inspector = NULL;
	}

	// if we are removing this file totaly we have to delete the torrent file from repository as well
	if(bDelete)
	{
		foreach(CTorrent* pTorrent, m_Torrents)
		{
			if(pTorrent)
				pTorrent->RemoveTorrentFile();
		}
	}

	if(bDelete && pList)
		pList->RemoveFile(this); // this will delete this
	return bDelete;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// File properties
//

bool CFile::IsMultiFile()
{
	if(m_Parts)
		return m_Parts->inherits("CJoinedPartMap");
	if(m_HashMap.contains(HashXNeo))
		return true;
	return false; // files without a part map are by definition single files
}

bool CFile::IsSubFile()
{
	if(CSharedPartMap* pParts = qobject_cast<CSharedPartMap*>(m_Parts.data()))
		return !pParts->GetLinks().isEmpty();
	return false;
}

bool CFile::MetaDataMissing()
{
	// Note: A incomplate file must have a part map, only compelte single files can have none
	return IsIncomplete() && m_Parts.isNull();
}

QList<uint64> CFile::GetSubFiles()
{
	QList<uint64> Files;
	if(CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
	{
		QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();
		for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin();)
		{
			SPartMapLink* pLink = *(--I);
			Files.append(pLink->ID);
		}
	}
	return Files;
}

QList<uint64> CFile::GetParentFiles()
{
	QList<uint64> Files;
	if(CSharedPartMap* pParts = qobject_cast<CSharedPartMap*>(m_Parts.data()))
	{
		foreach(SPartMapLink* pLink, pParts->GetLinks())
			Files.append(pLink->ID);
	}
	return Files;
}

CSharedPartMap* CFile::SharePartMap()
{
	CPartMap* pMap = GetPartMap();
	if(pMap && pMap->inherits("CJoinedPartMap"))
	{
		ASSERT(0);
		return NULL;
	}

	if(GetFileSize() == 0)
		return NULL;

	CSharedPartMap* pSubParts = qobject_cast<CSharedPartMap*>(GetPartMap());
	if(!pSubParts) // replace the normal part map with a shared one
	{
		pSubParts = new CSharedPartMap(GetFileSize());
		if(pMap)
			pSubParts->Merge(pMap/*, 0, CPartMap::ESet*/); // default add is same as set for a ampty map
		else
			pSubParts->SetRange(0, -1, Part::Available | Part::Verified);
		SetPartMap(CPartMapPtr(pSubParts));
	}
	return pSubParts;
}

bool CFile::ReplaceSubFile(CFile* pOldFile, CFile* pNewFile)
{
	CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data());
	ASSERT(pParts);
	foreach(SPartMapLink* pLink, pParts->GetLinks())
	{
		if(pLink->ID == pOldFile->GetFileID())
		{
			if(pNewFile->GetFileSize() != pLink->uShareEnd - pLink->uShareBegin)
			{
				ASSERT(0);
				return false;
			}

			if(CSharedPartMap* pSubParts = pNewFile->SharePartMap())
			{
				pParts->BreakLink(pOldFile->GetFileID());

				pParts->SetupLink(pLink->uShareBegin, pLink->uShareEnd, pNewFile->GetFileID());

				pParts->EnableLink(pLink->ID, pNewFile->GetPartMapPtr());
				pSubParts->EnableLink(GetFileID(), GetPartMapPtr());
				return true;
			}
		}
	}
	ASSERT(0);
	return false;
}

void CFile::AddHash(CFileHashPtr pFileHash)
{
	if(pFileHash->GetType() == HashTorrent)
	{
		if(!m_Torrents.contains(pFileHash->GetHash()))
		{
			CTorrent* &pTorrent = m_Torrents[pFileHash->GetHash()];
			pTorrent = new CTorrent(this);
			pTorrent->AddEmptyTorrent(m_FileName, pFileHash->GetHash());
		}
	}
#ifndef NO_HOSTERS
	else if(pFileHash->GetType() == HashArchive)
	{
		if(!m_Archives.contains(pFileHash->GetHash().left(ARCH_PREFIX_LEN)))
			m_Archives.insert(pFileHash->GetHash().left(ARCH_PREFIX_LEN), new CArchiveSet(this, pFileHash));
	}
#endif
	else
	{
		CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(pFileHash.data());
		//ASSERT(pHashEx); // Note: the actual hash map must cotain only proper unique hashing capable hashes
		if(pHashEx && (m_UpLimit || m_DownLimit) && pFileHash->IsValid() && !pHashEx->IsLoaded())
			theCore->m_Hashing->LoadHash(pFileHash.data());

		m_HashMap.insert(pFileHash->GetType(), pFileHash);
	}
}

CFileHashPtr CFile::GetHashPtr(EFileHashType Type)
{
	ASSERT(Type != HashTorrent && Type != HashArchive);
	CFileHashPtr pHash = m_HashMap.value(Type);
	if(pHash && !pHash->inherits("CFileHashEx"))
		return CFileHashPtr();
	return pHash;
}

CFileHashPtr CFile::GetHashPtrEx(EFileHashType Type, const QByteArray& Hash)
{
	if(Type == HashTorrent) // torrent hashes are not unique and those storred separatly
	{
		foreach(CTorrent* pTorrent, m_Torrents)
		{
			if(!pTorrent)
				continue;

			if(Hash.isEmpty() || pTorrent->GetInfoHash() == Hash)
				return pTorrent->GetHash();
		}
	}
#ifndef NO_HOSTERS
	else if(Type == HashArchive)
	{
		foreach(CArchiveSet* pArchive, m_Archives)
		{
			if(Hash.isEmpty() || pArchive->GetHash()->GetHash() == Hash)
				return pArchive->GetHash();
		}
	}
#endif
	else if(CFileHashPtr pHash = m_HashMap.value(Type))
	{
		if(Hash.isEmpty() || pHash->GetHash() == Hash)
			return pHash;
	}
	return CFileHashPtr();
}

QList<CFileHashPtr> CFile::GetHashes(EFileHashType Type)
{
	QList<CFileHashPtr> Hashes;
	if(Type == HashTorrent)
	{
		foreach(CTorrent* pTorrent, m_Torrents)
		{
			if(!pTorrent)
				continue;
			if(!pTorrent->GetInfoHash().isEmpty()) // return only actually allocated torrents - empty infohash is only when creating a torrent
				Hashes.append(pTorrent->GetHash());
		}
	}
#ifndef NO_HOSTERS
	else if(Type == HashArchive)
	{
		foreach(CArchiveSet* pArchive, m_Archives)
			Hashes.append(pArchive->GetHash());
	}
#endif
	else if(CFileHashPtr pHash = m_HashMap.value(Type))
		Hashes.append(pHash);
	return Hashes;
}

QList<CFileHashPtr> CFile::GetAllHashes(bool bAll)
{
	QList<CFileHashPtr> Hashes = m_HashMap.values();

	for(QMap<QByteArray, CTorrent*>::iterator I = m_Torrents.begin(); I != m_Torrents.end(); I++)
	{
		if(!I.value())
		{
			if(bAll)
			{
				CFileHashPtr pHash(new CFileHash(HashTorrent));
				pHash->SetHash(I.key());
				Hashes.append(pHash);
			}
			continue;
		}

		if(!I.value()->GetInfoHash().isEmpty()) // return only actually allocated torrents - empty infohash is only when creating a torrent
			Hashes.append(I.value()->GetHash());
	}

#ifndef NO_HOSTERS
	if(bAll)
	{
		foreach(CArchiveSet* pArchive, m_Archives)
			Hashes.append(pArchive->GetHash());
	}
#endif

	return Hashes;
}

bool CFile::CompareHash(const CFileHash* pFileHash)
{
	if(!pFileHash)
		return false;

	if(pFileHash->GetType() == HashTorrent)
	{
		if(m_Torrents.contains(pFileHash->GetHash()))
			return true;
	}
#ifndef NO_HOSTERS
	else if(pFileHash->GetType() == HashArchive)
	{
		if(m_Archives.contains(pFileHash->GetHash().left(ARCH_PREFIX_LEN)))
			return true;
	}
#endif
	else if(m_HashMap.contains(pFileHash->GetType()))
		return m_HashMap.value(pFileHash->GetType())->Compare(pFileHash);
	return false;
}

void CFile::Purge(CFileHashPtr pFileHash)
{
	CTorrent* pTorrent = (pFileHash->GetType() == HashTorrent) ? m_Torrents.value(pFileHash->GetHash()) : NULL;
	foreach(CTransfer* pTransfer, m_Transfers)
	{
		if(pFileHash->Compare(pTransfer->GetHash().data()))
			RemoveTransfer(pTransfer);
	}
}

void CFile::DelHash(CFileHashPtr pFileHash)
{
	Purge(pFileHash);

	if(pFileHash->GetType() == HashTorrent)
		delete m_Torrents.take(pFileHash->GetHash());
#ifndef NO_HOSTERS
	else if(pFileHash->GetType() == HashArchive)
	{
		if(CArchiveSet* pArchive = m_Archives.take(pFileHash->GetHash().left(ARCH_PREFIX_LEN)))
		{
			pArchive->CleanUp();
			delete pArchive;
		}
	}
#endif
	else
		m_HashMap.remove(pFileHash->GetType());
}

bool CFile::SelectMasterHash()
{
	if(GetFileSize() == 0)
		return false;

	EFileHashType HashPrio[3] = {HashNeo,HashXNeo,HashEd2k};
	for(int i=0; i<ARRSIZE(HashPrio); i++)
	{
		if(CFileHashPtr pHash = GetHashPtr(HashPrio[i]))
		{
			m_pMasterHash = pHash;
			return true;
		}
	}

	foreach(CTorrent* pTorrent, m_Torrents)
	{
		if(pTorrent && !pTorrent->GetInfoHash().isEmpty())
		{
			m_pMasterHash = pTorrent->GetHash();
			return true;
		}
	}

#ifndef NO_HOSTERS
	if(!m_Archives.isEmpty())
	{
		m_pMasterHash = m_Archives.values().first()->GetHash();
		return true;
	}
#endif

	return false;
}

int CFile::IsAutoShare()
{
	return GetProperty("AutoShare", 2).toInt();
}

int CFile::IsTorrent()
{
	return GetProperty("Torrent").toInt();
}

bool CFile::IsEd2kShared()
{
	return GetProperty("Ed2kShare").toBool();
}

bool CFile::IsNeoShared()
{
	return GetProperty("NeoShare").toBool();
}

#ifndef NO_HOSTERS
int CFile::IsHosterDl()
{
	return GetProperty("HosterDl").toInt();
}

bool CFile::IsHosterUl()
{
	return GetProperty("HosterUl").toBool();
}

bool CFile::IsArchive()
{
	return !m_pMasterHash.isNull() && !m_Archives.isEmpty() && (m_pMasterHash->GetType() == HashArchive || IsHosterDl() == 3);
}

bool CFile::IsRawArchive()
{
	if(!(m_pMasterHash && m_pMasterHash->GetType() == HashArchive) && IsHosterDl() != 3)
		return false;
	if(m_HashMap.contains(HashNeo) || m_HashMap.contains(HashXNeo))
		return false;
	return m_Parts.isNull();
}
#endif

void CFile::SetProperty(const QString& Name, const QVariant& Value)
{
	if(Name == "QueuePos")
		return SetQueuePos(Value.toInt());

	if(Name == "Torrent")
	{
		bool bOne = false;
		bool bStart = false;
		if(Value.toInt() == 0 || (bOne = (Value.toInt() == 2)))
		{
			CTorrent* pTopTorrent = bOne ? GetTopTorrent() : NULL;
			for(QMap<QByteArray, CTorrent*>::iterator I = m_Torrents.begin(); I != m_Torrents.end(); )
			{
				CTorrent* &pTorrent = I.value();
				if(!pTorrent || pTorrent == pTopTorrent)
				{
					I++;
					continue;
				}

				CFileHashPtr pHash = pTorrent->GetHash();
				Purge(pTorrent->GetHash());
				delete pTorrent;

				if(m_Inspector || !pHash->IsValid())
				{
					I = m_Torrents.erase(I);
					if(m_Inspector && pHash->IsValid())
						m_Inspector->AddAuxHash(pHash);
				}
				else
				{
					pTorrent = NULL;
					I++;
				}
			}

			// we wanted one torrent we have none, start one
			if(bOne && !pTopTorrent)
				bStart = true;
		}
		else
			bStart = true;

		if(bStart)
		{
			for(QMap<QByteArray, CTorrent*>::iterator I = m_Torrents.begin(); I != m_Torrents.end(); I++)
			{
				CTorrent* &pTorrent = I.value();
				if(pTorrent)
					continue;

				pTorrent = new CTorrent(this);
				pTorrent->LoadTorrentFromFile(I.key());

				if(bOne)
					break;
			}

			if(m_Inspector)
				m_Inspector->Update();
		}
	}

	if(Value.isValid()) 
		m_Properties.insert(Name, Value); 
	else 
		m_Properties.remove(Name);

	if(Name == "Ed2kShare" && Value.toBool() == true && IsComplete()) 
	{
		// Update hashes if needed
		if(theCore->m_IOManager->HasIO(GetFileID()))
		{
			if(theCore->Cfg()->GetBool("Content/CalculateHashes"))
				CalculateHashes();
		}
	}

	if(Name == "Priority" || Name == "Upload" || Name == "Download")
		UpdateBC();

	if(Name == "Description" || Name == "Rating")
		SetProperty("RatingExpiration", QVariant());

	if(Name == "Priority" || Name == "Stream")
		UpdateSelectionPriority();
}

QVariant CFile::GetProperty(const QString& Name, const QVariant& Default)
{
	if(Name == "QueuePos")
		return GetQueuePos();

	return m_Properties.value(Name, Default);
}

void CFile::SetQueuePos(int QueuePos, bool bAdjust)
{
	m_QueuePos = QueuePos;

	if(!bAdjust)
		return;

	// ensure the quele sorting is right and unique
	if(CFileList* pList = GetList())
	{
		QMap<int, CFile*> Files; // all files except this one, sorted by queue position
		foreach(CFile* pFile, pList->GetFiles())
		{
			if(pFile == this)
				continue;
			Files.insert(pFile->GetQueuePos(), pFile);
		}

		QList<int> Queue; // we find each file we need to move one palce down
		for(; Files.contains(QueuePos); QueuePos++)
			Queue.prepend(QueuePos);
		foreach(int Pos, Queue) // and than we move them down in the right order
			Files[Pos]->m_QueuePos = Pos + 1;
	}
}

void CFile::UpdateSelectionPriority()
{
	if(!m_Parts)
		return;

	// Note: part priority has no meaning for a multi file file, so it most always be cleared
	if(IsMultiFile())
	{
		m_Parts->SetRange(0, -1, Part::Stream, CPartMap::eClr);
		m_Parts->SetRange(0, -1, SPartRange::Priority(0), CPartMap::eSet);
	}
	else
	{
		int iPriority = GetProperty("Priority", 0).toInt(); // 1 - 9
		if(!iPriority)
			iPriority = 5;

		bool bStream = GetProperty("Stream").toBool();
		bool bPreview = false;
		if(theCore->Cfg()->GetBool("Content/PreparePreview"))
			bPreview = GetProperty("Streamable", false).toBool();

		if(bStream)
			m_Parts->SetRange(0, -1, SPartRange::Priority(iPriority + 4), CPartMap::eSet); // make sure streaming priority is always higher than just priority 14^2 = 196
		else
		{
			m_Parts->SetRange(0, -1, SPartRange::Priority(iPriority), CPartMap::eSet); // prio is ^2

			if(bPreview)
			{
				if(uint64 FrontLoadSize = theCore->Cfg()->GetInt("Content/FrontLoadSize"))
				{
					uint64 FileSize = m_Parts->GetSize();
					uint64 ToFrontLoad = Min(FrontLoadSize, FileSize/10);
	
					m_Parts->SetRange(0, ToFrontLoad, SPartRange::Priority(iPriority + 4), CPartMap::eSet);
				}
			}
		}

		if(bStream || bPreview)
		{
			if(uint64 BackLoadSize = theCore->Cfg()->GetInt("Content/BackLoadSize"))
			{
				uint64 FileSize = m_Parts->GetSize();
				uint64 ToBackLoad = Min(BackLoadSize, FileSize/20);
				uint64 BackStart = FileSize - ToBackLoad;
	
				m_Parts->SetRange(BackStart, FileSize, SPartRange::Priority(iPriority + 5), CPartMap::eSet);
			}
		}

		if(bStream)
			m_Parts->SetRange(0, -1, Part::Stream, CPartMap::eAdd);
		else
			m_Parts->SetRange(0, -1, Part::Stream, CPartMap::eClr);
	}

	m_Parts->NotifyChange();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// File Status
//

void CFile::Merge(CFile* pFile)
{
	SetProperty("CompletedTime", pFile->GetProperty("CompletedTime"));

	m_UploadedBytes += pFile->m_UploadedBytes;
	m_DownloadedBytes = pFile->m_DownloadedBytes;
}

bool CFile::CheckDuplicate()
{
	ASSERT(IsComplete());

	CFileList* pList = GetList();
	if(!pList)
		return false;

	QList<CFile*> Files = pList->FindDuplicates(this);
	if(!Files.isEmpty())
	{
		CFile* pFoundFile = NULL;
		foreach(CFile* pFile, Files)
		{
			if(pFile->GetFilePath() == GetFilePath())
			{
				pFoundFile = pFile;
				break;
			}
		}

		if(!pFoundFile)
			pFoundFile = Files.at(0);
		if(pFoundFile->IsRemoved())
		{
			pFoundFile->LogLine(LOG_INFO, tr("Resumed known File %1 from new file %2").arg(pFoundFile->GetFileName()).arg(GetFileName()));

			pFoundFile->UnRemove();
			pFoundFile->SetProperty("LastModified", GetProperty("LastModified"));
			pFoundFile->SetFilePath(m_FilePath);
			pFoundFile->SetFileName(m_FileName);
			pFoundFile->Resume();

			// this file is a sub file -  we have to update all masters to the new file
			if(CSharedPartMap* pSharedParts = qobject_cast<CSharedPartMap*>(m_Parts.data()))
			{
				foreach(SPartMapLink* pLink, pSharedParts->GetLinks())
				{
					if(CFile* pMultiFile = pList->GetFileByID(pLink->ID))
					{
						pFoundFile->Merge(this);
						pMultiFile->ReplaceSubFile(this, pFoundFile);
					}
					pSharedParts->BreakLink(pLink->ID);
				}
			}

			// if the found file was a removed sub file we check if there ara masters we can clear from error
			if(CSharedPartMap* pSharedParts = qobject_cast<CSharedPartMap*>(pFoundFile->GetPartMap()))
			{
				foreach(SPartMapLink* pLink, pSharedParts->GetLinks())
				{
					if(CFile* pMultiFile = pList->GetFileByID(pLink->ID))
					{
						bool bAllOK = true;

						foreach(uint64 FileID, pMultiFile->GetSubFiles())
						{
							CFile* pSubFile = pList->GetFileByID(FileID);
							if(!pSubFile || pSubFile->IsRemoved())
							{
								bAllOK = false;
								break;
							}
						}

						if(bAllOK)
							pMultiFile->Start(false, true);
					}
				}
			}

			m_FilePath = ""; // prevetn deleting file on disc
			m_Status = eDuplicate;
			return true; // please delete this
		}
		
		LogLine(LOG_WARNING, tr("Found File duplicate, file %1 is already known as %2").arg(GetFileName()).arg(pFoundFile->GetFileName()));
		if(pFoundFile->IsIncomplete())
			pFoundFile->SetError("Already Downloaded");
		else if(!pFoundFile->IsDuplicate())
			m_Status = eDuplicate;
	}
	return false;
}

uint64 CFile::GetStatusStats(uint32 eStatus, bool bInverse)
{
#ifndef NO_HOSTERS
	if(IsArchive())
	{
		uint64 uLength = 0;
		switch(eStatus)
		{
			case Part::Available:	uLength = CArchiveDownloader::GetDownloadedSize(this); break;
			case Part::Verified:	uLength = theCore->m_ArchiveDownloader->GetProgress(this); break;
		}
		return bInverse ? GetFileSize() - uLength : uLength;
	}
#endif

	if(eStatus == Part::NotAvailable)
	{
		if(CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
		{
			uint64 uAllocated = 0;
			bool bAllocating = false;
			foreach(SPartMapLink* pLink, pParts->GetLinks())
			{
				if(!bAllocating && theCore->m_IOManager->IsAllocating(pLink->ID))
					bAllocating = true;
				uAllocated += theCore->m_IOManager->GetSize(pLink->ID);
			}
			if(bAllocating)
				return uAllocated;
		}
		else if(theCore->m_IOManager->IsAllocating(GetFileID()))
			return theCore->m_IOManager->GetSize(GetFileID());
		return 0;
	}

	if(!m_Parts)
		return 0;

	int tmp = bInverse ? -((int)eStatus) : (int)(eStatus);
	SStatusStat& StatusStat = m_StatusStats[tmp];
	if(StatusStat.uRevision != m_Parts->GetRevision() && StatusStat.uInvalidate < GetCurTick())
	{
		StatusStat.uLength = m_Parts->CountLength(eStatus, bInverse);
		StatusStat.uRevision = m_Parts->GetRevision();
		StatusStat.uInvalidate = GetCurTick() + SEC2MS(3);
	}
	return StatusStat.uLength;
}

bool CFile::IsAllocating()
{
	if(CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(m_Parts.data()))
	{
		foreach(SPartMapLink* pLink, pParts->GetLinks())
		{
			if(theCore->m_IOManager->IsAllocating(pLink->ID))
				return true;
		}
		return false;
	}
	return theCore->m_IOManager->IsAllocating(GetFileID());
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Transfers
//

void CFile::AddTransfer(CTransfer* pTransfer)
{
	pTransfer->SetFile(this); 
	m_Transfers.insert(pTransfer);

	m_FileStats->AddTransfer(pTransfer);
}

void CFile::RemoveTransfer(CTransfer* pTransfer, bool bDelete)
{
	pTransfer->Hold(bDelete);
	m_FileStats->RemoveTransfer(pTransfer);
	m_Transfers.remove(pTransfer);
	pTransfer->SetFile(NULL);
	if(bDelete)
		delete pTransfer;
}

CTransfer* CFile::GetTransfer(uint64 ID)
{
	foreach(CTransfer* pTransfer, m_Transfers)
	{
		if((uint64)pTransfer == ID)
			return pTransfer;
	}
	return NULL;
}

void CFile::CancelRequest(uint64 uBegin, uint64 uEnd)
{
	foreach(CTransfer* pTransfer, m_Transfers)
	{
		if(!pTransfer->IsActiveDownload())
			continue;

		if(CP2PTransfer* pP2PTransfer = qobject_cast<CP2PTransfer*>(pTransfer))	
			pP2PTransfer->CancelRequest(uBegin, uEnd);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Load/Store
//

QVariantMap CFile::Store()
{
	QVariantMap File;
	File["ID"] = GetFileID();

	File["FileName"] = m_FileName;
	File["FileDir"] = m_FileDir;
	File["FileSize"] = m_FileSize;

	File["FilePath"] = m_FilePath;

	QVariantMap HashMap;
	foreach(EFileHashType Type, m_HashMap.uniqueKeys())
	{
		CFileHash* pHash = m_HashMap.value(Type).data();
		if(pHash->IsValid())
			HashMap[CFileHash::HashType2Str(Type)] = QString(pHash->ToString());
	}
	File["HashMap"] = HashMap;

	QVariantList Torrents;
	for(QMap<QByteArray, CTorrent*>::iterator I = m_Torrents.begin(); I != m_Torrents.end(); I++)
	{
		QVariantMap Torrent;
		Torrent["InfoHash"] = I.key().toHex();
		Torrent["Enabled"] = I.value() != NULL;
		// Z-ToDo-Now: add more torrent info like upload/download
		Torrents.append(Torrent);
	}
	File["Torrents"] = Torrents;

#ifndef NO_HOSTERS
	QVariantList Archives;
	for(QMap<QByteArray, CArchiveSet*>::iterator I = m_Archives.begin(); I != m_Archives.end(); I++)
		Archives.append(I.value()->Store());
	File["Archives"] = Archives;
#endif

	if(!m_pMasterHash.isNull())
		File["MasterHash"] = CFileHash::HashType2Str(m_pMasterHash->GetType()) + ":" + QString(m_pMasterHash->ToString());

	File["Details"] = m_FileDetails->Store();

	//QVariantMap Properties;
	//foreach(const QByteArray& Name, dynamicPropertyNames())
	//	Properties[Name] = property(Name);
	File["Properties"] = m_Properties;

	File["QueuePos"] = m_QueuePos;

	if(!m_Parts.isNull())
		File["PartMap"] = m_Parts->Store();

	if(m_Inspector)
		File["Inspector"] = m_Inspector->Store();

	QVariantList Transfers;
	foreach(CTransfer* pTransfer, m_Transfers)
	{
		QVariantMap Transfer = pTransfer->Store();
		if(!Transfer.isEmpty()) // if its empty than the source didnt wanted to be storred
		{
			Transfer["Type"] = CTransfer::GetTypeStr(pTransfer);
			Transfers.append(Transfer);
		}
	}
	File["Transfers"] = Transfers;

	if(m_State == eStarted)
		File["State"] = "Started";
	else if(m_State == ePaused)
		File["State"] = "Paused";
	else if(m_State == eStopped)
		File["State"] = "Stopped";

	if(m_Status == eRemoved)
		File["Status"] = "Removed";
	else if(m_Status == eComplete)
		File["Status"] = "Complete";
	else if(m_Status == eIncomplete)
		File["Status"] = "Incomplete";
	else if(m_Status == eDuplicate)
		File["Status"] = "Duplicate";
	else if(m_Status == ePending)
		File["Status"] = "Pending";
	else{
		ASSERT(0);} // dont save other states;

	File["Halted"] = m_Halted;

	File["Uploaded"] = m_UploadedBytes;
	File["Downloaded"] = m_DownloadedBytes;

	return File;
}

bool CFile::Load(const QVariantMap& File)
{
	if(File.isEmpty())
		return false;

	if(quint64 ID = File["ID"].toULongLong())
	{
		SetFileID(ID);
		if(ID != GetFileID())
		{
			ASSERT(0); // this should never efer happen unless someone messed up his filelist.xml
			LogLine(LOG_ERROR, tr("File ID conflict, file %1 has now new ID %2").arg(GetFileName()).arg(GetFileID()));
		}
	}

	m_FileName = File["FileName"].toString();

	m_FileDir = File["FileDir"].toString();

	SetFileSize(File["FileSize"].toULongLong());

	QVariantMap HashMap = File["HashMap"].toMap();
	foreach(const QString& sType, HashMap.uniqueKeys())
	{
		EFileHashType Type = CFileHash::Str2HashType(sType);
		if(Type == HashNone)
			LogLine(LOG_WARNING, tr("file %1 contians an unknown hash type %2").arg(m_FileName, Type));
		else if(CFileHashPtr pHash = CFileHashPtr(CFileHash::FromString(HashMap.value(sType).toByteArray(), Type, GetFileSize())))
		{
			// Legacy: this load stage is for loading old file list without explicite torrent section
			/*if(Type == HashTorrent) 
			{
				CTorrent* pTorrent = new CTorrent(this);
				pTorrent->LoadTorrentFromFile(pHash->GetHash());
				m_Torrents.insert(pTorrent->GetInfoHash(), pTorrent);
			}
			else if(Type != HashArchive)*/
			AddHash(pHash);
		}
		else
		{
			CFileHash::FromString(HashMap.value(sType).toByteArray(), Type, GetFileSize());
			LogLine(LOG_WARNING, tr("file %1 contians an invalid hash value %2").arg(m_FileName, Type));
		}
	}

	foreach(const QVariant& vTorrent, File["Torrents"].toList())
	{
		QVariantMap Torrent = vTorrent.toMap();
		QByteArray InfoHash = QByteArray::fromHex(Torrent["InfoHash"].toByteArray());
		CTorrent* &pTorrent = m_Torrents[InfoHash];
		if(Torrent["Enabled"].toBool())
		{
			pTorrent = new CTorrent(this);
			pTorrent->LoadTorrentFromFile(InfoHash);
		}
	}

#ifndef NO_HOSTERS
	foreach(const QVariant& vArchive, File["Archives"].toList())
	{
		QVariantMap Archive = vArchive.toMap();
		CArchiveSet* pArchive = new CArchiveSet(this);
		if(!pArchive->Load(Archive) || m_Archives.contains(pArchive->GetID()))
			delete pArchive;
		else
			m_Archives.insert(pArchive->GetID(), pArchive);
	}
#endif

	if(File.contains("MasterHash"))
	{
		StrPair TypeHash = Split2(File["MasterHash"].toString(), ":");
		EFileHashType Type = CFileHash::Str2HashType(TypeHash.first);
		QByteArray HashValue = CFileHash::DecodeHash(Type, TypeHash.second.toLatin1());
		m_pMasterHash = GetHashPtrEx(Type, HashValue);
		if(m_pMasterHash.isNull()) // we dont have this hash
		{
			// Note: at this poitn we stil havnt populated tha archive ahsh list
			if(!HashValue.isEmpty()) // thats the case the master hash belongs to a parent file, or its an archive hash
			{
				m_pMasterHash = CFileHashPtr(new CFileHash(Type));
				m_pMasterHash->SetHash(HashValue);
			}
			else if(!SelectMasterHash()) // thats a legacy case - it must be a torrent a NeoX file woudl have neo hash
				m_pMasterHash = CFileHashPtr(new CFileHash(HashTorrent));
		}
	}

	m_FileDetails->Load(File["Details"].toMap());

	//QVariantMap Properties = File["Properties"].toMap();
	//foreach(const QString& Name, Properties.keys())
	//	setProperty(Name.toLatin1(), Properties[Name]);
	m_Properties = File["Properties"].toMap();

	m_QueuePos = File["QueuePos"].toInt();

	// Note: part map must be loaded befoure adding hashes!
	if(File.contains("PartMap"))
	{
		CPartMapPtr pParts = CPartMapPtr(CLinkedPartMap::Restore(File["PartMap"].toMap()));
		if(pParts->GetSize() != m_FileSize)
		{
			LogLine(LOG_WARNING, tr("file %1 contians an invalid part map").arg(m_FileName));
			pParts.clear();
		}
		SetPartMap(pParts);
		if(!m_Parts.isNull())
			m_Parts->SetRange(0, -1, Part::Cached /* | Part::Marked | Part::Scheduled | Part::Requested*/, CPartMap::eClr); // clear temporary status flags just (when saving all ware dumped)
		else
		{
			LogLine(LOG_WARNING, tr("file %1 contians an invalid part map").arg(m_FileName));
			SetError("Invalid PartMap Loaded");
		}
	}

	if(IsIncomplete())
	{
		m_Inspector = new CHashInspector(this);
		m_Inspector->Load(File["Inspector"].toMap());
	}

	QVariantList Transfers = File["Transfers"].toList();
	foreach(const QVariant& transfer, Transfers)
	{
		QVariantMap Transfer = transfer.toMap();

		if(CTransfer* pTransfer = CTransfer::New(Transfer["Type"].toString()))
		{
			pTransfer->SetFoundBy(eStored);
			pTransfer->setParent(this);
			if(pTransfer->Load(Transfer))
				AddTransfer(pTransfer);
			else
			{
				LogLine(LOG_ERROR, tr("failed to load transfer"));
				delete pTransfer;
			}
		}
		else
			LogLine(LOG_ERROR, tr("failed to load transfer type %1 is unsupported").arg(Transfer["Type"].toString()));
	}

	SetProperty("Streamable", (Split2(m_FileName, ".", true).second.indexOf(QRegExp(theCore->Cfg()->GetString("Content/Streamable")), Qt::CaseInsensitive) == 0));

	if(File["State"] == "Started")
		m_State = eStarted;
	else if(File["State"] == "Paused")
		m_State = ePaused;
	else if(File["State"] == "Stopped")
		m_State = eStopped;
	
	ASSERT(m_Status == eNone);
	if(File["Status"] == "Removed")
		m_Status = eRemoved;
	else if(File["Status"] == "Duplicate")
		m_Status = eDuplicate;
	else if(File["Status"] == "Pending")
		m_Status = ePending;
	else if(File["Status"] == "Complete")
		m_Status = eComplete;
	else if(File["Status"] == "Incomplete")
		m_Status = eIncomplete;
	else
		m_Status = m_pMasterHash ? eIncomplete : eComplete;

	m_Halted = File["Halted"].toBool();

	m_UploadedBytes = File["Uploaded"].toULongLong();
	m_DownloadedBytes = File["Downloaded"].toULongLong();

	if(!IsPending())
		SetFilePath(File["FilePath"].toString());

	return true;
}

void CFile::AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line)
{
	Line.AddMark((uint64)this);
	QObjectEx::AddLogLine(uStamp, uFlag, Line);
}