#include "GlobalHeader.h"
#include "NeoClient.h"
#include "NeoSession.h"
#include "../../NeoCore.h"
#include "NeoManager.h"
#include "../../FileList/File.h"
#include "../../FileList/FileManager.h"
#include "NeoEntity.h"
#include "../../FileList/IOManager.h"
#include "../../../Framework/Exception.h"
#include "../../FileList/Hashing/FileHashSet.h"
#include "../../FileList/Hashing/FileHashTree.h"
#include "../../FileList/Hashing/FileHashTreeEx.h"
#include "../../FileList/Hashing/HashingThread.h"
#include "../../../Framework/Exception.h"
#include "../BitTorrent/Torrent.h"
#include "../BitTorrent/TorrentInfo.h"


CNeoClient::CNeoClient(CNeoEntity* pNeo, QObject* qObject)
 : CP2PClient(qObject)
{
	m_Session = NULL;
	m_Neo = pNeo->GetNeo();
	m_pNeo = pNeo;
	Init();
}

CNeoClient::CNeoClient(CNeoSession* pSession, QObject* qObject)
 : CP2PClient(qObject)
{
	m_Session = pSession;
	m_Neo.EntityID = pSession->GetEntityID();
	m_Neo.TargetID = pSession->GetTargetID();
	m_Neo.MyEntityID = pSession->GetMyEntityID();

	m_Session->AddUpLimit(m_UpLimit);
	m_Session->AddDownLimit(m_DownLimit);

	m_pNeo = NULL;
	connect(m_Session, SIGNAL(Activity()), this, SLOT(OnActivity()));
	connect(m_Session, SIGNAL(ProcessPacket(QString, QVariant)), this, SLOT(OnProcessPacket(QString, QVariant)), Qt::QueuedConnection);
	connect(m_Session, SIGNAL(Disconnected(int)), this, SLOT(OnDisconnected(int)));
	LogLine(LOG_DEBUG, tr("connected session (Incoming)"));
	Init();
}

CNeoClient::~CNeoClient()
{
	if(m_Session)
		theCore->m_NeoManager->CloseSession(m_Neo, m_Session);
}

void CNeoClient::Init()
{
	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("NeoShare/ConnectTimeout"));
	m_uNextKeepAlive = -1;
	m_bHandshakeSent = false;
	m_bHandshakeRecived = false;
	m_UploadedSize = 0;
	m_DownloadedSize = 0;
}

void CNeoClient::Process(UINT Tick)
{
	if(!IsDisconnected())
	{
		if(m_uTimeOut < GetCurTick())
			Disconnect();
		else if(m_uNextKeepAlive < GetCurTick())
			KeepAlive();
		else if(m_pNeo) // Note: pocess is called form the manager also for not yet fully handshaked cleints!
		{

		}
	}
}

QString CNeoClient::GetUrl()
{
	QByteArray TargetID = m_Neo.TargetID;
	std::reverse(TargetID.begin(), TargetID.end());
	return QString("neo://%1@%2/").arg(QString::fromLatin1(m_Neo.EntityID.toHex())).arg(QString::fromLatin1(TargetID.toHex()).toUpper());
}

QString CNeoClient::GetConnectionStr()
{
	if(IsDisconnected())
		return "Disconnected";
	if(!IsConnected())
		return "Connecting";
	return "Connected";
}

CFile* CNeoClient::GetFile()
{
	if(m_pNeo)
		return m_pNeo->GetFile();
	return NULL;
}

bool CNeoClient::Connect()
{
	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("NeoShare/ConnectTimeout"));
	m_uNextKeepAlive = -1;
	m_bHandshakeSent = false;
	m_bHandshakeRecived = false;

	ASSERT(m_Session == NULL);
	m_Session = theCore->m_NeoManager->OpenSession(m_Neo);
	if(!m_Session)
	{
		m_Error = "ConnectFailed";
		return false;
	}

	m_Session->AddUpLimit(m_UpLimit);
	m_Session->AddDownLimit(m_DownLimit);
	
	connect(m_Session, SIGNAL(Connected()), this, SLOT(OnConnected()));
	connect(m_Session, SIGNAL(Activity()), this, SLOT(OnActivity()));
	connect(m_Session, SIGNAL(ProcessPacket(QString, QVariant)), this, SLOT(OnProcessPacket(QString, QVariant)), Qt::QueuedConnection);
	connect(m_Session, SIGNAL(Disconnected(int)), this, SLOT(OnDisconnected(int)));
	return true;
}

void CNeoClient::OnConnected()
{
	LogLine(LOG_DEBUG, tr("connected session (outgoing)"));
	SendHandshake();
}

void CNeoClient::Disconnect()
{
	if(m_pNeo && m_pNeo->IsActiveUpload())
	{
		m_pNeo->LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb disconnecting due to timeout")
			.arg(m_pNeo->GetFile()->GetFileName()).arg(m_pNeo->GetDisplayUrl()).arg((double)m_UploadedSize/1024.0));
		m_pNeo->StopUpload();
	}

	ASSERT(m_Session);
	theCore->m_NeoManager->CloseSession(m_Neo, m_Session);
}

void CNeoClient::OnDisconnected(int Error)
{
	ASSERT(m_Session);

	if(m_Error.isEmpty() && m_bHandshakeRecived == false)
	{
		m_Error = "Not Connected";
	}

	if(m_pNeo && m_pNeo->IsActiveUpload())
	{
		m_pNeo->LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb session disconnected, reason: %4")
			.arg(m_pNeo->GetFile()->GetFileName()).arg(m_pNeo->GetDisplayUrl()).arg((double)m_UploadedSize/1024.0).arg(Error ? "Error" : 0));
		m_pNeo->StopUpload();
	}

	//if(m_pNeo)
	//{
	//	CFile* pFile = m_pNeo->GetFile();
	//	ASSERT(pFile);
	//	m_Session->RemoveUpLimit(pFile->GetUpLimit());
	//	m_Session->RemoveDownLimit(pFile->GetDownLimit());
	//
	//	m_Session->RemoveUpLimit(m_pNeo->GetUpLimit());
	//	m_Session->RemoveDownLimit(m_pNeo->GetDownLimit());
	//}

	m_Session = NULL;
}

// Handshake

void CNeoClient::SendHandshake()
{
	LogLine(LOG_DEBUG, tr("SendHandshake"));

	m_bHandshakeSent = true;
	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("NeoShare/ConnectTimeout"));

	QVariantMap OutPacket;
	OutPacket["CS"] = theCore->m_NeoManager->GetVersion(); // Client Software
	OutPacket["PV"] = 1; // Protocol Version

	SendPacket("HS", OutPacket);
}

void CNeoClient::ProcessHandshake(const QVariantMap& InPacket)
{
	m_bHandshakeRecived = true;

	m_Software = InPacket["CS"].toString();
#ifndef NO_LEGACY
	if (m_Software.isEmpty())
	{
		m_Software = InPacket["CV"].toString();

		StrPair CV = Split2(m_Software, " ");
		if (CV.first == "NL")
			m_Software = "NeoLoader v" + CV.second;
	}
#endif

	if(InPacket["PV"].toInt() < 1)
		throw "OV"; // OldVersion
}

void CNeoClient::RequestFile()
{
	LogLine(LOG_DEBUG, tr("SendRequestFile"));

	ASSERT(m_pNeo);

	CFileHashPtr pFileHash = m_pNeo->GetHash();
	ASSERT(pFileHash);

	QVariantMap OutPacket;
	OutPacket["HF"] = CFileHash::HashType2Str(pFileHash->GetType()); // HashType
	OutPacket["HV"] = pFileHash->GetHash(); // HashValue
	
	SendPacket("FR", OutPacket);
}

void CNeoClient::ProcessFileRequest(const QVariantMap& InPacket)
{
	LogLine(LOG_DEBUG, tr("ProcessFileRequest"));

	if(!InPacket.contains("HF") || !InPacket.contains("HV")) // HF - HashType // HV - HashValue
		throw "MA"; // missing argument

	EFileHashType HashType = CFileHash::Str2HashType(InPacket["HF"].toString());
	if(HashType == HashNone)
		throw "HI"; // Hash Incompatible

	CFileHash FileHash(HashType);
	FileHash.SetHash(InPacket["HV"].toByteArray());
	if(!FileHash.IsValid())
		throw "HB"; // Hash Bad (Invalid)

	if(!m_pNeo)
	{
		if(!theCore->m_NeoManager->DispatchClient(this, &FileHash))
			throw "FNF"; // file not found
		ASSERT(m_pNeo); // it eider has to have been set or we head to throw an error
		m_pNeo->OnConnected();
	}
	else if(!FileHash.Compare(m_pNeo->GetHash().data()))
		throw "PV"; // protocol violation - we are not allowed to swap files

	SendFileInfo();
}

void CNeoClient::SendFileInfo()
{
	LogLine(LOG_DEBUG, tr("SendFileInfo"));

	ASSERT(m_pNeo);

	CFile* pFile = m_pNeo->GetFile();

	QVariantMap OutPacket;
	if(uint64 uFileSize = pFile->GetFileSize())
		OutPacket["FS"] = uFileSize;
	OutPacket["FN"] = pFile->GetFileName();

	OutPacket["HM"] = theCore->m_NeoManager->WriteHashMap(pFile->GetAllHashes(true));

	SendPacket("FI", OutPacket);
}

void CNeoClient::ProcessFileInfo(const QVariantMap& InPacket)
{
	LogLine(LOG_DEBUG, tr("ProcessFileInfo"));
			
	if(m_Neo.IsHub) // initialise keep alive
		m_uNextKeepAlive = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("NeoShare/KeepAlive"));

	CFile* pFile = m_pNeo->GetFile();

	QString FileName = InPacket["FN"].toString();
	if(pFile->GetFileName().isEmpty())
		pFile->SetFileName(FileName);

	uint64 uFileSize = InPacket["FS"].toULongLong();
	if(pFile->GetFileSize() == 0)
		pFile->SetFileSize(uFileSize);

	QVariantMap HashMap = InPacket["HM"].toMap();

	QList<CFileHashPtr> Hashes = theCore->m_NeoManager->ReadHashMap(HashMap, pFile->GetFileSize() ? pFile->GetFileSize() : uFileSize);
	theCore->m_NeoManager->AddAuxHashes(pFile, Hashes);

	ASSERT(m_pNeo->GetHash());
	CFileHashEx* pHash = qobject_cast<CFileHashEx*>(m_pNeo->GetHash().data());
	if (m_pNeo->GetHash()->GetType() != HashTorrent && pHash && !pHash->CanHashParts())
	{
		// N-ToDo: dont request from everyone make a smart req mgmt
		RequestHashdata();
	}

	if(pFile->MetaDataMissing())
	{
		connect(pFile, SIGNAL(MetadataLoaded()), this, SLOT(OnMetadataLoaded()));

		RequestMetadata();
	}
	else
	{
		if (m_pNeo->GetHash()->GetType() == HashTorrent)
		{
			CTorrent* pTorrent = pFile->GetTorrent(m_pNeo->GetHash()->GetHash());
			ASSERT(pTorrent);
			if(pTorrent->GetInfo()->IsEmpty())
				RequestMetadata();
		}

		RequestFileStatus();
	}
}

void CNeoClient::OnMetadataLoaded()
{
	disconnect(this, SLOT(OnMetadataLoaded()));

	RequestFileStatus();
}

// 

// FileStatus

void CNeoClient::RequestFileStatus()
{
	LogLine(LOG_DEBUG, tr("RequestFileStatus"));

	QVariantMap OutPacket;
	SendPacket("SR", OutPacket);
}

void CNeoClient::ProcessFileStatusRequest(const QVariantMap& InPacket)
{
	LogLine(LOG_DEBUG, tr("ProcessFileStatusRequest"));

	CFile* pFile = m_pNeo->GetFile();

	QVariantMap OutPacket;
	if(!pFile->IsComplete())
	{
		QBitArray BitField;
		CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(m_pNeo->GetHash().data());
		if(pHashEx && pHashEx->CanHashParts())
		{
			uint64 PartSize = pHashEx->GetPartSize();
			if(!PartSize)
				PartSize = pHashEx->GetBlockSize();

			if(PartSize && pFile->GetFileSize())
			{
				uint32 PartCount = DivUp(pFile->GetFileSize(), PartSize);
				int JoinBlocks = 1;
	
				for (;PartCount > 8*1024;) // we dont want bit maps to be larger than 1 KB bytes;
				{
					JoinBlocks *= 2;
					PartCount = DivUp(pFile->GetFileSize(), PartSize * JoinBlocks);
				}
			
				QBitArray StatusMap = pHashEx->GetStatusMap();
				if(!StatusMap.isEmpty())
				{
					OutPacket["PS"] = PartSize;

					if(JoinBlocks == 1)
						BitField = StatusMap;
					else
					{
						BitField.resize(PartCount);
						int Index = 0;
						for(uint32 i=0; i < StatusMap.count(); i += JoinBlocks)
							BitField.setBit(Index++, testBits(StatusMap, QPair<uint32, uint32>(i, Min(i + JoinBlocks, StatusMap.size()))));
						ASSERT(Index = PartCount);
					}
				}
			}
		}
		if(BitField.isEmpty())
			BitField = QBitArray(1, 0);

		OutPacket["PC"] = (uint32)BitField.size();
		OutPacket["PM"] = CShareMap::Bits2Bytes(BitField);
	}
	SendPacket("SA", OutPacket);
}

void CNeoClient::ProcessFileStatus(const QVariantMap& InPacket)
{
	LogLine(LOG_DEBUG, tr("ProcessFileStatus"));

	CFile* pFile = m_pNeo->GetFile();

	QBitArray BitField;
	uint64 PartSize;
	if(uint32 PartCount = InPacket["PC"].toUInt()) // filled partmap means part incomplete
	{
		if(PartCount == 1)
			PartSize = pFile->GetFileSize();
		else
		{
			PartSize = InPacket["PS"].toULongLong();

			uint32 BlockCount = DivUp(pFile->GetFileSize(), PartSize);
			ASSERT(BlockCount % PartCount == 0); // a part must be a multiple of of blocks
			PartSize = PartSize * (BlockCount / PartCount);
		}

		BitField.resize(PartCount);
		BitField = CShareMap::Bytes2Bits(InPacket["PM"].toByteArray(), PartCount);
	}
	else // no part map means file is complete
	{
		PartSize = pFile->GetFileSize();
		BitField.resize(1);
		BitField.fill(true);
	}
	if(!PartSize) // We should have head not requested the status map if we dont have the metadata yet
		throw "PV";
	m_pNeo->PartsAvailable(BitField, PartSize);
}

// 

// Hashdata

void CNeoClient::RequestHashdata()
{
	LogLine(LOG_DEBUG, tr("RequestHashdata"));

	ASSERT(m_pNeo);

	CFileHashPtr pFileHash = m_pNeo->GetHash();
	ASSERT(pFileHash);

	QVariantMap OutPacket;
	OutPacket["HF"] = CFileHash::HashType2Str(pFileHash->GetType()); // HashType
	OutPacket["HV"] = pFileHash->GetHash(); // HashValue
	
	// ToDo: Specify more tetails if applicable

	SendPacket("HR", OutPacket);
}

void CNeoClient::ProcessHashdataRequest(const QVariantMap& InPacket)
{
	LogLine(LOG_DEBUG, tr("ProcessHashdataRequest"));

	CFile* pFile = m_pNeo->GetFile();

	EFileHashType HashType = CFileHash::Str2HashType(InPacket["HF"].toString());
	if(HashType == HashNone)
		throw "HI"; // Hash Incompatible

	CFileHashPtr pFileHash = pFile->GetHashPtrEx(HashType, InPacket["HV"].toByteArray());
	if(!pFileHash)
		throw "HU"; // Hash Unknown

	QVariantMap OutPacket;

	OutPacket["HF"] = InPacket["HF"];
	OutPacket["HV"] = InPacket["HV"];

	if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFileHash.data()))
	{
		OutPacket["HT"] = pHashTree->SaveBin();
			
		if(CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(pFileHash.data()))
			OutPacket["MH"] = pHashTreeEx->GetMetaHash(); // MetaHash
	}
	else if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFileHash.data())) // Note this code could also handle ed2k hashsets
		OutPacket["HS"] = pHashSet->SaveBin(); // HashSet

	SendPacket("HA", OutPacket);
}

void CNeoClient::ProcessHashdata(const QVariantMap& InPacket)
{
	LogLine(LOG_DEBUG, tr("ProcessHashdata"));

	CFile* pFile = m_pNeo->GetFile();

	EFileHashType HashType = CFileHash::Str2HashType(InPacket["HF"].toString());
	if(HashType == HashNone)
		throw "HI"; // Hash Incompatible

	CFileHashPtr pFileHash = pFile->GetHashPtrEx(HashType, InPacket["HV"].toByteArray());
	if(!pFileHash)
		throw "HU"; // Hash Unknown

	try
	{
		if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pFileHash.data()))
		{
			if(CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(pFileHash.data()))
				pHashTreeEx->SetMetaHash(InPacket["MH"].toByteArray()); // MetaHash

			// Note: MetaData must be set befoure loading tree, or else load will fail!!
			if(!pHashTree->IsComplete())
			{
				if(!pHashTree->LoadBin(InPacket["HT"].toByteArray())) // HashTree
					throw CException(LOG_DEBUG, L"HashTree couldn't be loaded");
			}
		}
		else if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pFileHash.data()))
		{
			if(!pHashSet->LoadBin(InPacket["HS"].toByteArray())) // HashSet
				throw CException(LOG_DEBUG, L"HashSet couldn't be loaded");
		}
		else {
			ASSERT(0); // we shouldn't done that for this
		}

		theCore->m_Hashing->SaveHash(pFileHash.data()); // X-TODO-P: this is misplaced here
	}
	catch(const CException& Exception) 
	{
		LogLine(Exception.GetFlag(), tr("recived malformated packet; %1").arg(QString::fromStdWString(Exception.GetLine())));
	}
}

//

// Metadata

void CNeoClient::RequestMetadata()
{
	LogLine(LOG_DEBUG, tr("RequestMetadata"));

	ASSERT(m_pNeo);

	CFile* pFile = m_pNeo->GetFile();

	CFileHashPtr pFileHash = m_pNeo->GetHash();
	ASSERT(pFileHash);

	QVariantMap OutPacket;
	OutPacket["HF"] = CFileHash::HashType2Str(pFileHash->GetType()); // HashType
	OutPacket["HV"] = pFileHash->GetHash(); // HashValue
	
	int Index = -1;
	switch(pFileHash->GetType())
	{
		case HashXNeo:
			Index = theCore->m_NeoManager->NextMetadataBlock(pFile, m_Neo.EntityID);
			break;
		case HashTorrent:
		{
			CTorrent* pTorrent = pFile->GetTorrent(pFileHash->GetHash());
			ASSERT(pTorrent);
			Index = pTorrent->NextMetadataBlock(m_Neo.EntityID);
			break;
		}
	}
	if(Index == -1)
		return;

	OutPacket["MI"] = Index; // MetadataIndex

	SendPacket("MR", OutPacket);
}

void CNeoClient::ProcessMetadataRequest(const QVariantMap& InPacket)
{
	LogLine(LOG_DEBUG, tr("ProcessMetadataRequest"));

	CFile* pFile = m_pNeo->GetFile();

	EFileHashType HashType = CFileHash::Str2HashType(InPacket["HF"].toString());
	if(HashType == HashNone)
		throw "HI"; // Hash Incompatible

	CFileHashPtr pFileHash = pFile->GetHashPtrEx(HashType, InPacket["HV"].toByteArray());
	if(!pFileHash)
		throw "HU"; // Hash Unknown

	QVariantMap OutPacket;

	OutPacket["HF"] = InPacket["HF"];
	OutPacket["HV"] = InPacket["HV"];

	int Index = InPacket["MI"].toInt();
	OutPacket["MI"] = Index;

	switch(pFileHash->GetType())
	{
		case HashXNeo:
		{
			OutPacket["MC"] = pFile->GetSubFiles().size(); 
			OutPacket["ME"] = theCore->m_NeoManager->GetMetadataEntry(pFile, Index);
			break;
		}
		case HashTorrent:
		{
			CTorrent* pTorrent = pFile->GetTorrent(pFileHash->GetHash());
			if(pTorrent && !pTorrent->GetInfo()->IsEmpty())
			{
				OutPacket["MS"] = pTorrent->GetInfo()->GetMetadataSize(); // MetadataSize
				OutPacket["MB"] = pTorrent->GetInfo()->GetMetadataBlock(Index); //MetadataBlock
			}
			break;
		}
	}
	
	SendPacket("MA", OutPacket);
}

void CNeoClient::ProcessMetadata(const QVariantMap& InPacket)
{
	LogLine(LOG_DEBUG, tr("ProcessMetadata"));

	CFile* pFile = m_pNeo->GetFile();

	EFileHashType HashType = CFileHash::Str2HashType(InPacket["HF"].toString());
	if(HashType == HashNone)
		throw "HI"; // Hash Incompatible

	CFileHashPtr pFileHash = pFile->GetHashPtrEx(HashType, InPacket["HV"].toByteArray());
	if(!pFileHash)
		throw "HU"; // Hash Unknown

	int Index = InPacket["MI"].toInt();

	switch(pFileHash->GetType())
	{
		case HashXNeo:
		{
			if(int EntryCount = InPacket["MC"].toULongLong())
				theCore->m_NeoManager->AddMetadataBlock(pFile, Index, InPacket["ME"], EntryCount, m_Neo.EntityID);
			else
				theCore->m_NeoManager->ResetMetadataBlock(pFile, Index, m_Neo.EntityID);
			break;
		}
		case HashTorrent:
		{
			CTorrent* pTorrent = pFile->GetTorrent(pFileHash->GetHash());
			ASSERT(pTorrent);
			if(uint64 uTotalSize = InPacket["MS"].toULongLong())
				pTorrent->AddMetadataBlock(Index, InPacket["MB"].toByteArray(), uTotalSize, m_Neo.EntityID);
			else
				pTorrent->ResetMetadataBlock(Index, m_Neo.EntityID);
			break;
		}
	}

	RequestMetadata();
}

//

// Transfer

void CNeoClient::RequestDownload()
{
	LogLine(LOG_DEBUG, tr("SendRequestDownload"));

	QVariantMap OutPacket;
	SendPacket("DR", OutPacket);
}

void CNeoClient::SendQueueStatus(bool bAccepted)
{
	LogLine(LOG_DEBUG, tr("SendQueueStatus"));

	ASSERT(m_Session);
	m_Session->SetUpload(false);

	QVariantMap OutPacket;
	OutPacket["QR"] = bAccepted ? 1 : 0;
	SendPacket("QS", OutPacket);
}

void CNeoClient::OfferUpload()
{
	LogLine(LOG_DEBUG, tr("SendUploadOffer"));

	m_UploadedSize = 0;
	ASSERT(m_Session);
	m_Session->SetUpload(true);

	QVariantMap OutPacket;
	SendPacket("UO", OutPacket);
}

void CNeoClient::RequestBlock(uint64 uOffset, uint32 uLength)
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("SendBlockRequest"));

	ASSERT(m_pNeo);

	m_PendingRanges.insert(uOffset, uLength);

	QVariantMap OutPacket;
	OutPacket["OFF"] = uOffset;
	OutPacket["LEN"] = uLength;
	SendPacket("BR", OutPacket);
}

void CNeoClient::ProcessBlockRequest(const QVariantMap& InPacket)
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("ProcessBlockRequest"));

	uint64 uOffset = InPacket["OFF"].toULongLong();
	uint32 uLength = InPacket["LEN"].toUInt();

	ASSERT(m_pNeo);
	if(!m_pNeo->IsActiveUpload())
	{
		RejectBlock(uOffset, uLength);
		return;
	}

	theCore->m_IOManager->ReadData(this, m_pNeo->GetFile()->GetFileID(), uOffset, uLength);
}

void CNeoClient::RejectBlock(uint64 uOffset, uint32 uLength)
{
	LogLine(LOG_DEBUG, tr("SendRequestRejected"));

	QVariantMap OutPacket;
	OutPacket["OFF"] = uOffset;
	OutPacket["LEN"] = uLength;
	SendPacket("RR", OutPacket);
}

void CNeoClient::OnDataRead(uint64 uOffset, uint64 uLength, const QByteArray& Data, bool bOk, void* Aux)
{
	ASSERT(m_pNeo);

	m_UploadedSize += uLength;

	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("SendDataBlock"));

	QVariantMap OutPacket;
	OutPacket["OFF"] = uOffset;
	OutPacket["DATA"] = Data;

	if(m_Session)
		m_Session->QueuePacket("DB", OutPacket);

	m_DownloadedSize += uLength;

	m_pNeo->OnBytesWritten(uLength);
	theCore->m_NeoManager->OnBytesWritten(uLength);
}

void CNeoClient::ProcessRequestRejected(const QVariantMap& InPacket)
{
	LogLine(LOG_DEBUG, tr("ProcessRequestRejected"));

	uint64 uOffset = InPacket["OFF"].toULongLong();
	uint32 uLength = InPacket["LEN"].toUInt();

	uint64 uReqLength = m_PendingRanges.take(uOffset);
	if(uReqLength == 0) // we havnt requested that
		return;
	else if(uReqLength > uLength) // only a part was calceled
		m_PendingRanges.insert(uOffset + uLength, uReqLength - uLength);
	else if(uReqLength < uLength) // to much was canceled
	{
		uLength = uReqLength;
		ASSERT(0);
	}

	m_pNeo->ReleaseRange(uOffset, uOffset + uLength);
}

void CNeoClient::ProcessDataBlock(const QVariantMap& InPacket)
{
	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug == 3)
		LogLine(LOG_DEBUG, tr("ProcessDataBlock"));

	ASSERT(m_pNeo);
	CFile* pFile = m_pNeo->GetFile();
	// Note: in endgame mode we might requested blocks twice, prevent overwritign recived data
	if(!pFile->IsIncomplete())
		return; 

	uint64 uOffset = InPacket["OFF"].toULongLong();
	QByteArray Data = InPacket["DATA"].toByteArray();
	uint64 uLength = Data.size();

	uint64 uReqLength = m_PendingRanges.take(uOffset);
	if(uReqLength == 0) // we havnt requested that
		return;
	else if(uReqLength > uLength) // we didnt get all
		m_PendingRanges.insert(uOffset + uLength, uReqLength - uLength);
	else if(uReqLength < uLength) // we got to much
	{
		uLength = uReqLength;
		Data.truncate(uReqLength);
		ASSERT(0);
	}

	m_pNeo->RangeReceived(uOffset, uOffset + Data.length(), Data);
	m_pNeo->OnBytesReceived(Data.length());
	theCore->m_NeoManager->OnBytesReceived(Data.length());
	
	// request more
	m_pNeo->RequestBlocks();
}

//

void CNeoClient::KeepAlive()
{
	SendPacket("", QVariantMap());

	// schedule next keep alive
	m_uNextKeepAlive = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("NeoShare/KeepAlive"));
}

bool CNeoClient::SendPacket(const QString& Name, const QVariantMap& Packet)
{
	ASSERT(m_Session);
	if(!m_Session)
		return false;
	m_Session->SendPacket(Name, Packet);
	return true;
}

void CNeoClient::OnActivity()
{
	m_uTimeOut = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("NeoShare/IdleTimeout"));
}

void CNeoClient::OnProcessPacket(QString Name, QVariant Data)
{
	QVariantMap InPacket = Data.toMap();

	if(Name.isEmpty())
		return; // Keep Alive Packet

	try
	{
		if(Name == "HS") // <-> Handshake 
		{
			LogLine(LOG_DEBUG, tr("ProcessHandShake"));

			ProcessHandshake(InPacket);

			if(!m_bHandshakeSent)
				SendHandshake();

			if(m_pNeo)
				m_pNeo->OnConnected();
		}
		else if(Name == "FR") // -> FileRequest
		{
			ProcessFileRequest(InPacket);
		}
		else if(Name == "FI") // <- FileInfo
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			ProcessFileInfo(InPacket);
		}
		else if(Name == "SR") // -> StatusRequest
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			ProcessFileStatusRequest(InPacket);
		}
		else if(Name == "SA") // <- StatusAnswer
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			ProcessFileStatus(InPacket);
		}
		else if(Name == "HR") // -> HashdataRequest
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			ProcessHashdataRequest(InPacket);
		}
		else if(Name == "HA") // <- HashdataAnswer
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			ProcessHashdata(InPacket);
		}
		else if(Name == "MR") // -> MetadataRequest
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			ProcessMetadataRequest(InPacket);
		}
		else if(Name == "MA") // <- MetadataAnswer
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			ProcessMetadata(InPacket);
		}
		else if(Name == "DR") // -> DownloadRequest - request upload
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			LogLine(LOG_DEBUG, tr("ProcessDownloadRequest"));

			m_pNeo->DownloadRequested();
		}
		else if(Name == "QS") // <- QueueStatus - choke - tel the current uplaod status so the client knows when to expec an upload offer
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			LogLine(LOG_DEBUG, tr("ProcessQueueStatus"));
			if(m_Session)
				m_Session->SetDownload(false);
			m_pNeo->QueueStatus(InPacket["QR"].toInt() != 0);
		}
		else if(Name == "UO") // <- UploadOffer - unchoke - offer uplaod to client
		{
			if(!m_pNeo) throw "PV"; // protocol violation

			LogLine(LOG_DEBUG, tr("ProcessUploadOffer"));
			m_DownloadedSize = 0;
			if(m_Session)
				m_Session->SetDownload(true);
			m_pNeo->UploadOffered();
		}
		else if(Name == "BR") // -> BlockRequest - request next data block
		{
			if(!m_pNeo) throw "PV";

			ProcessBlockRequest(InPacket);
		}
		else if(Name == "RR") // <- RequestRejected - block with file data
		{
			if(!m_pNeo) throw "PV";

			ProcessRequestRejected(InPacket);
		}
		else if(Name == "DB") // <- DataBlock - block with file data
		{
			if(!m_pNeo) throw "PV";

			ProcessDataBlock(InPacket);
		}
		else if(Name == "ERR")
		{
			LogLine(LOG_DEBUG, tr("Recived Error from Neo Entity: %1").arg(InPacket["EC"].toString()));

			if(InPacket["EC"] == "FNF")
			{
				m_Error = "FileNotFound";
				Disconnect();
			}
		}
		else
			LogLine(LOG_DEBUG, tr("Recived unknwon Neo packet %1").arg(Name));
	}
	catch(const CException& Exception) // should not happen but could, when binary reading part maps or hash sets
	{
		LogLine(Exception.GetFlag(), tr("recived malformated packet; %1").arg(QString::fromStdWString(Exception.GetLine())));
		
		QVariantMap OutPacket;
		OutPacket["EC"] = "FE"; // fatal error
		SendPacket("ERR", OutPacket);
	}
	catch(const char* Err)
	{
		LogLine(LOG_DEBUG, tr("Neo Entity caused an error: %1").arg(Err));

		QVariantMap OutPacket;
		OutPacket["EC"] = Err; // error Code
		SendPacket("ERR", OutPacket);
	}
}

void CNeoClient::AddLogLine(time_t uStamp, uint32 uFlag, const CLogMsg& Line)
{
	Line.AddMark((uint64)m_pNeo);

	int iDebug = theCore->Cfg()->GetInt("Log/Level");
	if(iDebug >= 2 || (iDebug == 1 && (uFlag & LOG_DEBUG) == 0))
	{
		Line.Prefix(QString("NeoEntity - %1@%2").arg(QString::fromLatin1(m_Neo.EntityID.toHex())).arg(QString::fromLatin1(m_Neo.TargetID.toHex())));
		QObjectEx::AddLogLine(uStamp, uFlag | LOG_MOD('s'), Line);
	}
}
