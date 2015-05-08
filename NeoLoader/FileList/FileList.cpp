#include "GlobalHeader.h"
#include "FileList.h"
#include "File.h"
#include "../FileTransfer/Transfer.h"
#include "./Hashing/HashingThread.h"

QSet<uint64> CFileList::m_FileIDs;
QSet<CFileList*> CFileList::m_AllLists;

CFileList::CFileList(QObject* qObject)
 : QObjectEx(qObject)
{
	m_LastID = 0;

	m_AllLists.insert(this);
}

CFileList::~CFileList()
{
	m_AllLists.remove(this);
}

void CFileList::Process(UINT Tick)
{
	QMap<uint64, CFile*>::iterator I = m_FileMap.find(m_LastID);
	if(I != m_FileMap.end())
		I++;
	for(int j = 0; !m_FileMap.isEmpty(); j++, I++)
	{
		if(I == m_FileMap.end())
			I = m_FileMap.begin();

		if(j >= m_FileMap.size())
		{
			m_LastID = I.key();
			break;
		}

		if(I.value()->IsStarted())
			I.value()->Process(Tick);
	}
}

void CFileList::AddFile(CFile* pFile)
{
	if(CFileList* pList = pFile->GetList())
		pList->UnlistFile(pFile);
	ListFile(pFile);
}

CFile* CFileList::GetFileByID(uint64 FileID)
{
	return m_FileMap.value(FileID);
}

CFile* CFileList::GetFileByHash(const CFileHash* pFileHash, bool bAlsoRemoved)
{
	if(!pFileHash)
		return NULL;
	foreach(CFile* pFile, m_FileMap)
	{
		if(pFile->IsDuplicate())
			continue;
		if(!bAlsoRemoved && pFile->IsRemoved())
			continue;

		if(pFile->CompareHash(pFileHash))
			return pFile;
	}
	return NULL;
}

QList<CFile*> CFileList::FindDuplicates(CFile* pFile, bool bNoDuplicates)
{
	QList<CFileHashPtr> Hashes = pFile->GetAllHashes(true);
	if(Hashes.isEmpty())
		return QList<CFile*>();
	
	QList<CFile*> Files = GetFilesByHash(Hashes, pFile->GetFileSize(), bNoDuplicates);
	Files.removeAll(pFile);
	return Files;
}

QList<CFile*> CFileList::GetFilesByHash(const QList<CFileHashPtr>& Hashes, uint64 uFileSize, bool bNoDuplicates)
{
	QList<CFile*> Files;
	for (QMap<uint64, CFile*>::Iterator I = m_FileMap.begin(); I != m_FileMap.end(); ++I)
	{
		CFile* pCurFile = I.value();

		if(pCurFile->GetFileSize() != 0 && uFileSize != 0
		&& pCurFile->GetFileSize() != uFileSize)
			continue; // files must have the exact same size

		if(bNoDuplicates && pCurFile->IsDuplicate())
			continue; // we are looking for active duplicates, ignore passive ones

		// Note: the code below is not obtimized (GetAllHashes) but it seams fast enough
		foreach(CFileHashPtr pHash, Hashes)
		{
			if(pCurFile->CompareHash(pHash.data()))
			{
				if(pCurFile->IsDuplicate())
					Files.append(pCurFile);
				else
					Files.prepend(pCurFile); // real duplicates at the top
				break;
			}
		}
	}
	return Files;
}

QList<CFile*> CFileList::GetFilesByName(const QString& FileName, bool bArchives)
{
	QList<CFile*> Files;
	foreach(CFile* pFile, m_FileMap)
	{
		if(FileName.compare(pFile->GetFileName(), Qt::CaseInsensitive) == 0)
		{
#ifndef NO_HOSTERS
			if(bArchives && !pFile->IsArchive())
				continue;
#endif

			Files.append(pFile);
		}
	}
	return Files;
}

CFile* CFileList::GetFileByProperty(const QString& Name, const QVariant& Value)
{
	foreach(CFile* pFile, m_FileMap)
	{
		if(pFile->GetProperty(Name) == Value)
			return pFile;
	}
	return NULL;
}

CFile* CFileList::GetArchiveFile(const QString& FileName)
{
#ifndef NO_HOSTERS
	foreach(CFile* pFile, m_FileMap)
	{
		if(pFile->IsArchive() && FileName.compare(pFile->GetFileName(), Qt::CaseInsensitive) == 0)
			return pFile;
	}
#endif
	return NULL;
}

CFile* CFileList::GetFileByPath(const QString& FilePath)
{
	foreach(CFile* pFile, m_FileMap)
	{
#ifdef WIN32
		if(FilePath.compare(pFile->GetFilePath(), Qt::CaseInsensitive) == 0)
#else
		if(FilePath.compare(pFile->GetFilePath(), Qt::CaseSensitive) == 0)
#endif
			return pFile;
	}
	return NULL;
}

void CFileList::RemoveFile(CFile* pFile)
{
	UnlistFile(pFile);
	delete pFile;
}

QList<CFile*> CFileList::GetFilesBySourceUrl(const QString& sUrl)
{
	QList<CFile*> FileList;
	foreach(CFile* pFile, m_FileMap)
	{
		foreach(CTransfer* pSource, pFile->m_Transfers)
		{
			if(sUrl.compare(pSource->GetUrl()) == 0)
			{
				FileList.append(pFile);
				break;
			}
		}
	}
	return FileList;
}

void CFileList::ListFile(CFile* pFile)
{
	pFile->setParent(this);
	m_FileMap.insert(pFile->GetFileID(), pFile);
}

void CFileList::UnlistFile(CFile* pFile)
{
	//uint64 FileID = m_FileMap.key(pFile);
	m_FileMap.remove(pFile->GetFileID());
}

uint64 CFileList::AllocID(uint64 FileID)
{
	while(m_FileIDs.contains(FileID) || FileID == 0) // 0 is not allowed
	{
#ifdef _DEBUG
		static uint64 ID = 0;
		ID++;
		FileID = ID;
#else
		FileID = GetRand64() & MAX_FLOAT;
#endif
	}

	m_FileIDs.insert(FileID);
	return FileID;
}

void CFileList::ReleaseID(uint64 FileID)
{
	m_FileIDs.remove(FileID);
}

CFile* CFileList::GetFile(uint64 FileID)
{
	foreach(CFileList* pList, m_AllLists)
	{
		if(CFile* pFile = pList->GetFileByID(FileID))
			return pFile;
	}
	return NULL;
}

QList<CFile*> CFileList::GetAllFiles()
{
	QList<CFile*> Files;
	foreach(CFileList* pList, m_AllLists)
		Files.append(pList->m_FileMap.values());
	return Files;
}

/////////////////////////////////////////////////////////////////////////////////////
// Load/Store

QVariant CFileList::Store()
{
	QVariantList FileList;
	for (QMap<uint64, CFile*>::Iterator I = m_FileMap.begin(); I != m_FileMap.end(); ++I)
	{
		CFile* pFile = I.value();
		FileList.append(pFile->Store());
	}
	return FileList;
}

int CFileList::Load(const QVariantList& FileList)
{
	int Count = 0;
	foreach(const QVariant& vFile, FileList)
	{
		QVariantMap File = vFile.toMap();
		CFile* pFile = new CFile(this);
		if(pFile->Load(File))
		{
			ListFile(pFile);
			Count++;
		}
		else
		{
			delete pFile;
			LogLine(LOG_ERROR, tr("failed to load file"));
		}
	}
	return Count;
}