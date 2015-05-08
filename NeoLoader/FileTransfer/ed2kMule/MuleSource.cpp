#include "GlobalHeader.h"
#include "MuleSource.h"
#include "MuleClient.h"
#include "../../NeoCore.h"
#include "MuleManager.h"
#include "../UploadManager.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"
#include "../../Networking/BandwidthControl/BandwidthLimiter.h"
#include "MuleServer.h"
#include "../../FileList/FileStats.h"
#include "../../Networking/SocketThread.h"
#include "../../FileList/IOManager.h"
#include "../../FileList/FileDetails.h"

bool SMuleSource::SelectIP()
{
	int IPv6Support = theCore->m_MuleManager->GetIPv6Support();
	if(HasV4() && HasV6()) // are booth addresses available
	{
		if(IPv6Support == 1 && !ClosedIPv6())
			Prot = CAddress::IPv6;
		else
			Prot = CAddress::IPv4;
	}
	else if(HasV4())
		Prot = CAddress::IPv4;
	else if(HasV6() && IPv6Support != 0)
		Prot = CAddress::IPv6;
	else
		return false;
	return TCPPort != 0;
}

CMuleSource::CMuleSource()
{
	Init();
}

CMuleSource::CMuleSource(const SMuleSource& Mule)
{
	m_Mule = Mule;
	Init();
}

void CMuleSource::Init()
{
	m_pClient = NULL;

	m_Status.Bits = 0;
	m_RemoteQueueRank = 0;
	m_LocalQueueRank = 0;
	m_NextDownloadRequest = 0;
	m_LastUploadRequest = 0;

	m_RemoteRating = eNotRated;
	m_RemoteAvailability = 0;
}

CMuleSource::~CMuleSource()
{
	ASSERT(!m_Parts || m_Parts->CountLength(Part::Scheduled) == 0);

	if(m_pClient)
		m_pClient->DettacheSource(this);
}

bool CMuleSource::Process(UINT Tick)
{
	if(!m_pClient)
		AttacheClient();

	if(m_pClient->HasError())
		SetError("ClientError: " + m_pClient->GetError());
	if(HasError())
		return false;

	CFile* pFile = GetFile();

	// upload stopper
	if(m_pClient->GetUpSource() == this)
	{
		if(m_pClient->IsConnected())
		{
			if(IsActiveUpload())
			{
				uint64 uPartLimit = ED2K_PARTSIZE;
				if(m_pClient->GetHordeState() == CMuleClient::eAccepted)
				{
					double ClientRatio = double(UploadedBytes())/double(DownloadedBytes());
					double FileRatio = double(pFile->UploadedBytes())/double(pFile->DownloadedBytes());
					if(ClientRatio > FileRatio)
						uPartLimit = 0;
				}

				if(uPartLimit && m_pClient->GetSentPartSize() >= uPartLimit && theCore->m_UploadManager->GetWaitingCount() > 0)
				{
					LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb upload finished")
						.arg(pFile->GetFileName()).arg(GetDisplayUrl()).arg((double)m_pClient->GetSentPartSize()/1024.0));
					StopUpload();
				}
			}
		}
		else if(GetCurTick() - m_LastUploadRequest > MIN2MS(60)) // client hasnt asked so lets drop him
			m_pClient->CancelUpload();
	}

	return CP2PTransfer::Process(Tick);
}

void CMuleSource::OnHelloRecived()
{
	ASSERT(m_pClient);

	if(m_Status.Flags.Uploading)
		m_pClient->SendAcceptUpload();

	if(m_pClient->GetDownSource() == this && !GetFile()->IsPaused()) // are we teh downlaod source
	{
		if(GetFile()->IsIncomplete() && m_NextDownloadRequest < GetCurTick())
			ReaskForDownload();
	}

	// get all connection info (id hash ip ports server buddy )
	m_Mule = m_pClient->GetMule();
}

void CMuleSource::AskForDownload()
{
	ASSERT(m_pClient);
	if (m_pClient->IsConnected())
		ReaskForDownload();
	else 
	{
		if (m_pClient->IsDisconnected())
			m_pClient->Connect();
		m_NextDownloadRequest = 0;
	}
}

void CMuleSource::ReaskForDownload()
{
	CFile* pFile = GetFile();
	m_NextDownloadRequest = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/ReaskInterval"));
	m_pClient->ClearUDPPingRequest();
	m_RemoteQueueRank = 0; // reset Queue Rank
	m_pClient->SendFileRequest(pFile);
}

void CMuleSource::SetQueueRank(uint16 uRank, bool bUDP)
{
	m_RemoteQueueRank = uRank;
	m_Status.Flags.OnRemoteQueue = uRank > 0;

	if(bUDP) // if its UDP we successfully pinged and dont have to reask over TCP
		m_NextDownloadRequest = GetCurTick() + SEC2MS(theCore->Cfg()->GetInt("Ed2kMule/ReaskInterval"));
}

void CMuleSource::HandleComment(const QString& Description, uint8 Rating)
{
	m_RemoteComment = Description; 
	m_RemoteRating = (EMuleRating)Rating;

	QVariantMap Note;
	Note["Name"] = m_RemoteFileName;
	Note["Rating"] = Rating;
	Note["Description"] = Description;
	GetFile()->GetDetails()->Add("ed2k://" + GetUserHash().ToArray().toHex(), Note);
}

void CMuleSource::OnSocketClosed()
{
	// or callback timeout
	m_Status.Flags.Uploading = false;
	m_Status.Flags.Downloading = false;

	// Note: mules dont answer witha qRankif the client is fullQ, 
	//	so if the socket closes without us obtaining a queue rank it means we are out of queue
	if(m_RemoteQueueRank == 0 && m_pClient->MuleProtSupport())
		m_Status.Flags.OnRemoteQueue = false;

	if (m_pClient->HasError())
		SetError("ClientError: " + m_pClient->GetError());

	ReleaseRange();
}

QString CMuleSource::GetUrl() const
{
	return QString("ed2k://|source,%1:%2|").arg(m_Mule.GetIP().ToQString(true)).arg(m_Mule.TCPPort);
}

QString CMuleSource::GetDisplayUrl() const
{
	if(m_pClient)
	{
		QString Nick = m_pClient->GetNick();
		if(!Nick.isEmpty())
		{
			QUrl Url(Nick);
			if(!Url.scheme().isEmpty())
				Nick = Url.host();
			return QString("ed2k://|%3|source,%1:%2|").arg(m_Mule.GetIP().ToQString(true)).arg(m_Mule.TCPPort).arg(Nick);
		}
	}
	return GetUrl();
}

void CMuleSource::Hold(bool bDelete)
{
	if(m_NextDownloadRequest != 0)
		m_NextDownloadRequest = 0;

	if(m_pClient)
	{
		if(m_ReservedSize > 0)
			ReleaseRange();

		if(m_pClient->IsConnected())
		{
			if(m_Status.Flags.Downloading)
				m_pClient->CancelDownload();

			if(m_Status.Flags.Uploading)
				m_pClient->CancelUpload();
		}

		m_pClient->DettacheSource(this);
		m_pClient->disconnect(this);
		m_pClient = NULL;
	}
	
	ASSERT(!m_Parts || m_Parts->CountLength(Part::Scheduled) == 0);
}

bool CMuleSource::IsConnected() const
{
	return m_pClient && !m_pClient->IsDisconnected();
}

bool CMuleSource::Connect()
{
	return m_pClient && m_pClient->Connect();
}

void CMuleSource::Disconnect()
{
	if (m_pClient)
		m_pClient->Disconnect();
}

QString CMuleSource::GetConnectionStr() const
{
	if(m_pClient)
		return m_pClient->GetConnectionStr();
	return "";
}

CFileHashPtr CMuleSource::GetHash()
{
	return GetFile()->GetHashPtr(HashEd2k);
}

void CMuleSource::AttacheClient(CMuleClient* pClient)
{
	ASSERT(m_pClient == NULL);

	if(pClient == NULL) // create or get client object for this source
		pClient = theCore->m_MuleManager->GetClient(m_Mule);

	connect(pClient, SIGNAL(HelloRecived()), this, SLOT(OnHelloRecived()));
	connect(pClient, SIGNAL(SocketClosed()), this, SLOT(OnSocketClosed()));

	m_pClient = pClient;

	m_pClient->AttacheSource(this);

	// get all connection info (id hash ip ports server buddy )
	m_Mule = m_pClient->GetMule();
}

CBandwidthLimit* CMuleSource::GetUpLimit()
{
	return m_pClient && (m_pClient->GetUpSource() == this || m_pClient->GetUpSource() == NULL) ? m_pClient->GetUpLimit() : NULL;
}

CBandwidthLimit* CMuleSource::GetDownLimit()
{
	return m_pClient && (m_pClient->GetDownSource() == this || m_pClient->GetDownSource() == NULL) ? m_pClient->GetDownLimit() : NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Download

void CMuleSource::BeginDownload()
{
	m_NextDownloadRequest = 0; // make a full reask now
	if(CMuleSource* pPrev = m_pClient->GetDownSource())
	{
		m_Status.Flags.Downloading = pPrev->IsActiveDownload();
		m_Status.Flags.OnRemoteQueue = pPrev->IsWaitingDownload();
	}
	m_pClient->SetDownSource(this);
}

void CMuleSource::EndDownload()
{
	m_Status.Flags.OnRemoteQueue = false;
	m_Status.Flags.Downloading = false;

	ReleaseRange();
}

bool CMuleSource::CheckInterest()
{
	if(!m_pClient->SupportsLargeFiles() && IsLargeEd2kMuleFile(GetFile()->GetFileSize()))
	{
		LogLine(LOG_DEBUG | LOG_WARNING, tr("Client that does not support large files clames to have a large file"));
		return false;
	}

	if(CTransfer::CheckInterest())
		m_Status.Flags.HasNeededParts = true;
	else
		m_Status.Flags.HasNeededParts = false;

	return m_Status.Flags.HasNeededParts;
}

void CMuleSource::OnDownloadStart()
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

void CMuleSource::OnDownloadStop()
{
	if(m_Status.Flags.Downloading == true)
	{
		m_Status.Flags.Downloading = false;

		m_DownloadStartTime = 0;
	}

	ReleaseRange(0, -1);
}

void CMuleSource::RequestBlocks(bool bRetry)
{
	ASSERT(m_pClient);

	if(m_Parts.isNull())
		return;

	CFile* pFile = GetFile();
	if(pFile->IsComplete() || pFile->IsPaused() || theCore->m_IOManager->IsWriteBufferFull())
		return;

	CPartMap* pFileParts = pFile->GetPartMap();
	ASSERT(pFileParts);

	if(m_pClient->IncomingBlockCount() >= 3)
		return; // enogh sheduled

	const uint64 uBlockSize = ED2K_BLOCKSIZE * 3; // Note: if thats bigger most clients would drop us

	int Index = 0;
	uint64 Begins[3] = {0,0,0};
	uint64 Ends[3] = {0,0,0};

	CShareMap::SIterator SrcIter;
	while(Index < 3 && m_Parts->IterateRanges(SrcIter))
	{
		if((SrcIter.uState & Part::Scheduled) == 0 || (SrcIter.uState & Part::Requested) != 0) // not scheduled or already requested
			continue;

		CPartMap::SIterator FileIter(SrcIter.uBegin, SrcIter.uEnd);
		while(Index < 3 && pFileParts->IterateRanges(FileIter))
		{
			// Check if an other client elready finished it, and if so clear the schedule
			if((FileIter.uState & (Part::Available | Part::Cached)) != 0)
			{
				ReleaseRange(FileIter.uBegin, FileIter.uEnd);
				continue;
			}

			if((FileIter.uState & Part::Requested) != 0) // or requested
				continue; // this block has already been requested
		
			if(FileIter.uEnd - FileIter.uBegin > uBlockSize)
				FileIter.uEnd = FileIter.uBegin + uBlockSize;

			m_Parts->SetRange(FileIter.uBegin, FileIter.uEnd, Part::Requested, CShareMap::eAdd);
			pFileParts->SetRange(FileIter.uBegin, FileIter.uEnd, Part::Requested, CPartMap::eAdd);

			ASSERT(Index < ARRSIZE(Begins) && ARRSIZE(Begins)  == ARRSIZE(Ends));
			Begins[Index] = FileIter.uBegin;
			Ends[Index] = FileIter.uEnd;
			Index++;
		}
	}

	if(Index != 0)
		m_pClient->SendBlockRequest(GetFile(), Begins, Ends);
	else if(!bRetry && SheduleDownload())
		RequestBlocks(true);
}

void CMuleSource::OnCrumbComplete(uint32 Index)
{
	bool b1st;
	if(b1st = m_Parts.isNull()) // thats the first request
		m_Parts = CShareMapPtr(new CShareMap(GetFile()->GetFileSize()));

	uint64 uBegin = Index * ED2K_CRUMBSIZE;
	if(uBegin >= GetFile()->GetFileSize())
		return; // Error
	uint64 uEnd = uBegin + ED2K_CRUMBSIZE;
	if(uEnd > GetFile()->GetFileSize())
		uEnd = GetFile()->GetFileSize();

	if((m_Parts->GetRange(uBegin, uEnd) & Part::Available) == 0)
	{
		m_AvailableBytes += uEnd - uBegin;

		CAvailDiff AvailDiff;
		AvailDiff.Add(uBegin, uEnd, Part::Available, 0);
		m_Parts->SetRange(uBegin, uEnd, Part::Available | Part::Verified, CShareMap::eAdd);
		GetFile()->GetStats()->UpdateAvail(this, AvailDiff, b1st);
	}

	CheckInterest();
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Upload

void CMuleSource::RequestUpload()
{
	m_Status.Flags.RequestedUpload = true;
	m_LastUploadRequest = GetCurTick();
	m_LocalQueueRank = GetQueueRank();
	if(m_LocalQueueRank)
	{
		m_Status.Flags.OnLocalQueue = true;
		m_pClient->SendRankingInfo(m_LocalQueueRank);
	}
	// else // no answer means FullQ

	SendCommendIfNeeded();
}

void CMuleSource::SendCommendIfNeeded()
{
	// sent comment if needed
	if(!m_Status.Flags.CommentSent && m_pClient->AcceptCommentVer() > 0)
	{
		m_Status.Flags.CommentSent = true; // EM-ToDo: this flags hould be resetet when a coment gets changed

		CFile* pFile = GetFile();
		QString Description = pFile->GetProperty("Description").toString();
		uint8 Rating = pFile->GetProperty("Rating").toUInt();
		if(!Description.isEmpty() || Rating)
			m_pClient->SendComment(pFile, Description, Rating);
	}
}

uint16 CMuleSource::GetQueueRank(bool bUDP)
{
	if(bUDP) // if its a udp request update the last ask time so that the soruce dont gets dropped
		m_LastUploadRequest = GetCurTick();

	// we need this amount of upload starts to for sure be selected once
	double NeededAtempts = theCore->m_UploadManager->GetProbabilityRange() / GetProbabilityRange();
	// we wil have performed the needed amount of atemots after this time
	double NeededTime = NeededAtempts * theCore->m_UploadManager->GetAverageStartTime();

	if(GetFile()->IsPaused())
		return 0; // FullQ if file is paused

	// if client is not already on queue and would have to wait to long, tell him the queue is full
	if(!m_Status.Flags.OnLocalQueue && NeededTime > SEC2MS(theCore->Cfg()->GetInt("Upload/UpWaitTime")))
		return 0;

	if(NeededAtempts > 1)
		return NeededAtempts;
	return 1;
}

bool CMuleSource::StartUpload()
{
	ASSERT(m_Status.Flags.RequestedUpload);

	m_Status.Flags.OnLocalQueue = false;
	m_Status.Flags.Uploading = true;

	m_UploadStartTime = GetCurTick();

	if(m_pClient->IsConnected())
		m_pClient->SendAcceptUpload();
	else if(m_pClient->IsDisconnected())
		m_pClient->Connect();

	return true;
}

void CMuleSource::StopUpload(bool bStalled)
{
	if(!m_Status.Flags.Uploading)
		return;
	m_Status.Flags.Uploading = false;

	m_UploadStartTime = 0;

	if(!m_pClient->IsConnected())
		return;
	m_pClient->SendOutOfPartReqs();

	if(bStalled) // file has been stopped - or the client was stalling
		return;

	// Put client back in queue
	if(uint16 uRank = GetQueueRank())
	{
		m_Status.Flags.OnLocalQueue = true;
		m_pClient->SendRankingInfo(uRank);
	}
}

void CMuleSource::CancelUpload()
{
	m_Status.Flags.RequestedUpload = false;
	m_Status.Flags.OnLocalQueue = false;
}

QString CMuleSource::GetFileName() const
{
	if(m_RemoteFileName.isEmpty())
		return GetFile()->GetFileName();
	return m_RemoteFileName;
}

bool CMuleSource::IsComplete() const
{
	return (!m_Parts.isNull() && (m_Parts->GetRange(0, -1) & Part::Available) != 0);
}

qint64 CMuleSource::PendingBytes() const
{
	if(!m_pClient || !m_pClient->GetSocket())
		return 0;
	return m_pClient->GetSocket()->QueueSize();
}

qint64 CMuleSource::PendingBlocks() const
{
	if(!m_pClient || !m_pClient->GetSocket())
		return 0;
	return m_pClient->GetSocket()->QueueCount();
}

qint64 CMuleSource::LastUploadedBytes() const
{
	if(!m_pClient || !m_pClient->GetSocket())
		return 0;
	return m_pClient->GetSentPartSize();
}

bool CMuleSource::IsBlocking() const
{
	if(!m_pClient || !m_pClient->GetSocket())
		return 0;
	return m_pClient->GetSocket()->IsBlocking();
}

qint64 CMuleSource::RequestedBlocks() const
{
	if(!m_pClient)
		return 0;
	return m_pClient->IncomingBlockCount();
}

qint64 CMuleSource::LastDownloadedBytes() const
{
	if(!m_pClient || !m_pClient->GetSocket())
		return 0;
	return m_pClient->GetRecivedPartSize();
}

bool CMuleSource::IsSeed() const
{
	return (m_Parts && (m_Parts->GetRange(0, -1) & Part::Available) != 0);
}

bool CMuleSource::SupportsHostCache() const
{
	return m_pClient && m_pClient->SupportsHostCache();
}

QPair<const byte*, size_t> CMuleSource::GetID() const
{
	if(m_pClient)
	{
		if(CMuleSocket* pSocket = m_pClient->GetSocket())
			return QPair<const byte*, size_t>(pSocket->GetAddress().Data(), pSocket->GetAddress().Size());
	}
	ASSERT(0);
	return QPair<const byte*, size_t>(NULL, 0);
}

QString CMuleSource::GetSoftware() const
{
	if(m_pClient)
	{
		QString Software = m_pClient->GetSoftware();
		if(Software.isEmpty())
			Software = "Unknown";

		if(!m_Mule.ClosedIPv6())
			Software += " IPv6";
		if(m_Mule.HasLowID())
			Software += " LowID";

		return Software;
	}
	return "";
}

/////////////////////////////////////////////////////////////////////////////////////
// Load/Store

QVariantMap CMuleSource::Store()
{
	if(!theCore->Cfg()->GetBool("Ed2kMule/SaveSources") || !IsChecked())
		return QVariantMap();

	QVariantMap	Transfer;
	
	Transfer["ClientID"] = m_Mule.ClientID;
	Transfer["OpenIPv6"] = m_Mule.OpenIPv6;
	Transfer["UserHash"] = m_Mule.UserHash.ToArray();

	Transfer["IPv4"] = m_Mule.IPv4.ToQString();
	Transfer["IPv6"] = m_Mule.IPv6.ToQString();
	Transfer["TCPPort"] = m_Mule.TCPPort;
	Transfer["UDPPort"] = m_Mule.UDPPort;
	Transfer["KadPort"] = m_Mule.KadPort;
	Transfer["ConOpts"] = m_Mule.ConOpts.Bits;

	Transfer["ServerAddress"] = m_Mule.ServerAddress.ToQString();
	Transfer["ServerPort"] = m_Mule.ServerPort;

	Transfer["BuddyID"] = m_Mule.BuddyID;
	Transfer["BuddyAddress"] = m_Mule.BuddyAddress.ToQString();
	Transfer["BuddyPort"] = m_Mule.BuddyPort;

	if(m_Parts)
		Transfer["PartMap"] = m_Parts->Store();
	return Transfer;
}

bool CMuleSource::Load(const QVariantMap& Transfer)
{
	if(Transfer.isEmpty() || !GetFile()->IsEd2kShared())
		return false;
	
	m_Mule.ClientID = Transfer["ClientID"].toUInt();
	m_Mule.OpenIPv6 = Transfer["OpenIPv6"].toBool();
	m_Mule.UserHash = Transfer["UserHash"].toByteArray();

	m_Mule.AddIP(CAddress(Transfer["IPv4"].toString()));
	m_Mule.AddIP(CAddress(Transfer["IPv6"].toString()));
	m_Mule.TCPPort = Transfer["TCPPort"].toUInt();
	m_Mule.UDPPort = Transfer["UDPPort"].toUInt();
	m_Mule.KadPort = Transfer["KadPort"].toUInt();
	m_Mule.ConOpts.Bits = Transfer["ConOpts"].toUInt();
	m_Mule.SelectIP();

	m_Mule.ServerAddress = CAddress(Transfer["ServerAddress"].toString());
	m_Mule.ServerPort = Transfer["ServerPort"].toUInt();

	m_Mule.BuddyID = Transfer["BuddyID"].toByteArray();
	m_Mule.BuddyAddress = CAddress(Transfer["BuddyAddress"].toString());
	m_Mule.BuddyPort = Transfer["BuddyPort"].toUInt();

	if(Transfer.contains("PartMap"))
	{
		m_Parts = CShareMapPtr(new CShareMap(GetFile()->GetFileSize()));
		if(!m_Parts->Load(Transfer["PartMap"].toMap()))
			LogLine(LOG_WARNING, tr("MuleSource %1 contians an invalid part map").arg(GetDisplayUrl()));
		m_Parts->SetRange(0, -1, Part::Scheduled | Part::Requested, CShareMap::eClr); // Clear all ald stated
	}

	return true;
}

bool CMuleSource::ParseUrl(const QString& Url, CAddress& IP, uint16& Port, const QString& Type)
{
	if(Url.isEmpty())
		return false;

	StrPair ProtRest = Split2(Url.toLower(), "://");
	if(ProtRest.second.isEmpty())
	{
		ProtRest.second = ProtRest.first;
		ProtRest.first = "";
	}

	StrPair IPPort;
	QStringList Sections = ProtRest.second.split("|", QString::SkipEmptyParts);
	if(!Type.isEmpty())
	{
		foreach(const QString& Section, Sections)
		{
			StrPair NameValue = Split2(Section, ",");
			if(NameValue.first.compare(Type, Qt::CaseInsensitive) == 0)
				IPPort = Split2(NameValue.second, ":", true);
		}
		if(IPPort.second.isEmpty())
		{
			int Index = Sections.indexOf(Type);
			if(Index != -1 && Sections.count() - Index > 3)
			//if(Sections.count() >= 3 && Sections[0].compare(Type, Qt::CaseInsensitive) == 0)
			{
				IPPort.first = Sections[Index+1];
				IPPort.second = Sections[Index+2];
			}
		}
	}
	if(IPPort.second.isEmpty())
		IPPort = Split2(Split2(ProtRest.second, "/").first, ":", true);
	if(IPPort.second.isEmpty())
		return false;

	IP = IPPort.first;
	Port = IPPort.second.toUInt();
	return true;
}