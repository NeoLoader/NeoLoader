#include "GlobalHeader.h"
#include "NeoEntity.h"
#include "NeoClient.h"
#include "NeoManager.h"
#include "../../NeoCore.h"
#include "../../FileList/FileStats.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"
#include "../../Networking/BandwidthControl/BandwidthLimiter.h"
#include "../../FileList/IOManager.h"
#include "../UploadManager.h"

CNeoEntity::CNeoEntity(CFileHashPtr& pHash)
{
	Init();
	m_pHash = pHash;
}

CNeoEntity::CNeoEntity(CFileHashPtr& pHash, const SNeoEntity& Neo)
{
	Init();
	m_pHash = pHash;
	m_Neo = Neo;
}

void CNeoEntity::Init()
{
	m_pClient = NULL;

	m_Status.Bits = 0;
	m_NextDownloadRequest = 0;
	m_LastUploadRequest = 0;
	m_RequestVolume = 16;
}

CNeoEntity::~CNeoEntity()
{
	ASSERT(m_pClient == NULL);
}

bool CNeoEntity::Process(UINT Tick)
{
	if(m_pClient && m_pClient->IsDisconnected())
	{
		Disconnect();

		if(HasError())
			return false;
	}

	if(!theCore->m_NeoManager->GetKad()->IsConnected())
		return true; // we cant do anythign without the KAD

	CFile* pFile = GetFile();

	if(pFile->IsIncomplete())
	{
		if(m_NextDownloadRequest < GetCurTick())
		{
			if(m_pClient && m_pClient->IsConnected())
			{
				SetChecked();
				m_pClient->RequestDownload();
			}
			else if(!m_pClient)
				Connect();
		}
	}

	// upload stopper
	if(IsActiveUpload())
	{
		if(m_pClient && m_pClient->GetUploadedSize() >= MB2B(18) && theCore->m_UploadManager->GetWaitingCount() > 0) // U-ToDo-Now: customize
		{
			LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb upload finished")
				.arg(pFile->GetFileName()).arg(GetDisplayUrl()).arg((double)m_pClient->GetUploadedSize()/1024.0));
			StopUpload();
		}
	}

	return CP2PTransfer::Process(Tick);
}

QString CNeoEntity::GetUrl() const
{
	QByteArray TargetID = m_Neo.TargetID;
	std::reverse(TargetID.begin(), TargetID.end());
	return QString("neo://%1@%2/%3").arg(QString::fromLatin1(m_Neo.EntityID.toHex())).arg(QString::fromLatin1(TargetID.toHex()).toUpper()).arg(CFileHash::HashType2Str(m_pHash->GetType()) + ":" + m_pHash->ToString());
}

QString CNeoEntity::GetConnectionStr() const
{
	if(m_pClient)
		return m_pClient->GetConnectionStr();
	return "";
}

bool CNeoEntity::Connect()
{
	ASSERT(m_pClient == NULL);

	CNeoClient* pClient = new CNeoClient(this, theCore->m_NeoManager);
	theCore->m_NeoManager->AddConnection(pClient);

	if(!pClient->Connect())
	{
		m_Error = "NoConnection";
		return false;
	}

	// Connect signals
    AttacheClient(pClient);
	return true;
}

void CNeoEntity::AttacheClient(CNeoClient* pClient)
{
	ASSERT(m_pClient == NULL);

	m_pClient = pClient;
	m_pClient->SetNeoEntity(this);

	CNeoSession* pSession = m_pClient->GetSession();
	ASSERT(pSession); // connect must  be in progress or already connected

	CFile* pFile = GetFile();
	pSession->AddUpLimit(pFile->GetUpLimit());
	pSession->AddDownLimit(pFile->GetDownLimit());
}

void CNeoEntity::Hold(bool bDelete)
{
	if(m_NextDownloadRequest != 0)
		m_NextDownloadRequest = 0;

	if(m_pClient) 
	{
		if(!m_pClient->IsDisconnected())
			m_pClient->Disconnect(); 
		
		Disconnect();

		if(m_ReservedSize > 0)
			ReleaseRange();
	}
}

void CNeoEntity::Disconnect()
{
	if(!m_pClient)
		return; // already disconnected

	// Remove the host from our list of known entities if the connection failed.
	if (m_pClient->HasError())
		SetError("ClientError: " + m_pClient->GetError());
	else if (!m_pClient->HasReceivedHandShake())
		SetError("ClientError: Not Connected");

	DetacheClient();
}

void CNeoEntity::DetacheClient()
{
	ASSERT(m_pClient);

	theCore->m_NeoManager->RemoveConnection(m_pClient);

	ASSERT(m_pClient->GetSession() == NULL);

	// Delete the m_pClient later.
	m_pClient->SetNeoEntity(NULL);
	delete m_pClient;
	m_pClient = NULL;

	m_Status.Flags.Downloading = false;
	m_Status.Flags.Uploading = false;

	ReleaseRange();
}

CBandwidthLimit* CNeoEntity::GetUpLimit()
{
	return m_pClient ? m_pClient->GetUpLimit() : NULL;
}

CBandwidthLimit* CNeoEntity::GetDownLimit()
{
	return m_pClient ? m_pClient->GetDownLimit() : NULL;
}

void CNeoEntity::OnConnected()
{
	ASSERT(m_pClient);

	// get all connection info
	m_Neo = m_pClient->GetNeo();
	m_Software = m_pClient->GetSoftware();

	SetChecked(); // actually we wonr ask if we are compelte of source is nnp
	m_pClient->RequestFile(); // this requests file info not file download

	if(IsActiveUpload())
		m_pClient->OfferUpload();
}

bool CNeoEntity::CheckInterest()
{
	if(CTransfer::CheckInterest())
		m_Status.Flags.HasNeededParts = true;
	else
		m_Status.Flags.HasNeededParts = false;

	if(m_Status.Flags.HasNeededParts)
	{
		m_pClient->RequestDownload();
		return true;
	}
	return false;
}

void CNeoEntity::SetChecked()
{
	m_NextDownloadRequest = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("NeoShare/RequestInterval"));
}

void CNeoEntity::DownloadRequested()
{
	m_Status.Flags.RequestedUpload = true;
	m_Status.Flags.OnLocalQueue = true; // N-ToDo-Now: dont accet anyone to queue
	m_LastUploadRequest = GetCurTick();
	
	m_pClient->SendQueueStatus(m_Status.Flags.RequestedUpload);
}

void CNeoEntity::QueueStatus(bool bAccepted)
{
	if(m_Status.Flags.Downloading == true)
	{
		m_Status.Flags.Downloading = false;

		m_DownloadStartTime = 0;

		ReleaseRange();
	}

	m_Status.Flags.OnRemoteQueue = bAccepted;
}

bool CNeoEntity::StartUpload()
{
	ASSERT(m_Status.Flags.RequestedUpload);

	m_Status.Flags.OnLocalQueue = false;
	m_Status.Flags.Uploading = true;

	m_UploadStartTime = GetCurTick();

	if(m_pClient && m_pClient->IsConnected())
		m_pClient->OfferUpload();
	else if(!m_pClient)
		Connect();

	return true;
}

void CNeoEntity::StopUpload(bool bStalled)
{
	if(!bStalled)
		m_Status.Flags.OnLocalQueue = true;
	m_Status.Flags.Uploading = false;

	m_UploadStartTime = 0;

	m_pClient->SendQueueStatus(m_Status.Flags.OnLocalQueue);
}

void CNeoEntity::UploadOffered()
{
	if(m_Status.Flags.Downloading == false)
	{
		m_Status.Flags.OnRemoteQueue = false;
		m_Status.Flags.Downloading = true;

		m_DownloadStartTime = GetCurTick();
	}

	if(!GetFile()->IsIncomplete())
	{
		SheduleDownload();
		RequestBlocks();
	}
}

void CNeoEntity::RequestBlocks(bool bRetry)
{
	ASSERT(m_pClient);
	ASSERT(m_Parts);

	CFile* pFile = GetFile();
	if(!m_Parts || pFile->IsComplete() || pFile->IsPaused() || theCore->m_IOManager->IsWriteBufferFull())
		return;

	CPartMap* pFileParts = pFile->GetPartMap();
	ASSERT(pFileParts);

	uint64 uAvailable = pFile->GetStatusStats(Part::Available);
	uint64 uRequested = pFile->GetStatusStats(Part::Requested);

	const uint64 uBlockSize = KB2B(64);
	int uRequestLimit = theCore->Cfg()->GetInt("NeoShare/RequestLimit");
	int uRequestVolume = Min(uRequestLimit, m_RequestVolume); // max pending blocks
	int iOldCount = m_pClient->GetRequestCount();

	CShareMap::SIterator SrcIter;
	while(m_pClient->GetRequestCount() < uRequestVolume && m_Parts->IterateRanges(SrcIter))
	{
		if((SrcIter.uState & Part::Scheduled) == 0 || (SrcIter.uState & Part::Requested) != 0) // not scheduled or already requested
			continue;
		
		CPartMap::SIterator FileIter(SrcIter.uBegin, SrcIter.uEnd);
		while(m_pClient->GetRequestCount() < uRequestVolume && pFileParts->IterateRanges(FileIter))
		{
			// Check if an other client elready finished it, and if so clear the schedule
			if((FileIter.uState & (Part::Available | Part::Cached)) != 0)
			{
				ReleaseRange(FileIter.uBegin, FileIter.uEnd);
				continue;
			}

			if((FileIter.uState & Part::Requested) != 0) // or requested
				continue; // this block has already been requested

			// Do not request to much data at once
			uint64 uSize = FileIter.uEnd - FileIter.uBegin;
			if(uSize > uBlockSize)
				uSize = uBlockSize;

			// Try not to request past a single block brder
			uint64 uBlockOffset = FileIter.uBegin % NEO_BLOCKSIZE;
			if(uBlockOffset + uSize > NEO_BLOCKSIZE)
				uSize = NEO_BLOCKSIZE - uBlockOffset;

			if(FileIter.uEnd - FileIter.uBegin > uSize)
				FileIter.uEnd = FileIter.uBegin + uSize;

			m_Parts->SetRange(FileIter.uBegin, FileIter.uEnd, Part::Requested, CShareMap::eAdd);
			pFileParts->SetRange(FileIter.uBegin, FileIter.uEnd, Part::Requested, CPartMap::eAdd);

			m_pClient->RequestBlock(FileIter.uBegin, (uint32)uSize);
		}
	}

	// Dynamically addapt request volume
	if(iOldCount == 0 && m_pClient->GetRequestCount() > 0 && m_RequestVolume < uRequestLimit) // if all was served and something requested allow requesting more on the next tick
		m_RequestVolume *= 2;
	else if(iOldCount > m_RequestVolume / 2 && m_RequestVolume > 16)
		m_RequestVolume /= 2;

	if(!bRetry && m_pClient->GetRequestCount() == 0 && SheduleDownload())
		RequestBlocks(true);
}

qint64 CNeoEntity::PendingBytes() const
{
	if(!m_pClient || !m_pClient->GetSession())
		return 0;
	return m_pClient->GetSession()->QueueSize();
}

qint64 CNeoEntity::PendingBlocks() const
{
	if(!m_pClient || !m_pClient->GetSession())
		return 0;
	return m_pClient->GetSession()->GetQueueCount();
}

qint64 CNeoEntity::LastUploadedBytes() const
{
	if(!m_pClient)
		return 0;
	return m_pClient->GetUploadedSize();
}

bool CNeoEntity::IsBlocking() const
{
	return false; // N-TODO-Now
}

qint64 CNeoEntity::RequestedBlocks() const
{
	if(!m_pClient)
		return 0;
	return m_pClient->GetRequestCount();
}

qint64 CNeoEntity::LastDownloadedBytes() const
{
	if(!m_pClient)
		return 0;
	return m_pClient->GetDownloadedSize();
}

bool CNeoEntity::IsSeed() const
{
	return (m_Parts && (m_Parts->GetRange(0, -1) & Part::Available) != 0);
}

void CNeoEntity::AskForDownload()
{
	ASSERT(m_pClient);
	if (m_pClient->IsConnected())
		m_pClient->RequestDownload();
	else
	{
		if (m_pClient->IsDisconnected())
			m_pClient->Connect();
		m_NextDownloadRequest = 0;
	}
}

QPair<const byte*, size_t> CNeoEntity::GetID() const
{
	if(m_pClient)
		return  QPair<const byte*, size_t>((byte*)m_pClient->GetNeo().EntityID.constData(), m_pClient->GetNeo().EntityID.size());;
	ASSERT(0);
	return QPair<const byte*, size_t>(NULL, 0);
}

/////////////////////////////////////////////////////////////////////////////////////
// Load/Store

QVariantMap CNeoEntity::Store()
{
	if(!theCore->Cfg()->GetBool("NeoShare/SaveEntities") || !IsChecked())
		return QVariantMap();

	QVariantMap	Transfer;

	Transfer["TargetID"] = m_Neo.TargetID;
	Transfer["EntityID"] = m_Neo.EntityID;
	Transfer["FileHash"] = CFileHash::HashType2Str(m_pHash->GetType()) + ":" + m_pHash->ToString();
	Transfer["IsHub"] = m_Neo.IsHub;

	if(m_Parts)
		Transfer["PartMap"] = m_Parts->Store();
	return Transfer;
}

bool CNeoEntity::Load(const QVariantMap& Transfer)
{
	if(Transfer.isEmpty())
		return false;
	
	m_Neo.TargetID = Transfer["TargetID"].toByteArray();
	m_Neo.EntityID = Transfer["EntityID"].toByteArray();
	StrPair TypeHash = Split2(Transfer["FileHash"].toString(), ":");
	EFileHashType Type = CFileHash::Str2HashType(TypeHash.first);
	if(Type != HashNone)
		m_pHash = GetFile()->GetHashPtrEx(Type, CFileHash::DecodeHash(Type, TypeHash.second.toLatin1()));
	if(!m_pHash)
		m_pHash = GetFile()->GetHashPtr(GetFile()->IsMultiFile() ? HashXNeo : HashNeo);
	if(!m_pHash)
		return false;
	m_Neo.IsHub = Transfer["IsHub"].toBool();

	if(Transfer.contains("PartMap"))
	{
		m_Parts = CShareMapPtr(new CShareMap(GetFile()->GetFileSize()));
		if(!m_Parts->Load(Transfer["PartMap"].toMap()))
			LogLine(LOG_WARNING, tr("MuleSource %1 contians an invalid part map").arg(GetDisplayUrl()));
		m_Parts->SetRange(0, -1, Part::Scheduled | Part::Requested, CShareMap::eClr); // Clear all old stated
	}
	return true;
}

