#include "GlobalHeader.h"
#include "Torrent.h"
#include "../../FileList/FileList.h"
#include "../../FileTransfer/Transfer.h"
#include "../../FileList/PartMap.h"
#include "../../FileList/IOManager.h"
#include "../../FileList/Hashing/FileHashTree.h"
#include "../../FileList/Hashing/FileHashSet.h"
#include "../../NeoCore.h"
#include "../../FileTransfer/BitTorrent/TorrentManager.h"
#include "../../FileTransfer/BitTorrent/TorrentInfo.h"
#include "../../FileTransfer/FileGrabber.h"
#include "../../FileList/FileManager.h"
#include "../../FileTransfer/ed2kMule/MuleManager.h"
#include "../../FileTransfer/ed2kMule/MuleKad.h"
#include "../../../Framework/Cryptography/HashFunction.h"
#include "../../FileTransfer/HashInspector.h"
#include "../../../Framework/OtherFunctions.h"

int	GetMetadataBlockCount(uint64 MetadataSize)
{
	int BlockCount = MetadataSize / META_DATA_BLOCK_SIZE;
	if(MetadataSize % META_DATA_BLOCK_SIZE)
		BlockCount += 1;
	return BlockCount;
}

CTorrent::CTorrent(CFile* pFile)
: QObjectEx(pFile)
{
	m_TorrentInfo = NULL;

	m_MetadataExchange = NULL;
}

CTorrent::~CTorrent()
{
	delete m_MetadataExchange;
}

bool CTorrent::AddTorrentFromFile(const QByteArray& Torrent)
{
	CFile* pFile = GetFile();

	m_TorrentInfo = new CTorrentInfo(this);
	if(!m_TorrentInfo->LoadTorrentFile(Torrent))
		return false;
	
	// Privat etorrents should only be shared on the torrent network
	// Note: we overwrite any user choices except the file is completed than the use has all liberties
	if(pFile->IsIncomplete() && m_TorrentInfo->IsPrivate())
		pFile->SetProperty("Torrent", 2);

	if(!pFile->IsPending())
		SaveTorrentToFile();

	pFile->SetFileSize(m_TorrentInfo->GetTotalLength());
	LoadPieceHashes();

	theCore->m_TorrentManager->RegisterInfoHash(m_TorrentInfo->GetInfoHash());

	pFile->SetMasterHash(m_pHash);

	pFile->SetFileName(m_TorrentInfo->GetTorrentName());

	pFile->SetProperty("Description", m_TorrentInfo->GetProperty("Description"));

	SetupPartMap();

	return true;
}

void CTorrent::SetupPartMap()
{
	ASSERT(!m_TorrentInfo->IsEmpty());

	CFile* pFile = GetFile();

	// Single File
	if(!m_TorrentInfo->IsMultiFile())
	{
		if(!pFile->GetPartMap())
			pFile->SetPartMap(CPartMapPtr(new CSynced<CPartMap>(m_TorrentInfo->GetTotalLength())));
		return;
	}

	// Multi File:

	CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(pFile->GetPartMap());
	if(pParts)
	{
		ASSERT(!pParts->GetLinks().isEmpty());
		return; // is already set up
	}
	pParts = new CJoinedPartMap(pFile->GetFileSize());
	
	pFile->GetInspector()->SetIndexSource(HashTorrent);

	CFileList* pList = pFile->GetList();

	uint64 Offset = 0;
	foreach(const CTorrentInfo::SFileInfo& SubFile, m_TorrentInfo->GetFiles())
	{
		if(SubFile.Length == 0)
		{
			LogLine(LOG_DEBUG | LOG_WARNING, tr("Ignoring empty file '%1' in torrent '%2'").arg(SubFile.FileName).arg(pFile->GetFileName()));
			continue;
		}

		CFile* pSubFile = new CFile();
		if(pFile->GetProperty("Temp").toBool())
			pSubFile->SetProperty("Temp", true);

		QString Dir = pFile->GetFileDir();
		Dir += pFile->GetFileName() + "/";
		if(!SubFile.FilePath.isEmpty())
			Dir += SubFile.FilePath.join("/") + "/";
		pSubFile->SetFileDir(Dir);

		pSubFile->AddEmpty(HashTorrent, SubFile.FileName, SubFile.Length, pFile->IsPending());

		// Note: SubFile->MasterHash is set in MasterFile->Resume

		uint64 uBegin = Offset; 
		uint64 uEnd = Offset + SubFile.Length;
		Offset += SubFile.Length;
		
		CSharedPartMap* pSubParts = new CSharedPartMap(uEnd - uBegin);
		pSubFile->SetPartMap(CPartMapPtr(pSubParts));

		pParts->SetupLink(uBegin, uEnd, pSubFile->GetFileID());
		pSubParts->SetupLink(uBegin, uEnd, pFile->GetFileID());

		pList->AddFile(pSubFile);

		if(!pSubFile->IsPending())
			pSubFile->Resume();

		if(pFile->IsPaused(true))
			pSubFile->Pause();
		else if(pFile->IsStarted())
			pSubFile->Start();
	}

	pFile->SetPartMap(CPartMapPtr(pParts));
}

bool CTorrent::LoadPieceHashes()
{
	if(m_TorrentInfo->IsMerkle())
	{
		m_pHash = CFileHashPtr(new CFileHashTree(HashTorrent, m_TorrentInfo->GetTotalLength(), m_TorrentInfo->GetPieceLength()));
		m_pHash->SetHash(m_TorrentInfo->GetInfoHash());
		((CFileHashTree*)m_pHash.data())->SetRootHash(m_TorrentInfo->GetRootHash());
	}
	else if(!m_TorrentInfo->GetPieceHashes().isEmpty())
	{
		CFileHashSet* pHashSet = new CFileHashSet(HashTorrent, m_TorrentInfo->GetTotalLength(), m_TorrentInfo->GetPieceLength());
		m_pHash = CFileHashPtr(pHashSet);
		pHashSet->SetHash(m_TorrentInfo->GetInfoHash());
		pHashSet->SetHashSet(m_TorrentInfo->GetPieceHashes());
	}
	else
		return false;
	return true;
}

void CTorrent::AddEmptyTorrent(const QString& FileName, const QByteArray& InfoHash)
{
	ASSERT(m_TorrentInfo == NULL);
	m_TorrentInfo = new CTorrentInfo(this);
	m_TorrentInfo->SetInfoHash(InfoHash);
	
	ASSERT(m_pHash.isNull());
	m_pHash = CFileHashPtr(new CFileHash(HashTorrent));
	m_pHash->SetHash(InfoHash);

	m_TorrentInfo->SetTorrentName(FileName);

	theCore->m_TorrentManager->RegisterInfoHash(InfoHash);
}

bool CTorrent::CompareSubFiles(CPartMap* pPartMap)
{
	if(pPartMap->GetSize() != m_TorrentInfo->GetTotalLength())
		return false;

	CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(pPartMap);

	if(m_TorrentInfo->IsMultiFile() != (pParts != NULL))
		return false; // one is a single file the other a multi file

	if(pParts) // check sub files
	{
		const QList<CTorrentInfo::SFileInfo>& Files = m_TorrentInfo->GetFiles();
		QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();
		if(Links.isEmpty())
			return true;

		int Counter = 0;
		for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin(); )
		{
			SPartMapLink* pLink = *(--I);
			const CTorrentInfo::SFileInfo* File;
			do File = &Files[Counter++];
			while (File->Length == 0); // we always skip empty files

			if(pLink->uShareEnd - pLink->uShareBegin != File->Length)
				return false; // wrong sub file size
		}
	}

	return true;
}

bool CTorrent::InstallMetadata()
{
	CFile* pFile = GetFile();

	ASSERT(m_TorrentInfo);

	CPartMap* pPartMap = pFile->GetPartMap();
	if(pPartMap && !CompareSubFiles(pPartMap))
	{
		if(pFile->GetInspector()->BadMetaData(GetHash()))
			pPartMap = NULL;
		else
			return false;
	}

	if(m_pHash->Compare(pFile->GetMasterHash().data()))
		pFile->SetFileName(m_TorrentInfo->GetTorrentName());

	bool bOpenIO = false;
	CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(pPartMap);
	if((pPartMap == NULL && !pFile->IsComplete()) || (pJoinedParts && pJoinedParts->GetLinks().isEmpty()))
	{
		bOpenIO = true;
		if(!pPartMap)
			pFile->SetFileSize(m_TorrentInfo->GetTotalLength());

		SetupPartMap();

		if(!pPartMap && !pFile->IsPending())
			pFile->SetFilePath();
	}

	LoadPieceHashes();

	if(pFile->IsIncomplete() && pFile->GetMasterHash()->GetType() == HashTorrent)
		pFile->CleanUpHashes();

	// Note: if neo is the masterhash the IO is already opened
	if(bOpenIO && !pFile->IsPending())
		pFile->Resume();
	return true;
}

bool CTorrent::MakeTorrent(uint64 uPieceLength, bool bMerkle, const QString& Name, bool bPrivate)
{
	CFile* pFile = GetFile();

	if(!pFile->IsComplete())
	{
		LogLine(LOG_DEBUG | LOG_ERROR, tr("A torrent can not be made form an Incompelte file %1").arg(pFile->GetFileName()));
		return false;
	}
	
	if(uPieceLength < KB2B(16)) //if(!uPieceLength)
	{
		uPieceLength = pFile->GetFileSize() / (KB2B(40) / 20); // target hast set size 40 KB
	
		uint64 i = KB2B(16);
		for (; i < MB2B(32); i *= 2)
		{
			if (i >= uPieceLength) 
				break;
		}
		uPieceLength = i;
	}

	m_TorrentInfo = new CTorrentInfo(this);
	m_TorrentInfo->SetTorrentName(Name.isEmpty() ? pFile->GetFileName() : Name);
	m_TorrentInfo->SetTotalLength(pFile->GetFileSize());
	if(bPrivate)
		m_TorrentInfo->SetPrivate();

	m_TorrentInfo->SetProperty("CreationTime", QDateTime::currentDateTime());

	ASSERT(m_pHash.isNull());
	if(bMerkle)
		m_pHash = CFileHashPtr(new CFileHashTree(HashTorrent, m_TorrentInfo->GetTotalLength(), uPieceLength));
	else
		m_pHash = CFileHashPtr(new CFileHashSet(HashTorrent, m_TorrentInfo->GetTotalLength(), uPieceLength));
	return true;
}

bool CTorrent::ImportTorrent(const QByteArray &TorrentData)
{
	m_TorrentInfo = new CTorrentInfo(this);
	if(!m_TorrentInfo->LoadTorrentFile(TorrentData))
		return false;

	ASSERT(m_pHash.isNull());
	if(GetFile()->IsIncomplete())
	{
		if(!LoadPieceHashes())
			return false;

		theCore->m_TorrentManager->RegisterInfoHash(m_TorrentInfo->GetInfoHash());
	}
	else
	{
		if(m_TorrentInfo->IsMerkle())
			m_pHash = CFileHashPtr(new CFileHashTree(HashTorrent, m_TorrentInfo->GetTotalLength(), m_TorrentInfo->GetPieceLength()));
		else
			m_pHash = CFileHashPtr(new CFileHashSet(HashTorrent, m_TorrentInfo->GetTotalLength(), m_TorrentInfo->GetPieceLength()));
	}
	return true;
}

QByteArray CTorrent::GetInfoHash()
{
	ASSERT(m_TorrentInfo);
	return m_TorrentInfo->GetInfoHash();
}

CFileHashPtr CTorrent::GetHash()
{
	ASSERT(!m_pHash.isNull());
	return m_pHash;
}

void CTorrent::OnFileHashed()
{
	CFile* pFile = GetFile();
	ASSERT(m_TorrentInfo);
	if(m_TorrentInfo->IsEmpty()) // are we making a torrent
	{
		QStringList Shared = theCore->Cfg()->GetStringList("Content/Shared");
		Shared.append(theCore->GetIncomingDir());
		Shared.append(theCore->GetTempDir());

		QList<CTorrentInfo::SFileInfo> Files;
		if(CJoinedPartMap* pParts = qobject_cast<CJoinedPartMap*>(pFile->GetPartMap()))
		{
			QMap<uint64, SPartMapLink*> Links = pParts->GetJoints();
			for(QMap<uint64, SPartMapLink*>::iterator I = Links.end(); I != Links.begin();)
			{
				SPartMapLink* pLink = *(--I);

				CFile* pSubFile = pFile->GetList()->GetFileByID(pLink->ID);
				if(!pSubFile)
				{
					LogLine(LOG_DEBUG | LOG_ERROR, tr("A sub file of %1 has been being removed befoure the torrent was created").arg(pFile->GetFileName()));
					pFile->TorrentHashed(this, false);
					return;
				}

				CTorrentInfo::SFileInfo File;
				QString Root;
				QStringList Path = GetRelativeSharedPath(pSubFile->GetFilePath(), Shared, Root).split("/", QString::SkipEmptyParts);
				if(!Path.isEmpty())
				{
					if(Path.count() > 1)
						Path.removeFirst();
					File.FileName = Path.takeLast();
					File.FilePath = Path;
				}
				else
					File.FileName = "unknown";
				File.Length = pSubFile->GetFileSize();
				Files.append(File);
			}
		}
		
		if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(m_pHash.data()))
			m_TorrentInfo->MakeMetadata(Files, pHashTree->GetPartSize(), QList<QByteArray>(), pHashTree->GetRootHash());
		else if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(m_pHash.data()))
			m_TorrentInfo->MakeMetadata(Files, pHashSet->GetPartSize(), pHashSet->GetHashSet());
		else { ASSERT(0);
		}

		if(!pFile->IsPending())
			SaveTorrentToFile();

		m_pHash->SetHash(m_TorrentInfo->GetInfoHash());
		theCore->m_TorrentManager->RegisterInfoHash(m_TorrentInfo->GetInfoHash());

		pFile->TorrentHashed(this, true);
	}
	else // we are importing a torrent
	{
		bool bMatch = false;

		if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(m_pHash.data()))
			bMatch = m_TorrentInfo->GetRootHash() == pHashTree->GetRootHash();
		else if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(m_pHash.data()))
			bMatch = m_TorrentInfo->GetPieceHashes() == pHashSet->GetHashSet();
		else { ASSERT(0);
		}

		if(bMatch)
		{
			if(!pFile->IsPending())
				SaveTorrentToFile();

			m_pHash->SetHash(m_TorrentInfo->GetInfoHash());
			theCore->m_TorrentManager->RegisterInfoHash(m_TorrentInfo->GetInfoHash());
		}

		pFile->TorrentHashed(this, bMatch);
	}
}

QString	CTorrent::GetSubFileName(int Index)
{
	const QList<CTorrentInfo::SFileInfo>& Files = m_TorrentInfo->GetFiles();
	if(Files.size() <= Index)
		return "";
	return Files[Index].FileName;
}

QString	CTorrent::GetSubFilePath(int Index)
{
	const QList<CTorrentInfo::SFileInfo>& Files = m_TorrentInfo->GetFiles();
	if(Files.size() <= Index)
		return "";
	CFile * pFile = GetFile();
	
	QString Dir = pFile->GetFileDir();
	Dir += pFile->GetFileName() + "/";
	if(!Files[Index].FilePath.isEmpty())
		Dir += Files[Index].FilePath.join("/") + "/";

	return Dir;
}

bool CTorrent::SaveTorrentToFile()
{
	ASSERT(!m_TorrentInfo->IsEmpty());
	QString TorrentFile = QString(m_TorrentInfo->GetInfoHash().toHex()) + ".torrent";
	return m_TorrentInfo->SaveTorrentFile(theCore->m_TorrentManager->GetTorrentDir() + TorrentFile);
}

void CTorrent::RemoveTorrentFile()
{
	QString TorrentFile = QString(m_TorrentInfo->GetInfoHash().toHex()) + ".torrent";
	QFile::remove(theCore->m_TorrentManager->GetTorrentDir() + TorrentFile);
}

bool CTorrent::LoadTorrentFromFile(const QByteArray& InfoHash)
{
	ASSERT(m_TorrentInfo == NULL);
	m_TorrentInfo = new CTorrentInfo(this);

	ASSERT(m_pHash.isNull());
	m_pHash = CFileHashPtr(new CFileHash(HashTorrent));
	m_pHash->SetHash(InfoHash);

	QString TorrentFile = QString(InfoHash.toHex()) + ".torrent";

	if(m_TorrentInfo->LoadTorrentFile(theCore->m_TorrentManager->GetTorrentDir() + TorrentFile))
	{
		if(m_TorrentInfo->GetInfoHash() == InfoHash)
		{
			CFile* pFile = GetFile();

			if(pFile->GetFileSize() == 0)
				pFile->SetFileSize(m_TorrentInfo->GetTotalLength());

			LoadPieceHashes();

			if(m_TorrentInfo->IsMultiFile() && !pFile->IsMultiFile())
			{
				CFileHashPtr pMasterHash = pFile->GetMasterHash();
				if(!pMasterHash.isNull() && pMasterHash->GetHash() == InfoHash)
				{
					LogLine(LOG_DEBUG | LOG_ERROR, tr("The multi file %1 is missing its proper index, restoring form torrent").arg(pFile->GetFileName()));
					InstallMetadata();
				}
			}

			if(!m_TorrentInfo->IsEmpty() && !pFile->IsComplete() && !pFile->GetPartMap())
				SetupPartMap();
		}
		else
		{
			LogLine(LOG_DEBUG | LOG_ERROR, tr("The torrent file %1 contains an invalid infohash").arg(TorrentFile));
			delete m_TorrentInfo;
			m_TorrentInfo = new CTorrentInfo(this);
			m_TorrentInfo->SetInfoHash(InfoHash);
		}
	}
	else
		m_TorrentInfo->SetInfoHash(InfoHash);
	theCore->m_TorrentManager->RegisterInfoHash(m_TorrentInfo->GetInfoHash());

	return true; // Note: that is always true even if we fail to load as we always wil be able to proceed one way or another
}

int CTorrent::NextMetadataBlock(const QByteArray& ID)
{
	if(!m_TorrentInfo->IsEmpty())
		return -1;

	if(!m_MetadataExchange)
		m_MetadataExchange = new SMetadataExchange;

	if(m_MetadataExchange->Metadata[ID].MetadataSize == -1) // -1 means this one has rejected us
		return -1;

	uint64 MetadataSize = m_MetadataExchange->Metadata[ID].MetadataSize;
	if(!MetadataSize) // if this client is asked for the first time get a majority consence for the expected size
	{
		QMap<uint64, int> Map;
		for(QMap<QByteArray, SMetadata>::iterator I = m_MetadataExchange->Metadata.begin(); I != m_MetadataExchange->Metadata.end(); I++)
		{
			if(I->MetadataSize && I->MetadataSize != -1)
				Map[I->MetadataSize]++;
		}
		if(!Map.isEmpty())
		{
			QMap<uint64, int>::iterator I = (--Map.end()); // thats the entry with the highest count
			MetadataSize = I.key();
		}
	}
	int BlockCount = MetadataSize ? GetMetadataBlockCount(MetadataSize) : 1; // if we dont know the block count just assume 1

	int BestCount = 0;
	int BestIndex = -1;
	for(int i=0; i < BlockCount; i++)
	{
		if(m_MetadataExchange->Metadata[ID].Blocks.contains(i))
			continue; // we have this one block from that one peer

		int CurCount = 0;
		for(QMap<QByteArray, SMetadata>::iterator I = m_MetadataExchange->Metadata.begin(); I != m_MetadataExchange->Metadata.end(); I++)
		{
			if(I->Blocks.contains(i))
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
		m_MetadataExchange->Metadata[ID].Blocks[BestIndex] = QByteArray();
//#ifdef _DEBUG
//		qDebug() << "Meta Data Request block " << BestIndex << " out of " << BlockCount << " (" << MetadataSize << ")";
//#endif
	}
	return BestIndex;
}

void CTorrent::AddMetadataBlock(int Index, const QByteArray& MetadataBlock, uint64 uTotalSize, const QByteArray& ID)
{
	ASSERT(uTotalSize);
	if(!m_MetadataExchange)
		return;

//#ifdef _DEBUG
//		qDebug() << "Meta Data Got block " << Index;
//#endif

	if(!m_MetadataExchange->Metadata[ID].MetadataSize)
		m_MetadataExchange->Metadata[ID].MetadataSize = uTotalSize;
		
	m_MetadataExchange->Metadata[ID].Blocks[Index] = MetadataBlock;

	m_MetadataExchange->NewData = true;
}

void CTorrent::ResetMetadataBlock(int Index,const QByteArray& ID)
{
	if(!m_MetadataExchange)
		return;

//#ifdef _DEBUG
//		qDebug() << "Meta Data denyed block " << Index;
//#endif

	m_MetadataExchange->Metadata[ID].MetadataSize = -1; // dont ask this peer in future is rejected our request
	m_MetadataExchange->Metadata[ID].Blocks.clear();
}

QByteArray CTorrent::TryAssemblyMetadata()
{
	QVector<QByteArray> Peers = m_MetadataExchange->Metadata.keys().toVector();
	for(int i = 0; i < Peers.size(); i++)
	{
		const QByteArray& ID = Peers[i];
		SMetadata &Metadata = m_MetadataExchange->Metadata[ID];
		if(Metadata.MetadataSize == 0 || Metadata.MetadataSize == -1)
			continue;

		QByteArray Payload;

		int BlockCount = GetMetadataBlockCount(Metadata.MetadataSize);
		for(int Index = 0; Index < BlockCount; Index++)
		{
			if(Metadata.Blocks.contains(Index) && !Metadata.Blocks[Index].isEmpty())
				Payload += Metadata.Blocks[Index];
			else
			{
				int j = 1;
				for(; j < Peers.size(); j++)
				{
					int k = (i + j) % Peers.size();
					if(m_MetadataExchange->Metadata[Peers[k]].Blocks.contains(Index) && !m_MetadataExchange->Metadata[Peers[k]].Blocks[Index].isEmpty())
					{
						Payload += m_MetadataExchange->Metadata[Peers[k]].Blocks[Index];
						break;
					}
				}
				if(j >= Peers.size()) // we need this block
					return QByteArray();
			}
		}

		QByteArray InfoHash = CHashFunction::Hash(Payload, CAbstractKey::eSHA1);
		if(InfoHash == m_TorrentInfo->GetInfoHash())
			return Payload;
//#ifdef _DEBUG
//		else 
//			qDebug() << "Meta Data Hashign Failed Failed";
//#endif
	}
	return QByteArray();
}

bool CTorrent::TryInstallMetadata()
{
	if(m_MetadataExchange && m_MetadataExchange->NewData)
	{
		m_MetadataExchange->NewData = false;

		QByteArray Payload = TryAssemblyMetadata();
		if(!Payload.isEmpty())
		{
			if(!m_TorrentInfo->LoadMetadata(Payload))
			{
				LogLine(LOG_DEBUG | LOG_ERROR, tr("The torrent metadata for %1 (%2) cannot not be parsed.").arg(GetFile()->GetFileName()).arg(QString(m_TorrentInfo->GetInfoHash().toHex())));
				// that is not recoverable!!!!
				GetFile()->GetInspector()->BlackListHash(GetHash()); // Note: This will delete this
				return false;
			}
			else
			{
				LogLine(LOG_DEBUG | LOG_SUCCESS, tr("recived metadata for torrent %1 (%2)").arg(GetFile()->GetFileName()).arg(QString(m_TorrentInfo->GetInfoHash().toHex())));
				if(!GetFile()->IsPending())
					SaveTorrentToFile();

				if(!InstallMetadata()) // Note: This can delete this
					return false;
				
				emit MetadataLoaded();
			}

			delete m_MetadataExchange;
			m_MetadataExchange = NULL;
			return true;
		}
	}
	return true;
}