#include "GlobalHeader.h"
#include "Transfer.h"
#include "P2PClient.h"
#include "../FileList/File.h"
#include "../FileList/FileStats.h"
#ifndef NO_HOSTERS
#include "./HosterTransfer/WebManager.h"
#include "./HosterTransfer/HosterLink.h"
#endif
#include "./BitTorrent/TorrentPeer.h"
#include "./ed2kMule/MuleSource.h"
#include "./NeoShare/NeoEntity.h"
#include "../Networking/BandwidthControl/BandwidthLimit.h"
#include "../NeoCore.h"
#include "../FileList/IOManager.h"
#include "PartDownloader.h"
#include <math.h>
#include "../Networking/ListenSocket.h"
#include "HashInspector.h"
#include "CorruptionLogger.h"

void SAddressCombo::SetIP(const SAddressCombo& From){
	IPv4 = From.IPv4;
	IPv6 = From.IPv6;
	Prot = From.Prot;
}

void SAddressCombo::SetIP(const CAddress& IP){
	Prot = IP.Type();
	AddIP(IP);
}

void SAddressCombo::AddIP(const CAddress& IP)
{
	if(IP.Type() == CAddress::IPv4)
		IPv4 = IP;
	else if(IP.Type() == CAddress::IPv6)
		IPv6 = IP;
}

CAddress SAddressCombo::GetIP() const
{
	if(Prot == CAddress::IPv6)
		return IPv6;
	if(Prot == CAddress::IPv4)
		return IPv4;
	return CAddress();
}

bool SAddressCombo::CompareTo(const CAddress& Address) const
{
	if(Address.Type() == CAddress::IPv4)
	{
		if(IPv4 == Address)
			return true;
	}
	else if(Address.Type() == CAddress::IPv6)
	{
		if(IPv6 == Address)
			return true;
	}
	return false;
}

bool SAddressCombo::CompareTo(const SAddressCombo &Other) const	
{
	if(HasV4() && Other.HasV4())
	{
		if(IPv4 == Other.IPv4)
			return true;
	}
	if(HasV6() && Other.HasV6())
	{
		if(IPv6 == Other.IPv6)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////
//

QString CTransfer::GetTypeStr(const CTransfer* pTransfer)
{
#ifndef NO_HOSTERS
	if(pTransfer->inherits("CHosterLink"))
		return "HosterLink";
#endif
	if(pTransfer->inherits("CTorrentPeer"))
		return "TorrentPeer";
	if(pTransfer->inherits("CMuleSource"))
		return "MuleSource";
	if(pTransfer->inherits("CNeoEntity"))
		return "NeoEntity";
	return "";
}

QString CTransfer::GetTypeStr(const CP2PClient* pClient)
{
	if(pClient->inherits("CTorrentClient"))
		return "Torrent Peer";
	if(pClient->inherits("CMuleClient"))
		return "Mule Source";
	if(pClient->inherits("CNeoClient"))
		return "Neo Entity";
	return "";
}

CTransfer* CTransfer::New(const QString& Type)
{
#ifndef NO_HOSTERS
	if(Type.compare("HosterLink") == 0)
		return new CHosterLink("");
#endif
	if(Type.compare("TorrentPeer") == 0)
		return new CTorrentPeer(NULL);
	if(Type.compare("MuleSource") == 0)
		return new CMuleSource();
	if(Type.compare("NeoEntity") == 0)
    {
        CFileHashPtr pHash(NULL);
        return new CNeoEntity(pHash);
    }
	return NULL;
}

CTransfer* CTransfer::FromUrl(const QString& Url, CFile* pFile)
{
	StrPair ProtRest = Split2(Url, "://");
	if(ProtRest.first.compare("ed2k", Qt::CaseInsensitive) == 0) // return QString("ed2k://|source,%1:%2|/").arg(m_Mule.GetIP().ToQString()).arg(m_Mule.TCPPort);
	{
		CAddress Address;
		uint16 Port = 0;
		if(CMuleSource::ParseUrl(Url, Address, Port, "source"))
		{
			SMuleSource Mule;
			Mule.SetIP(Address);
			if(Mule.Prot == CAddress::IPv6)
				Mule.OpenIPv6 = true;
			Mule.TCPPort = Port;
			return new CMuleSource(Mule);
		}
	}
	else if(ProtRest.first.compare("bt", Qt::CaseInsensitive) == 0) // return QString("bt://%1:%2/%3").arg(m_Peer.GetIP().ToQString(true)).arg(m_Peer.Port).arg(QString(m_pTorrent->GetInfoHash().toHex()));
	{
		StrPair RestHash = Split2(ProtRest.second, "/", true);
		if(CTorrent* pTorrent = RestHash.second.isEmpty() ? pFile->GetTopTorrent() : pFile->GetTorrent(QByteArray::fromHex(RestHash.second.toLatin1())))
		{
			StrPair IPPort = Split2(Split2(RestHash.first, "/").first, ":", true);
			if(IPPort.second.isEmpty())
				return NULL;
			STorrentPeer Peer;
			Peer.SetIP(IPPort.first);
			Peer.Port = IPPort.second.toUInt();
			return new CTorrentPeer(pTorrent, Peer);
		}
	}
	else if(ProtRest.first.compare("neo", Qt::CaseInsensitive) == 0) // return QString("neo://%1@%2/%3").arg(QString::fromLatin1(m_Neo.EntityID.toHex())).arg(QString::fromLatin1(TargetID.toHex()).toUpper()).arg(CFileHash::HashType2Str(m_pHash->GetType()) + ":" + m_pHash->ToString());
	{
		StrPair EntityHash = Split2(ProtRest.second, "/");
		StrPair EntityTarget = Split2(EntityHash.first, "@");
		SNeoEntity Neo;
		Neo.EntityID = QByteArray::fromHex(EntityTarget.first.toLatin1());
		Neo.TargetID = QByteArray::fromHex(EntityTarget.second.toLatin1());
		std::reverse(Neo.TargetID.begin(), Neo.TargetID.end());
		CFileHashPtr pHash;
		if(EntityHash.second.isEmpty())
			pHash = pFile->GetHashPtr(pFile->IsMultiFile() ? HashXNeo : HashNeo);
		else
		{
			StrPair TypeHash = Split2(EntityHash.second, ":");
			EFileHashType Type = CFileHash::Str2HashType(TypeHash.first);
			if(Type != HashNone)
				pHash = pFile->GetHashPtrEx(Type, CFileHash::DecodeHash(Type, TypeHash.second.toLatin1()));
		}
		if(pHash)
			return new CNeoEntity(pHash, Neo);
	}
#ifndef NO_HOSTERS
	else if(ProtRest.first.compare("http", Qt::CaseInsensitive) == 0 || ProtRest.first.compare("https", Qt::CaseInsensitive) == 0 || ProtRest.first.compare("ftp", Qt::CaseInsensitive) == 0)
		return new CHosterLink(Url);
#endif
	return NULL;
}

CTransfer::CTransfer(CFile* pFile)
: QObjectEx(pFile)
{
	m_FoundBy = eOther;
}

CTransfer::~CTransfer()
{
}

QString CTransfer::GetTypeStr()
{
	switch(GetType())
	{
		case eNeoShare:		return "Neo Entity";
		case eHosterLink:	return "Hoster Link";
		case eBitTorrent:	return "Torrent Peer";
		case eEd2kMule:		return "Mule Source";
		default:			return "Unknown";
	}
}

void CTransfer::SetFile(CFile* pFile)
{
	setParent(pFile);
}

bool CTransfer::SheduleRange(uint64 uBegin, uint64 uEnd, bool bStream)
{
	CFile* pFile = GetFile();
	ASSERT(pFile);
	CPartMap* pFileParts = pFile->GetPartMap();
	if(!m_Parts || !pFileParts)
		return false;

	// Note: we enter this function for each part, we ignore only parts that are entierly set dissbaled
	if((pFileParts->GetRange(uBegin, uEnd) & Part::Disabled) != 0)
		return false;

	int ReservedCount = 0;
	int EndGameVolume = theCore->Cfg()->GetInt("Content/EndGameVolume");
	//UINT eCorrupt = CPartMap::Corrupt(pFile->GetMasterHash());

	//uint64 uRequested = 0;
	//uint64 uOmitted = 0;

	CShareMap::SIterator SrcIter(uBegin, uEnd);
	while(m_Parts->IterateRanges(SrcIter))
	{
		if((SrcIter.uState & Part::Available) == 0 || (SrcIter.uState & Part::Verified) == 0) // if the reange is not available its irrelevant
			continue;

		if((SrcIter.uState & Part::Scheduled) != 0) // this range is already reserved for this source
			continue;

		CPartMap::SIterator FileIter(SrcIter.uBegin, SrcIter.uEnd);
		while(pFileParts->IterateRanges(FileIter))
		{
			// check if this part has already been downlaoded, or is blocked, the download plan is not always up to date
			if((FileIter.uState & (Part::Available | Part::Cached)) != 0)
				continue; 

			// check if this part is reserved, in end game mode or when streaming ignore reservation
			if((FileIter.uState & Part::Scheduled) != 0)
			{
				//if(!bForce || FileIter.uState.uScheduled >= EndGameVolume || (FileIter.uState & eCorrupt) != 0)
				if(!bStream && FileIter.uState.uScheduled >= EndGameVolume)
				{
					//uOmitted += FileIter.uEnd - FileIter.uBegin;
					continue; 
				}
			}

			//uRequested += 
			ReserveRange(FileIter.uBegin, FileIter.uEnd, Part::Scheduled);
			ReservedCount++;
		}
	}

	return ReservedCount > 0;
}

void CTransfer::ReleaseRange(uint64 uBegin, uint64 uEnd, bool bAll)
{
	if(!m_Parts)
		return;

	CShareMap::SIterator SrcIter(uBegin, uEnd);
	while(m_Parts->IterateRanges(SrcIter))
	{
		if((SrcIter.uState & (Part::Scheduled | Part::Marked)) != 0 && (bAll || (SrcIter.uState & Part::Requested) == 0))
			UnReserveRange(SrcIter.uBegin, SrcIter.uEnd, (SrcIter.uState & (Part::Scheduled | Part::Marked | Part::Requested))); // unreserver only whats really eserved for that state
	}
}

void CTransfer::ReserveRange(uint64 uBegin, uint64 uEnd, uint32 uState)
{
	CFile* pFile = GetFile();
	CPartMap* pFileParts = pFile->GetPartMap();

	ASSERT(pFile->IsIncomplete());

	ASSERT(m_Parts);
	ASSERT((m_Parts->GetRange(uBegin, uEnd, CShareMap::eUnion) & uState) == 0);
	m_Parts->SetRange(uBegin, uEnd, uState, CShareMap::eAdd);

	ASSERT(pFileParts);
	pFileParts->SetRange(uBegin, uEnd, uState, CPartMap::eAdd);
}

void CTransfer::UnReserveRange(uint64 uBegin, uint64 uEnd, uint32 uState)
{
	m_Parts->SetRange(uBegin, uEnd, uState, CShareMap::eClr);
	if(CPartMap* pFileParts = GetFile()->GetPartMap())
		pFileParts->SetRange(uBegin, uEnd, uState, CPartMap::eClr);
}

void CTransfer::RangeReceived(uint64 uBegin, uint64 uEnd)
{
	ReleaseRange(uBegin, uEnd);

	CFile* pFile = GetFile();
	CPartMap* pFileParts = pFile->GetPartMap();

#ifdef REQ_DEBUG
	if(!pFileParts || (pFileParts->GetRange(uBegin, uEnd, CPartMap::eUnion) & (Part::Available | Part::Cached)) != 0)
		TRACE(tr("Redownlaoded range %1 - %2 of file %3").arg(uBegin).arg(uEnd).arg(GetFile()->GetFileName()));
#endif

	if(!pFileParts)
		return;

	// Check if we requested this block from an other source as well, and if so cancel all pending requests
	if((pFileParts->GetRange(uBegin, uEnd, CPartMap::eUnion) & Part::Requested) != 0)
		pFile->CancelRequest(uBegin, uEnd);

#ifdef REQ_DEBUG
	// Note: this wil fail if the file has sub files and thay have own requests, as the clearing above can only handle the parent file
	ASSERT((pFileParts->GetRange(uBegin, uEnd, CPartMap::eUnion) & (Part::Scheduled | Part::Requested)) == 0);
#endif
}

bool CTransfer::CheckInterest()
{
	if(!GetFile()->IsIncomplete())
		return false;

	CPartMap* pFileParts = GetFile()->GetPartMap();
	ASSERT(pFileParts);

	CShareMap::SIterator SrcIter;
	while(m_Parts->IterateRanges(SrcIter))
	{
		if((SrcIter.uState & Part::Available) != 0 && (pFileParts->GetRange(SrcIter.uBegin, SrcIter.uEnd) & Part::Available) == 0)
			return true;
	}
	return false;
}

QString CTransfer::GetFoundByStr() const
{
	switch(m_FoundBy)
	{
	case eSelf:		return "Self";
	case eStored:	return "Stored";
	case eNeo:		return "Neo";
	case eKad:		return "Kad";
	case eDHT:		return "DHT";
	case eXS:		return "XS";
	case ePEX:		return "PEX";
	case eEd2kSvr:	return "Ed2kSvr";
	case eTracker:	return "Tracker";
	case eGrabber:	return "Grabber";
	case eOther:
	default:		return "Other";
	}
}

///////////////////////////////////////////////////////////////////////////////////
// CP2PTransfer

CP2PTransfer::CP2PTransfer()
{
	m_FirstSeen = GetCurTick();

	m_ReservedSize = 0;
	//m_NextReserve = 0;
	//m_EndGameLevel = 0;

	m_AvailableBytes = 0;

	m_UploadedBytes = 0;
	m_UploadStartTime = 0;
	m_DownloadedBytes = 0;
	m_DownloadStartTime = 0;
}

CP2PTransfer::~CP2PTransfer()
{
	ASSERT(m_ReservedSize == 0);
}

bool CP2PTransfer::Process(UINT Tick)
{
	if(IsActiveDownload() && RequestedBlocks() == 0)
		RequestBlocks();

	// if we return false the transfer will be removed
	return !HasError();
}

void CP2PTransfer::ReserveRange(uint64 uBegin, uint64 uEnd, uint32 uState)
{
	ASSERT(IsConnected());

	uint64 uLength = uEnd - uBegin;
	m_ReservedSize += uLength;
#ifdef REQ_DEBUG
	ASSERT(m_ReservedSize == m_Parts->CountLength(Part::Scheduled));
#endif

	CTransfer::ReserveRange(uBegin, uEnd, uState);
}

void CP2PTransfer::UnReserveRange(uint64 uBegin, uint64 uEnd, uint32 uState)
{
	uint64 uLength = uEnd - uBegin;
	ASSERT(m_ReservedSize >= uLength);
	m_ReservedSize -= uLength;

	CTransfer::UnReserveRange(uBegin, uEnd, uState);
}

bool CP2PTransfer::SheduleDownload()
{
	CFile* pFile = GetFile();
	if(pFile->IsComplete(true))
		return false;

	CFileHashPtr pHash = GetHash();
	CFileHashEx* pFileHash = qobject_cast<CFileHashEx*>(pHash.data());
	if(!pFileHash)
	{
		ASSERT(0);
		return false;
	}

	CHashInspector* pInspector = pFile->GetInspector();
	ASSERT(pInspector);

	CCorruptionLogger* pLogger = pInspector->GetLogger(pHash);
	if(pLogger && pLogger->IsDropped(GetID()))	
		return false;

	QVector<CPartDownloader::SPart> Ranges = pFile->GetDownloader()->GetDownloadPlan(pFileHash);

	for(QVector<CPartDownloader::SPart>::const_iterator I = Ranges.constBegin(); I != Ranges.constEnd(); I++) // Note: each range is exactly one entire part/piece/block
	{
		const CPartDownloader::SPart& Range = *I;

		if(SheduleRange(Range.uBegin, Range.uEnd, Range.bStream))
			return true; // if we added somethign we are satisfyed
	}
	return false;
}

bool CP2PTransfer::SheduleRange(uint64 uBegin, uint64 uEnd, bool bStream)
{
#ifdef REQ_DEBUG
	if(m_Parts){
		ASSERT(m_ReservedSize == m_Parts->CountLength(Part::Scheduled));
	}
#endif

	// Note: we sometimes wast a lot of CPU traversing the range maps, so we add here a shortcut 
	//			We check the bitmap for hashed and verifyed status and if its verifid we just bypass the whole map thing
	CFileHashPtr pHash = GetHash();
	CFileHashEx* pFileHash = qobject_cast<CFileHashEx*>(pHash.data());
	if(pFileHash && pFileHash->GetResult(uBegin, uEnd))
		return false;

	return CTransfer::SheduleRange(uBegin, uEnd, bStream);
}

void CP2PTransfer::ReleaseRange(uint64 uBegin, uint64 uEnd, bool bAll)
{
	CTransfer::ReleaseRange(uBegin, uEnd, bAll);

#ifdef REQ_DEBUG
	if(m_Parts){
		ASSERT(m_ReservedSize == m_Parts->CountLength(Part::Scheduled));
	}
#endif
}

void CP2PTransfer::RangeReceived(uint64 uBegin, uint64 uEnd, QByteArray Data)
{
	CFile* pFile = GetFile();
	ASSERT(pFile);
	if(pFile->IsComplete())
		return;
	
	CPartMap* pFileParts = pFile->GetPartMap();
	ASSERT(pFileParts);
	CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(pFileParts);
	CHashInspector* pInspector = pFile->GetInspector();
	ASSERT(pInspector);
	CCorruptionLogger* pLogger = pInspector->GetLogger(GetHash());

	CPartMap::SIterator FileIter(uBegin, uEnd);
	while(pFileParts->IterateRanges(FileIter, Part::Available | Part::Cached))
	{
		// check if this part has already been downlaoded or cached
		if((FileIter.uState & (Part::Available | Part::Cached)) != 0)
			continue; // do not overwrite already downlaoded data.

		ASSERT(FileIter.uBegin >= uBegin);
		uint64 uOffset = FileIter.uBegin - uBegin;
		ASSERT(FileIter.uEnd <= uEnd);
		uint64 uLength = FileIter.uEnd - FileIter.uBegin;
		if(uOffset == 0 && uLength == Data.length()) // default happy case
			theCore->m_IOManager->WriteData(pFile, pFile->GetFileID(), uBegin, Data, NULL);
		else // fragmented case, endgame hit!
		{
			QByteArray Fragment = Data.mid(uOffset, uLength);
			theCore->m_IOManager->WriteData(pFile, pFile->GetFileID(), FileIter.uBegin, Fragment, NULL);
		}

		// Log transfer in case of corruption
		if(pLogger)
			pLogger->Record(FileIter.uBegin, FileIter.uEnd, GetID());

		// Set Cached
		if(pJoinedParts)
			pJoinedParts->SetSharedRange(FileIter.uBegin, FileIter.uEnd, Part::Cached, CPartMap::eAdd);
		else
			pFileParts->SetRange(FileIter.uBegin, FileIter.uEnd, Part::Cached, CPartMap::eAdd);
	}

	CTransfer::RangeReceived(uBegin, uEnd);
}

void CP2PTransfer::CancelRequest(uint64 uBegin, uint64 uEnd)
{
	if(m_ReservedSize == 0)
		return;

	ReleaseRange(uBegin, uEnd);
}

void CP2PTransfer::PartsAvailable(const QBitArray &Parts, uint64 uPartSize, uint64 uBlockSize)
{
	bool b1st;
	if(b1st = m_Parts.isNull()) // thats the first request
		m_Parts = CShareMapPtr(new CShareMap(GetFile()->GetFileSize()));

	CAvailDiff AvailDiff;
	m_AvailableBytes = m_Parts->FromBitArray(Parts, uPartSize, uBlockSize, Part::Available | Part::Verified, AvailDiff);
	GetFile()->GetStats()->UpdateAvail(this, AvailDiff, b1st);

	CheckInterest();
}

/*void CP2PTransfer::RangesAvailable(const CShareMap* Ranges)
{
	bool b1st;
	if(b1st = m_Parts.isNull()) // thats the first request
		m_Parts = CPartMapPtr(new CPartMap(GetFile()->GetFileSize()));
	
	CAvailDiff AvailDiff;
	Ranges->To(m_Parts.data(), AvailDiff);
	GetFile()->GetStats()->UpdateAvail(this, AvailDiff, b1st);

	CheckInterest();
}*/

double CP2PTransfer::GetProbabilityRange()
{
	CFile* pFile = GetFile();
	if(!pFile)
	{
		ASSERT(0);
		return 1.0;
	}

	double Range;
	if(UploadedBytes() <= DownloadedBytes()) // he recived less than he send
	{
		Range = 1 + sqrt((double)DownloadedBytes() - (double)UploadedBytes());

		if(UploadedBytes() > 0)
		{
			double Ratio = (double)DownloadedBytes() / (double)UploadedBytes();
			if(Range > Ratio)
				Range = Ratio;
		}
	}
	else if(pFile->IsIncomplete()) // if(UploadedBytes() > DownloadedBytes())
		Range = (double)DownloadedBytes() / (double)UploadedBytes();
	else
		Range = 1.0;

	if(Range < 0.5)
		Range = 0.5;
	else if(Range > 4)
		Range = 4;

	// file priority
	
	{
		if(int iPriority = pFile->GetProperty("Priority", 0).toInt())
		{
				 if(iPriority <= 1)		Range /= 4;
			else if(iPriority <= 3)		Range /= 2;
			else if(iPriority == 5)		; // no change
			else if(iPriority >= 9)		Range *= 8;
			else if(iPriority >= 7)		Range *= 4;
		}
	}
	return Range;
}
