#include "GlobalHeader.h"
#include "../../NeoCore.h"
#include "TorrentInfo.h"
#include "TorrentPeer.h"
#include "TorrentManager.h"
#include "../../FileList/File.h"
#include "../../FileList/FileStats.h"
#include "../FileGrabber.h"
#include "../../Networking/BandwidthControl/BandwidthLimit.h"
#include "../../Networking/BandwidthControl/BandwidthLimiter.h"
#include "../../FileList/Hashing/FileHash.h"
#include "TorrentClient.h"
#include "Torrent.h"
#include "TorrentServer.h"
#include "../../Networking/SocketThread.h"
#include "../../FileList/IOManager.h"
#include "../UploadManager.h"

bool STorrentPeer::SelectIP()
{
	int IPv6Support = theCore->m_TorrentManager->GetIPv6Support();
	if(HasV4() && HasV6()) // are booth addresses available
	{
		if(IPv6Support == 1)
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
	return true;
}

CTorrentPeer::CTorrentPeer(CTorrent* pTorrent)
{
	Init();
	m_pTorrent = pTorrent;
}

CTorrentPeer::CTorrentPeer(CTorrent* pTorrent, const STorrentPeer& Peer)
{
    m_Peer = Peer;
	m_pTorrent = pTorrent;
	Init();
}

void CTorrentPeer::Init()
{
	m_pClient = NULL;

	m_CachedMap = NULL;

	m_LastConnect = 0;
	//m_NextConnectRetry = 0;

	m_Status.Bits = 0;
	m_Interesting = false;
	m_RequestVolume = 8;
}

CTorrentPeer::~CTorrentPeer()
{
	ASSERT(m_pClient == NULL);

	ASSERT(!m_Parts || m_Parts->CountLength(Part::Scheduled) == 0);

	delete m_CachedMap;
}

bool CTorrentPeer::Process(UINT Tick)
{
	if(m_pClient && m_pClient->IsDisconnected())
	{
		Disconnect();

		if(HasError())
			return false;
	}

	if(m_pClient == NULL) // if we are not connected and dont have a port just drop it
		return m_Peer.Port == 0 ? false : true;

	CFile* pFile = GetFile();

	// upload stopper
	if(IsActiveUpload())
	{
		bool bKeepTraiding = false;
		if(IsActiveDownload())
		{
			//double ClientRatio = double(UploadedBytes())/double(DownloadedBytes());
			//double FileRatio = double(pFile->UploadedBytes())/double(pFile->DownloadedBytes());
			//if(ClientRatio > FileRatio)
			//	bKeepTraiding = true;
			bKeepTraiding = DownloadedBytes() > (UploadedBytes() * 7 / 10); // 30% tolerance
		}

		uint64 uPieceSize = m_pTorrent->GetInfo()->GetPieceLength();
		uint64 uPieceLimit = uPieceSize;
		while(uPieceSize && uPieceLimit < MB2B(5))
			uPieceLimit += uPieceSize;

		if(!bKeepTraiding && m_pClient->GetSentPieceSize() >= uPieceLimit && theCore->m_UploadManager->GetWaitingCount() > 10)
		{
			LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb upload finished")
				.arg(pFile->GetFileName()).arg(GetDisplayUrl()).arg((double)m_pClient->GetSentPieceSize()/1024.0));
			StopUpload();
		}
	}

	return CP2PTransfer::Process(Tick);
}

QString CTorrentPeer::GetUrl() const
{
	return QString("bt://%1:%2/%3").arg(m_Peer.GetIP().ToQString(true)).arg(m_Peer.Port).arg(QString(m_pTorrent->GetInfoHash().toHex()));
}

CFileHashPtr CTorrentPeer::GetHash()
{
	return m_pTorrent->GetHash();
}

void CTorrentPeer::Hold(bool bDelete)
{
	m_LastConnect = 0;
	//if(m_NextConnectRetry != 0)
	//	m_NextConnectRetry = GetCurTick();

	if(m_pClient) 
	{
		m_pClient->Disconnect(); 
		Disconnect();

		if(m_ReservedSize > 0)
			ReleaseRange();
	}
}

QString CTorrentPeer::GetConnectionStr() const
{
	if(m_pClient)
		return m_pClient->GetConnectionStr();
	return "";
}

bool CTorrentPeer::Connect()
{
	ASSERT(m_pClient == NULL);
	ASSERT(m_Peer.Port);

	CTorrentClient* pClient = new CTorrentClient(this, theCore->m_TorrentManager);
	theCore->m_TorrentManager->AddConnection(pClient);

	// Connect signals
    AttacheClient(pClient);

    return m_pClient->Connect();
}

void CTorrentPeer::AttacheClient(CTorrentClient* pClient)
{
	ASSERT(m_pClient == NULL);

	m_pClient = pClient;
	m_pClient->SetTorrentPeer(this);


	CFile* pFile = GetFile();
	m_pClient->GetSocket()->AddUpLimit(pFile->GetUpLimit());
	m_pClient->GetSocket()->AddDownLimit(pFile->GetDownLimit());
}

void CTorrentPeer::Disconnect()
{
	if(!m_pClient)
		return; // already disconnected

	// Remove the host from our list of known peers if the connection failed.
	if (m_pClient->HasError())
		SetError("ClientError: " + m_pClient->GetError());

	DetacheClient();
}

void CTorrentPeer::DetacheClient()
{
	ASSERT(m_pClient);

	// Reset all status flags, a disconnected torrent clietn does not wait for upload and is not interested.
	m_Status.Bits = 0;

	theCore->m_TorrentManager->RemoveConnection(m_pClient);

	// Delete the m_pClient later.
	m_pClient->SetTorrentPeer(NULL);
	delete m_pClient;
	m_pClient = NULL;

	ReleaseRange();
}

CBandwidthLimit* CTorrentPeer::GetUpLimit()
{
	return m_pClient ? m_pClient->GetUpLimit() : NULL;
}

CBandwidthLimit* CTorrentPeer::GetDownLimit()
{
	return m_pClient ? m_pClient->GetDownLimit() : NULL;
}

void CTorrentPeer::OnHandShakeRecived()
{
	if(m_Peer.ID.IsEmpty())
		m_Peer.ID = m_pClient->GetClientID();
	else if(m_Peer.ID != m_pClient->GetClientID())
	{
		//m_pClient->LogLine(LOG_WARNING, tr("Torrent client changed ist ID!"));
		m_Peer.ID = m_pClient->GetClientID();
	}

	m_Peer.SetIP(m_pClient->GetPeer().GetIP());
	m_Peer.ConOpts.Fields.SupportsEncryption = m_pClient->SupportsEncryption();
	m_Peer.ConOpts.Fields.SupportsUTP = (m_pClient->GetSocket()->GetSocketType() == SOCK_UTP);
	m_Peer.ConOpts.Fields.SupportsHolepunch = m_pClient->SupportsHolepunch();

	m_Software = m_pClient->GetSoftware();

	CFile* pFile = GetFile();
	if(m_pTorrent->GetInfo()->IsEmpty())
	{
		connect(m_pTorrent, SIGNAL(MetadataLoaded()), this, SLOT(OnMetadataLoaded()));
	}
	else
	{
		if(m_pClient->SupportsFAST() && pFile->IsComplete())
			m_pClient->SendHaveAll();
		else if(m_pClient->SupportsFAST() && (pFile->GetPartMap()->GetRange(0,-1, CPartMap::eUnion) & Part::Verified) == 0)
			m_pClient->SendHaveNone();
		else
		{
			CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(m_pTorrent->GetHash().data());
			ASSERT(pHashEx);
			m_pClient->SendPieceList(pHashEx->GetStatusMap());
		}
	}

	connect(GetFile(), SIGNAL(HavePiece(int)), m_pClient, SLOT(OnHavePiece(int)));

	m_LastConnect = GetCurTick();
}

void CTorrentPeer::OnExtensionsRecived()
{
	const STorrentPeer& Peer = m_pClient->GetPeer();
	if(Peer.Port)
		m_Peer.Port = Peer.Port;
	m_Peer.SetIP(Peer);

	m_Software = m_pClient->GetSoftware();
}

void CTorrentPeer::OnMetadataLoaded()
{
	disconnect(this, SLOT(OnMetadataLoaded()));

	CFile* pFile = GetFile();
	if(m_pClient && m_pClient->HasReceivedHandShake())
	{
		if(m_pClient->SupportsFAST() && pFile->IsComplete())
			m_pClient->SendHaveAll();
		else if(m_pClient->SupportsFAST() && (pFile->GetPartMap()->GetRange(0,-1, CPartMap::eUnion) & Part::Verified) == 0)
			m_pClient->SendHaveNone();
		else
		{
			CFileHashEx* pHashEx = qobject_cast<CFileHashEx*>(m_pTorrent->GetHash().data());
			ASSERT(pHashEx);
			m_pClient->SendPieceList(pHashEx->GetStatusMap());
		}
	}

	if(m_CachedMap)
	{
		switch(m_CachedMap->Type)
		{
			case SCachedMap::eBitField:	m_CachedMap->BitField.resize(m_pTorrent->GetInfo()->GetPieceHashes().size());
										OnPiecesAvailable(m_CachedMap->BitField);	break;
			case SCachedMap::eAll:		OnHaveAll();								break;
			case SCachedMap::eNone:		OnHaveNone();								break;
		}
		delete m_CachedMap;
		m_CachedMap = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Download

void CTorrentPeer::OnPiecesAvailable(const QBitArray &FullPieces)
{
	CTorrentInfo* pTorrentInfo = m_pTorrent->GetInfo();

	if(pTorrentInfo->IsEmpty())
	{
		if(!m_CachedMap)
			m_CachedMap = new SCachedMap;
		m_CachedMap->Type = SCachedMap::eBitField;
		m_CachedMap->BitField = FullPieces;
		return;
	}
	
	PartsAvailable(FullPieces, pTorrentInfo->GetPieceLength());

	if(GetFile()->IsComplete(true) && IsSeed()) // we are seed and he is seed than disconnect
		m_pClient->Disconnect();
}

void CTorrentPeer::OnPieceAvailable(uint32 Index)
{
	CTorrentInfo* pTorrentInfo = m_pTorrent->GetInfo();

	if(pTorrentInfo->IsEmpty())
	{
		if(!m_CachedMap)
			m_CachedMap = new SCachedMap;
		m_CachedMap->Type = SCachedMap::eBitField;
		if(Index + 1 > (uint32)m_CachedMap->BitField.size())
			m_CachedMap->BitField.resize(Index + 1);
		m_CachedMap->BitField.setBit(Index, 1);
		return;
	}

	bool b1st;
	if(b1st = m_Parts.isNull()) // thats the first request
		m_Parts = CShareMapPtr(new CShareMap(GetFile()->GetFileSize()));

	uint64 uBegin = Index * pTorrentInfo->GetPieceLength();
	if(uBegin >= GetFile()->GetFileSize())
		return; // Error
	uint64 uEnd = uBegin + pTorrentInfo->GetPieceLength();
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

void CTorrentPeer::OnHaveAll()
{
	CTorrentInfo* pTorrentInfo = m_pTorrent->GetInfo();

	if(pTorrentInfo->IsEmpty())
	{
		if(!m_CachedMap)
			m_CachedMap = new SCachedMap;
		m_CachedMap->Type = SCachedMap::eAll;
		return;
	}

	bool b1st;
	if(b1st = m_Parts.isNull()) // thats the first request
		m_Parts = CShareMapPtr(new CShareMap(GetFile()->GetFileSize()));

	m_AvailableBytes = pTorrentInfo->GetTotalLength();

	CAvailDiff AvailDiff;
	AvailDiff.Update(0, -1, Part::Available, m_Parts.data(), Part::Available);
	m_Parts->SetRange(0, -1, Part::Available | Part::Verified, CShareMap::eAdd);
	GetFile()->GetStats()->UpdateAvail(this, AvailDiff, b1st);

	CheckInterest();
}

void CTorrentPeer::OnHaveNone()
{
	CTorrentInfo* pTorrentInfo = m_pTorrent->GetInfo();

	if(pTorrentInfo->IsEmpty())
	{
		if(!m_CachedMap)
			m_CachedMap = new SCachedMap;
		m_CachedMap->Type = SCachedMap::eNone;
		return;
	}

	bool b1st;
	CAvailDiff AvailDiff;
	if(b1st = m_Parts.isNull()) // thats the first request
	{
		m_Parts = CShareMapPtr(new CShareMap(GetFile()->GetFileSize()));
		AvailDiff.Add(0, GetFile()->GetFileSize(), 0, 0);
	}
	else
		AvailDiff.Update(0, -1, 0, m_Parts.data(), Part::Available);
	
	m_AvailableBytes = 0;

	m_Parts->SetRange(0, -1, Part::Available | Part::Verified, CShareMap::eClr);
	GetFile()->GetStats()->UpdateAvail(this, AvailDiff, b1st);

	m_Peer.ConOpts.Fields.IsSeed = false;
}

bool CTorrentPeer::CheckInterest()
{
	m_Peer.ConOpts.Fields.IsSeed = (m_Parts->GetRange(0, -1) & Part::Available) != 0;

	m_Interesting = CTransfer::CheckInterest();
	if(!m_pClient)
		return m_Interesting;

	if(m_Interesting) // we are interested
	{
		if (!m_Status.Flags.InterestedInPeer)
		{
			m_Status.Flags.InterestedInPeer = true;
			m_pClient->SendInterested();
		}
	}
	else
	{
		if(m_Status.Flags.InterestedInPeer)
		{
			m_Status.Flags.InterestedInPeer = false;
			m_pClient->SendNotInterested();
		}
	}

	return m_Interesting;
}

void CTorrentPeer::OnPieceSuggested(uint32 Index)
{
	if(!m_Interesting)
		return;

	CPartMap* pFileParts = GetFile()->GetPartMap();
	if(!m_Parts || !pFileParts)
		return;

	CTorrentInfo* pTorrentInfo = m_pTorrent->GetInfo();

	uint64 uBegin = Index * pTorrentInfo->GetPieceLength();
	uint64 uEnd = uBegin + pTorrentInfo->GetPieceLength();
	if(uEnd > GetFile()->GetFileSize())
		uEnd = GetFile()->GetFileSize();

	SheduleRange(uBegin, uEnd);
}

void CTorrentPeer::OnUnchoked()
{
	if(m_Status.Flags.UnchokedByPeer == false)
	{
		m_Status.Flags.UnchokedByPeer = true;
		// We got unchoked, which means we can request more blocks.

		m_DownloadStartTime = GetCurTick();
	}

	if(!GetFile()->IsIncomplete())
	{
		SheduleDownload();
		RequestBlocks();
	}
}

void CTorrentPeer::OnChoked()
{
	if(m_Status.Flags.UnchokedByPeer == true)
	{
		m_Status.Flags.UnchokedByPeer = false;
		// When the peer chokes us, we immediately forget about all blocks
		// we've requested from it. We also remove the piece from out
		// payload, making it available to other clients.

		m_DownloadStartTime = 0;
	}

	ReleaseRange();
}

//void CTorrentPeer::ResetNextConnect()
//{
//	if(m_NextConnectRetry != 0)
//		m_NextConnectRetry = GetCurTick();
//}

void CTorrentPeer::RequestBlocks(bool bRetry)
{
	ASSERT(m_pClient);

	CFile* pFile = GetFile();
	if(pFile->IsComplete() || pFile->IsPaused() || theCore->m_IOManager->IsWriteBufferFull())
		return;

	CPartMap* pFileParts = pFile->GetPartMap();
	ASSERT(pFileParts);

	const uint64 uBlockSize = KB2B(16); // Note: if that is bigger other clients wil drop us
	uint64 uPieceSize = m_pTorrent->GetInfo()->GetPieceLength();
	uint64 uRequestVolume = Min(m_pClient->GetRequestLimit(), m_RequestVolume);
	int iOldCount = m_pClient->GetRequestCount();
	int iReservedCount = 0;
	int EndGameVolume = theCore->Cfg()->GetInt("Content/EndGameVolume");

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

			// Note: BT uses an agressive endgame mode, meaning if the source is scheduled to obtain this part, it will do it no mather if it was already requested from an other client
			if((FileIter.uState & Part::Requested) != 0 
			 && m_RequestVolume != 1 // or requested, in endgame only available
			 && ((FileIter.uState & Part::Stream) == 0 || FileIter.uState.uRequested >= EndGameVolume) // lets be agressive when streaming but lets not go nuts
			) 
			{
				iReservedCount++;
				continue; // this block has already been requested
			}

			uint32 Index = FileIter.uBegin / uPieceSize;
			uint64 uOffset = FileIter.uBegin % uPieceSize;
			uint64 uSize = FileIter.uEnd - FileIter.uBegin;
			if(uSize > uBlockSize)
				uSize = uBlockSize;
			// Note: we are not allowed to send requests past the piece border
			if(uOffset + uSize > uPieceSize)
				uSize = uPieceSize - uOffset;
			if(FileIter.uEnd - FileIter.uBegin > uSize)
				FileIter.uEnd = FileIter.uBegin + uSize;

			m_Parts->SetRange(FileIter.uBegin, FileIter.uEnd, Part::Requested, CShareMap::eAdd);	
			pFileParts->SetRange(FileIter.uBegin, FileIter.uEnd, Part::Requested, CPartMap::eAdd);

			m_pClient->RequestBlock(Index, uOffset, uSize);
		}
	}
	
	// enable endgame mode if needed
	if(iReservedCount > 0 && m_pClient->GetRequestCount() == 0)
		m_RequestVolume = 1; // Volume 1 marks endgame as its more agressive we request only one blockt at a time
	// Dynamically addapt request volume
	else if(iOldCount == 0 && m_pClient->GetRequestCount() > 0 && m_RequestVolume < theCore->Cfg()->GetInt("BitTorrent/RequestLimit")) // if all was served and something requested allow requesting more on the next tick
		m_RequestVolume *= 2;
	else if(m_pClient->GetRequestCount() > m_RequestVolume / 2 && m_RequestVolume > 4) // do not go down to 1!
		m_RequestVolume /= 2;

	if(!bRetry && m_pClient->GetRequestCount() == 0 && SheduleDownload())
		RequestBlocks(true);
}

void CTorrentPeer::CancelRequest(uint64 uBegin, uint64 uEnd)
{
	CP2PTransfer::CancelRequest(uBegin, uEnd);

	if(!m_pClient)
		return;

	uint64 uPieceSize = m_pTorrent->GetInfo()->GetPieceLength();
	foreach(const CTorrentClient::SRequestedBlock& Block, m_pClient->GetIncomingBlocks())
	{
		uint64 uCurBegin = Block.Index * uPieceSize + Block.Offset;
		uint64 uCurEnd = uCurBegin + Block.Length;
		if(uCurBegin >= uBegin && uCurEnd <= uEnd)
			m_pClient->CancelRequest(Block.Index, Block.Offset, Block.Length);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Upload

bool CTorrentPeer::StartUpload()
{
	ASSERT(m_pClient);
	ASSERT(m_Status.Flags.PeerIsInterested);

	if (!m_Status.Flags.PeerIsUnchoked)
	{
		m_UploadStartTime = GetCurTick();

		m_Status.Flags.PeerIsUnchoked = true;
		m_pClient->UnchokePeer();
	}
	return true;
}

void CTorrentPeer::StopUpload(bool bStalled)
{
	ASSERT(m_pClient);

	if (m_Status.Flags.PeerIsUnchoked)
	{
		m_UploadStartTime = 0;

		m_Status.Flags.PeerIsUnchoked = false;
		if(m_pClient->IsConnected())
			m_pClient->ChokePeer();
		if(bStalled)
			m_Status.Flags.PeerIsInterested = false;
	}
}

qint64 CTorrentPeer::PendingBytes() const
{
	if(!m_pClient || !m_pClient->GetSocket())
		return 0;
	return m_pClient->GetSocket()->QueueSize();
}

qint64 CTorrentPeer::PendingBlocks() const
{
	if(!m_pClient || !m_pClient->GetSocket())
		return 0;
	return m_pClient->GetSocket()->QueueCount();
}

qint64 CTorrentPeer::LastUploadedBytes() const
{
	if(!m_pClient || !m_pClient->GetSocket())
		return 0;
	return m_pClient->GetSentPieceSize();
}

bool CTorrentPeer::IsBlocking() const
{
	if(!m_pClient || !m_pClient->GetSocket())
		return 0;
	return m_pClient->GetSocket()->IsBlocking();
}

qint64 CTorrentPeer::RequestedBlocks() const
{
	if(!m_pClient)
		return 0;
	return m_pClient->GetRequestCount();
}

qint64 CTorrentPeer::LastDownloadedBytes() const
{
	if(!m_pClient)
		return 0;
	return m_pClient->GetRecivedPieceSize();
}

bool CTorrentPeer::SupportsHostCache() const
{
	return m_pClient && m_pClient->SupportsHostCache();
}

QPair<const byte*, size_t> CTorrentPeer::GetID() const
{
	if(m_pClient)
	{
		if(CTorrentSocket* pSocket = m_pClient->GetSocket())
			return QPair<const byte*, size_t>(pSocket->GetAddress().Data(), pSocket->GetAddress().Size());
	}
	ASSERT(0);
	return QPair<const byte*, size_t>(NULL, 0);
}

QString CTorrentPeer::GetSoftware() const
{
	QString Software = m_Software;
	if(Software.isEmpty())
		return "Unknown";
#ifdef _DEBUG
	if(!Software.isEmpty())
	{
		if(m_Peer.ConOpts.Fields.SupportsUTP)
			Software += " +uTP";
		if(m_Peer.ConOpts.Fields.SupportsEncryption)
			Software += " +Crypto";
	}
#endif
	return Software;
}

/////////////////////////////////////////////////////////////////////////////////////
// Load/Store

QVariantMap CTorrentPeer::Store()
{
	if(!theCore->Cfg()->GetBool("BitTorrent/SavePeers") || !IsChecked())
		return QVariantMap();

	QVariantMap	Transfer;

	Transfer["InfoHash"] = m_pTorrent->GetInfoHash().toHex();
	Transfer["PeerID"] = m_Peer.ID.ToArray();

	Transfer["IPv4"] = m_Peer.IPv4.ToQString();
	Transfer["IPv6"] = m_Peer.IPv6.ToQString();
	Transfer["TCPPort"] = m_Peer.Port;
	Transfer["ConOpts"] = m_Peer.ConOpts.Bits;

	if(m_Parts)
		Transfer["PartMap"] = m_Parts->Store();
	return Transfer;
}

bool CTorrentPeer::Load(const QVariantMap& Transfer)
{
	if(Transfer.isEmpty())
		return false;

	QByteArray InfoHash = QByteArray::fromHex(Transfer["InfoHash"].toByteArray());
	m_pTorrent = GetFile()->GetTorrent(InfoHash);
	if(!m_pTorrent)
		return false;

	m_Peer.ID = Transfer["PeerID"].toByteArray();

	m_Peer.AddIP(CAddress(Transfer["IPv4"].toString()));
	m_Peer.AddIP(CAddress(Transfer["IPv6"].toString()));
	m_Peer.Port = Transfer["TCPPort"].toUInt();
	if(m_Peer.Port == 0)
		return false;
	m_Peer.ConOpts.Bits = Transfer["ConOpts"].toUInt();
	m_Peer.SelectIP();

	if(Transfer.contains("PartMap"))
	{
		m_Parts = CShareMapPtr(new CShareMap(GetFile()->GetFileSize()));
		if(!m_Parts->Load(Transfer["PartMap"].toMap()))
			LogLine(LOG_WARNING, tr("TorrentPeer %1 contians an invalid part map").arg(GetDisplayUrl()));
		m_Parts->SetRange(0, -1, Part::Scheduled | Part::Requested, CShareMap::eClr); // Clear all old stated
	}
	return true;
}