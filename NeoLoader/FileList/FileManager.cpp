#include "GlobalHeader.h"
#include "FileManager.h"
#include "File.h"
#include "../FileTransfer/Transfer.h"
#include "../../Framework/Xml.h"
#include "../NeoCore.h"
#include "./Hashing/HashingThread.h"
#include "../../Framework/Cryptography/SymmetricKey.h"
#include "../../Framework/Cryptography/HashFunction.h"
#include "../../Framework/qzlib.h"
#include "../FileTransfer/BitTorrent/TorrentInfo.h"
#include "../FileTransfer/BitTorrent/Torrent.h"
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/HosterLink.h"
#include "../FileTransfer/HosterTransfer/WebManager.h"
#endif
#include "../FileTransfer/FileGrabber.h"
#include "../FileTransfer/DownloadManager.h"
#include "../FileTransfer/HashInspector.h"

CFileManager::CFileManager(QObject* qObject)
 : CFileList(qObject)
{
	m_LastSave = 0;
}

void CFileManager::Resume()
{
	LoadFromFile();

	QHash<QString, CFile*> PathMap;
	foreach(CFile* pFile, m_FileMap)
	{
#ifdef WIN32
		PathMap.insert(pFile->GetFilePath().toLower(), pFile);
#else
		PathMap.insert(pFile->GetFilePath(), pFile);
#endif
	}
	
	QStringList FilePaths = FindSharedFiles();
	foreach(const QString& FilePath, FilePaths)
	{
#ifdef WIN32
		CFile* pFile = PathMap.value(FilePath.toLower());
#else
		CFile* pFile = PathMap.value(FilePath);
#endif
		if(pFile && !pFile->CheckModification())
			continue;
		AddFromFile(FilePath); // this already starts the file
	}

	// Note: Files must be resumed after all files have been loaded so that Part Map Links can be established
	QList<CFile*> MultiFiles;
	foreach(CFile* pFile, m_FileMap.values())
	{
		if((pFile->IsPending() || pFile->MetaDataMissing()) 
#ifndef NO_HOSTERS
			&& !pFile->IsRawArchive()
#endif
			) // this file does not have metadata yet, thay will self resume once the data have been obtained
			pFile->Enable();
		else if(pFile->IsMultiFile())
			MultiFiles.append(pFile); 
		else
		{
			if(pFile->IsComplete())
			{
#ifndef NO_HOSTERS
				ASSERT(!pFile->IsArchive());
#endif
				if(pFile->CheckDeleted() || pFile->CheckModification())
				{
					pFile->Remove();
					continue;
				}
				else if(pFile->IsRemoved())
					pFile->UnRemove();
			}

			pFile->Resume();
		}
	}
	// Note: we must resume multi files later to proeprly enable PartMap Link IO
	foreach(CFile* pFile, MultiFiles)
	{
		if(pFile->CheckDeleted())
		{
			pFile->Remove();
			continue;
		}
		pFile->Resume();
	}
}

void CFileManager::Process(UINT Tick)
{
	CFileList::Process(Tick);

	if((Tick & EPerSec) == 0)
		return;
	
#ifndef _DEBUG
	if(m_LastSave + theCore->Cfg()->GetInt("Content/SaveInterval") < GetTime())
		StoreToFile();
#endif

	foreach(CFile* pFile, m_FileMap)
	{
		//if(pFile->IsComplete() && pFile->GetProperty("RatioActive").toBool())
		if (pFile->IsComplete() && !pFile->GetProperty("Force").toBool())
		{
			bool bStop = false;

			int ShareRatio = pFile->GetProperty("ShareRatio").toInt();
			if(!ShareRatio)
				ShareRatio = theCore->Cfg()->GetInt("Content/ShareRatio");
			if(ShareRatio)
			{
				uint64 UploadedBytes = pFile->UploadedBytes();
				uint64 DownloadedBytes = pFile->DownloadedBytes();
				if(DownloadedBytes <= 0 || (100 * UploadedBytes / DownloadedBytes) > ShareRatio)
					bStop = true;
			}

			int ShareTime = theCore->Cfg()->GetInt("Content/ShareTime");
			if(pFile->HasProperty("ShareTime"))
				ShareTime = pFile->GetProperty("ShareTime").toInt();
			if(ShareTime)
			{
				if(GetTime() + ShareTime < pFile->GetProperty("CompletedTime").toDateTime().toTime_t())
					bStop = true;
			}

			if(bStop)
			{
				//pFile->SetProperty("RatioActive", false);
				pFile->Stop();
			}
		}
	}
}

void CFileManager::Suspend()
{
	QList<uint64> CleanupList;
	foreach(CFile* pFile, m_FileMap.values())
	{
		if(pFile->GetProperty("Temp").toBool())
		{
			if(pFile->IsMultiFile()) // remove multifiles first
				CleanupList.prepend(pFile->GetFileID());
			else
				CleanupList.append(pFile->GetFileID());
		}
		else
			pFile->Suspend();
	}
	foreach(uint64 FileID, CleanupList)
	{
		if(CFile* pFile = m_FileMap.value(FileID))
			pFile->Remove(true, true);
	}

	StoreToFile();
}

bool CFileManager::GrabbFile(CFile* pFile, bool bDirect, CFile** ppKnownFile)
{	
	ASSERT(pFile->IsPending());
	pFile->SetPending(false);
	CFileList* pList = pFile->GetList(); // from that list we are pullign the sub fles if any

	if(!AddUniqueFile(pFile, bDirect, ppKnownFile))
		return false;
	
	foreach(uint64 SubFileID, pFile->GetSubFiles())
	{
		if(CFile* pSubFile = pList->GetFileByID(SubFileID))
		{
			QList<CFile*> Files = FindDuplicates(pSubFile, true);
			if(!Files.isEmpty())
			{
				ASSERT(Files.count() == 1);
				CFile* pFoundFile = Files.at(0);
				if(pFoundFile->IsRemoved())
				{
					LogLine(LOG_INFO | LOG_DEBUG, tr("Sub File %1 ot multi file %2 was already known as %3").arg(pSubFile->GetFileName()).arg(pFile->GetFileName()).arg(pFoundFile->GetFileName()));
					pFoundFile->Remove(true);
				}
				else
				{
					LogLine(LOG_INFO | LOG_DEBUG, tr("Sub File %1 ot multi file %2 is already known as %3").arg(pSubFile->GetFileName()).arg(pFile->GetFileName()).arg(pFoundFile->GetFileName()));
					if(pFile->ReplaceSubFile(pSubFile, pFoundFile))
					{
						pSubFile->Remove(true);
						continue;
					}
					LogLine(LOG_ERROR | LOG_DEBUG, tr("Sub File %1 does nto match supposed known file %2").arg(pSubFile->GetFileName()).arg(pFoundFile->GetFileName()));
				}
			}
			GrabbFile(pSubFile, true);
		}
	}

	if(!bDirect)
		theCore->m_DownloadManager->UpdateQueue();

	if(!pFile->MetaDataMissing()) // we can resume it only when we have the meta data
		pFile->Resume();

	if(theCore->Cfg()->GetBool("Content/AddPaused"))
		pFile->Pause();
	else if(!pFile->IsStarted())
		pFile->Start();
	return true;
}

bool CFileManager::AddUniqueFile(CFile* pFile, bool bDirect, CFile** ppKnownFile)
{
	QList<CFile*> Files;
	if(!bDirect)
	{
		//if(pFile->GetHash(pFile->GetMasterHash()) == NULL)
		//{
		//	LogLine(LOG_WARNING, tr("File %1 does nto have a masterhash, and can not be grabbed").arg(pFile->GetFileName()));
		//	return false;
		//}

		Files = FindDuplicates(pFile);
	}

#ifndef NO_HOSTERS
	if(Files.isEmpty() && pFile->IsRawArchive())
		Files.append(GetFilesByName(pFile->GetFileName(), true));
#endif

	foreach(CFile* pFoundFile, Files)
	{
		if(pFoundFile->IsRemoved())
			pFoundFile->Remove(true);
#ifndef NO_HOSTERS
		else if(pFile->IsArchive())
		{
			foreach(CTransfer* pTransfer, pFile->GetTransfers())
			{
				if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
					theCore->m_WebManager->AddToFile(pFoundFile, pHosterLink->GetUrl(), pHosterLink->GetFoundBy(), pHosterLink->GetProtection());
			}

			if(ppKnownFile)
				*ppKnownFile = pFoundFile;
			LogLine(LOG_INFO, tr("file %1 is already listed merged links").arg(pFile->GetFileName()));
			return false;
		}
#endif
		else
		{
			pFile->SetError("Already Known");
			if(ppKnownFile)
				*ppKnownFile = pFoundFile;
			LogLine(LOG_WARNING, tr("file %1 is already listed as %2").arg(pFile->GetFileName()).arg(pFoundFile->GetFileName()));
			return false;
		}
	}

	if(CTorrent* pTorrent = pFile->GetTopTorrent())
	{
		if(CFile* pMasterFile = MergeTorrent(pTorrent))
		{
			if(ppKnownFile)
				*ppKnownFile = pMasterFile;
			return false;
		}
	}

	foreach(CTorrent* pTorrent, pFile->GetTorrents())
	{
		if(pTorrent && !pTorrent->GetInfo()->IsEmpty())
			pTorrent->SaveTorrentToFile();
	}

	AddFile(pFile);
	return true;
}

CFile* CFileManager::MergeTorrent(CTorrent* pTorrent)
{

	CTorrentInfo* pInfo = pTorrent->GetInfo();
	if(!pInfo || pInfo->IsEmpty())
		return NULL;
		
	CFile* pMasterFile = NULL;
	if(pInfo->IsMultiFile())
	{
		const QList<CTorrentInfo::SFileInfo>& Files = pInfo->GetFiles();

		// found all needed sub files
		bool bFoundMatch = false;
		QList <CFile*> FoundFiles;
		QList <uint64> FoundSubFiles;
		foreach(const CTorrentInfo::SFileInfo& File, Files)
		{
			bFoundMatch = false;
			foreach(CFile* pSubFile, GetFilesByName(File.FileName))
			{
				// ToDo- Check Paths as well
				if(pSubFile->GetFileSize() == File.Length)
				{
					bFoundMatch = true;
					FoundFiles.append(pSubFile);
					FoundSubFiles.append(pSubFile->GetFileID());
					break;
				}
			}	
			if(!bFoundMatch)
				break;
		}

		// test potential matches
		if(bFoundMatch)
		{
			ASSERT(FoundFiles.size() > 0);

			// test if all sub files have a common master file
			QList<uint64> MasterFiles;
			CFile* pSubFile = FoundFiles.first();
			foreach(uint64 FileID, pSubFile->GetParentFiles())
				MasterFiles.append(FileID);

			for(int i=1; i < FoundFiles.size() && !MasterFiles.isEmpty(); i++)
			{
				CFile* pSubFile = FoundFiles.at(i);
				QList<uint64> ParentFiles = pSubFile->GetParentFiles();
				foreach(uint64 FileID, MasterFiles)
				{
					if(!ParentFiles.contains(FileID))
						MasterFiles.removeAll(FileID);
				}
			}

			// filter master files for only those that contain the sub files in the right order
			foreach(uint64 FileID, MasterFiles)
			{
				bool bMissmatch = true;
				if(CFile* pMasterFile = GetFileByID(FileID))
				{
					bMissmatch = false;
					QList<uint64> SubFiles = pMasterFile->GetSubFiles();
					ASSERT(SubFiles.size() == Files.size());
					for(int i=0; i < SubFiles.size(); i++)
					{
						CFile* pSubFile = GetFileByID(SubFiles[i]);
						if(!pSubFile || pSubFile->GetFileSize() != Files[i].Length)
							bMissmatch = true;
					}
				}
				if(bMissmatch)
					MasterFiles.removeAll(FileID);
			}
					
					
			// prepare master file for torrent adding
			if(!MasterFiles.isEmpty())
			{
				ASSERT(MasterFiles.size() == 1); // we should not have duplicates

				pMasterFile = GetFileByID(MasterFiles.first());
				if(pMasterFile->IsRemoved())
					pMasterFile->Start();
			}
			else // add new multifile
			{
				pMasterFile = new CFile(this);
				if(pMasterFile->AddNewMultiFile(pInfo->GetTorrentName(), FoundSubFiles))
				{
					AddFile(pMasterFile);

					pMasterFile->SetMasterHash(pTorrent->GetHash());
					pMasterFile->Resume();

					if(!theCore->Cfg()->GetBool("Content/AddPaused"))
						pMasterFile->Start();
				}
				else
				{
					delete pMasterFile;
					pMasterFile = NULL;
				}
			}
		}
	}
	else // single file is simple if name and ize match we just try it
	{
		foreach(CFile* pOldFile, GetFilesByName(pInfo->GetTorrentName()))
		{
			if(pOldFile->GetFileSize() == pInfo->GetTotalLength())
			{
				pMasterFile = pOldFile;
				break;
			}
		}	
	}

	if(pMasterFile)
	{
		LogLine(LOG_INFO, tr("Found potential torrent match: %1").arg(pInfo->GetTorrentName()));

		pMasterFile->AddTorrent(pInfo->SaveTorrentFile());

		return pMasterFile;
	}
	
	return NULL;
}

void CFileManager::ScanShare()
{
	QHash<QString, CFile*> PathMap;
	foreach(CFile* pFile, m_FileMap)
	{
#ifdef WIN32
		PathMap.insert(pFile->GetFilePath().toLower(), pFile);
#else
		PathMap.insert(pFile->GetFilePath(), pFile);
#endif
	}
	
	QStringList FilePaths = FindSharedFiles();
	foreach(const QString& FilePath, FilePaths)
	{
#ifdef WIN32
		if(CFile* pFile = PathMap.value(FilePath.toLower()))
#else
		if(CFile* pFile = PathMap.value(FilePath))
#endif
		{
			if(pFile->IsRemoved())
			{
				pFile->UnRemove();
				pFile->Resume();
			}		
			continue;
		}
		if(CFile* pFile = AddFromFile(FilePath))
			pFile->Resume();
	}

	QHash<QString, bool> NewPathMap;
	foreach(const QString& NewPath, FilePaths)
	{
#ifdef WIN32
		NewPathMap.insert(NewPath.toLower(), true);
#else
		NewPathMap.insert(NewPath, true);
#endif
	}

	foreach(CFile* pFile, m_FileMap.values())
	{
		if(!pFile->IsMultiFile())
		{
			if(!pFile->IsIncomplete())
			{
#ifndef NO_HOSTERS
				ASSERT(!pFile->IsArchive());
#endif
#ifdef WIN32
				if(!NewPathMap.contains(pFile->GetFilePath().toLower()) && !pFile->IsRemoved())
#else
				if(!NewPathMap.contains(pFile->GetFilePath()) && !pFile->IsRemoved())
#endif
					pFile->Remove();
			}
		}
	}
}

QStringList	CFileManager::ListDir(const QString& srcDirPath)
{
	ASSERT(srcDirPath.right(1) == "/");

	QStringList FileList;
	QDir srcDir(srcDirPath);
	if (!srcDir.exists())
		return FileList;

	QStringList Files = srcDir.entryList(QDir::Files);
	foreach (const QString& FileName, Files)
		FileList.append(FileName);

	QStringList Dirs = srcDir.entryList(QDir::Dirs);
	foreach (const QString& DirName, Dirs)
	{
		if (DirName.compare(".") == 0 || DirName.compare("..") == 0)
			continue;

		if(Split2(DirName, ".", true).second.compare("app", Qt::CaseInsensitive) == 0 && srcDir.exists(srcDirPath + DirName + "/Contents"))
		{
			LogLine(LOG_ERROR, tr("Please dont add app bundles to share %1").arg(srcDirPath + DirName));
			continue;
		}

		QStringList SubFiles = ListDir(srcDirPath + DirName + "/");
		foreach (const QString& FileName, SubFiles)
			FileList.append(DirName + "/" + FileName);

	}
	return FileList;
}

QStringList CFileManager::FindSharedFiles()
{
	QString DlDir = theCore->GetIncomingDir(false);

	QStringList Files;
	foreach(const QString& Name, ListDir(DlDir))
		Files.append(DlDir + Name);
	QStringList Shard = theCore->Cfg()->GetStringList("Content/Shared");
	foreach(const QString& Path, Shard)
	{
		if(Path.isEmpty())
			continue;

		if(Path.right(1) == "/")
		{
			foreach(const QString& Name, ListDir(Path))
				Files.append(Path + Name);
		}
		else
			Files.append(Path);
	}
	return Files;
}

CFile* CFileManager::AddFromFile(const QString& FilePath)
{
	CFile* pFile = new CFile();
	if(pFile->AddFromFile(FilePath))
	{
		AddFile(pFile);
	}
	else
	{
		delete pFile;
		LogLine(LOG_ERROR, tr("failed to add file %1 to list,").arg(FilePath));
		return NULL;
	}
	return pFile;
}

/////////////////////////////////////////////////////////////////////////////////////
// Load/Store

void CFileManager::StoreToFile()
{
	CXml::Write(Store(), theCore->Cfg()->GetSettingsDir() + "/FileList.xml");

	m_LastSave = GetTime();
	LogLine(LOG_DEBUG, tr("saved file list to disk"));
}

void CFileManager::LoadFromFile()
{
	Load(CXml::Read(theCore->Cfg()->GetSettingsDir() + "/FileList.xml").toList());

	LogLine(LOG_DEBUG, tr("loaded file list from disk"));
	m_LastSave = GetTime();
}

