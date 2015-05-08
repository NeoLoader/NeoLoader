#include "GlobalHeader.h"
#include "UploadManager.h"
#include "../FileList/File.h"
#include "../FileList/FileStats.h"
#include "Transfer.h"
#ifndef NO_HOSTERS
#include "./HosterTransfer/HosterLink.h"
#include "./HosterTransfer/WebEngine.h"
#include "./HosterTransfer/WebManager.h"
#include "./HosterTransfer/LoginManager.h"
#endif
#include "./BitTorrent/Torrent.h"
#include "./BitTorrent/TorrentInfo.h"
#include "./BitTorrent/TorrentManager.h"
#include "./ed2kMule/MuleSource.h"
#include "./ed2kMule/MuleClient.h"
#include "../NeoCore.h"
#include "../FileList/FileManager.h"
#include "../Networking/BandwidthControl/BandwidthLimit.h"
#include "../Networking/SocketThread.h"
#include "../../Framework/OtherFunctions.h"
#include <math.h>

CUploadManager::CUploadManager(QObject* qObject)
: QObjectEx(qObject)
{
	m_WaitingUploads = 0;
	m_NextUploadStart = 0;
	m_LastUploadDowngrade = 0;
	m_LastUploadUpgrade = 0;
	m_LastUpLimit = 0;
	m_LastFullSlotCount = 0;
	m_LastTricklSlotCount = 0;

	m_ProbabilityRange = 1.0;
}

#ifndef NO_HOSTERS
bool CUploadManager::OrderUpload(CFile* pFile, QStringList Hosts, CPartMap* pMap)
{
	CPartMap PartMap(pFile->GetFileSize());

	// mark what parts to upload
	if(pMap)
		PartMap.Merge(pMap);
	else
		PartMap.SetRange(0,-1,Part::Selected,CPartMap::eAdd);

	// mark what parts are available
	if(CPartMap* pParts = pFile->GetPartMap())
		PartMap.Merge(pParts);
	else
		PartMap.SetRange(0,-1,Part::Available,CPartMap::eAdd);

	uint64 MaxPartSize = theCore->Cfg()->GetUInt64("Hoster/PartSize");

	for(CPartMap::SIterator FileIter; PartMap.IterateRanges(FileIter, Part::Selected | Part::Available); )
	{
		if((FileIter.uState & (Part::Selected | Part::Available)) == 0)
			continue;

		foreach(const QString& Host, Hosts)
		{
			if(int PartSize = GetPartSizeForHost(Host))
			{
				if(MaxPartSize && PartSize > MaxPartSize)
					PartSize = MaxPartSize;

				CPartMap::SIterator SrcIter(FileIter.uBegin, FileIter.uEnd);
				//for(uint64 Pos=0; Pos < uEnd-uBegin; Pos += PartSize)
				for(;SrcIter.uBegin < SrcIter.uTo; )
				{
					SrcIter.uEnd = Min(SrcIter.uBegin + PartSize, SrcIter.uTo);

					CHosterLink* pHosterLink = new CHosterLink(Host,CHosterLink::eManualUpload);
					pHosterLink->setParent(pFile);
					pHosterLink->SetFileName(GetRand64Str());
					pHosterLink->SetFileSize(SrcIter.uEnd - SrcIter.uBegin);
					if(!NativeCryptoHost(Host))
						pHosterLink->InitCrypto();

					CShareMapPtr pPartMap = CShareMapPtr(new CSynced<CShareMap>(PartMap.GetSize()));
					pPartMap->SetRange(SrcIter.uBegin, SrcIter.uEnd, Part::Selected);
					pHosterLink->SetPartMap(pPartMap);

					pFile->AddTransfer(pHosterLink);

					SrcIter.uBegin = SrcIter.uEnd;
				}
			}
			else
			{
				LogLine(LOG_ERROR, tr("Upload to host %1 is not supported").arg(Host));
			}
		}
	}
	
	return true;
}

bool CUploadManager::OrderSolUpload(CFile* pFile, QStringList Hosts)
{
	if(!pFile->IsComplete())
	{
		LogLine(LOG_ERROR, tr("File %1 is not completly downlaoded and those can not be uplaoded as one").arg(pFile->GetFileName()));
		return false;
	}

	if(pFile->IsMultiFile())
	{
		LogLine(LOG_ERROR, tr("File %1 a multi file and can not be uploaded as one").arg(pFile->GetFileName()));
		return false;
	}

	foreach(const QString& Host, Hosts)
	{
		// A solid upload is a unencrypted single file upload that can be downloaded in one piece with any web browser
		CHosterLink* pHosterLink = new CHosterLink(Host,CHosterLink::eManualUpload);
		pHosterLink->setParent(pFile);
		pHosterLink->SetFileName(GetRand64Str());
		pHosterLink->SetFileSize(pFile->GetFileSize());

		CShareMapPtr pPartMap = CShareMapPtr(new CSynced<CShareMap>(pFile->GetFileSize()));
		pPartMap->SetRange(0, -1, Part::Selected);
		pHosterLink->SetPartMap(pPartMap);

		pFile->AddTransfer(pHosterLink);
	}
	return true;
}

bool CUploadManager::OrderReUpload(CHosterLink* pOldLink)
{
	CFile* pFile = pOldLink->GetFile();

	CShareMap* pMap = pOldLink->GetPartMap();
	if(!pMap && !QFile::exists(theCore->GetTempDir() + QString("%1_").arg(pFile->GetFileID()) + pOldLink->GetFileName()))
	{
		LogLine(LOG_ERROR, tr("Archive for file %1 can not be reuploaded, as its not linger cached").arg(pFile->GetFileName()));
		return false;
	}

	QString Host = pOldLink->GetUploadAcc() + "@" + pOldLink->GetHoster();

	CHosterLink* pHosterLink = new CHosterLink(Host, CHosterLink::eManualUpload);
	pHosterLink->setParent(pFile);

	if(pMap)
	{
		pHosterLink->SetFileName(GetRand64Str());
		pHosterLink->SetFileSize(pOldLink->GetFileSize());
		if(!NativeCryptoHost(Host))
			pHosterLink->InitCrypto();

		CShareMapPtr pPartMap = CShareMapPtr(new CSynced<CShareMap>(pMap->GetSize()));
		for(CShareMap::SIterator Iter; pMap->IterateRanges(Iter, Part::Available); )
		{
			if(Iter.uState & Part::Available)
				pPartMap->SetRange(Iter.uBegin, Iter.uEnd, Part::Selected);
		}
		pHosterLink->SetPartMap(pPartMap);
	}
	else // this is for archive files
	{
		pHosterLink->SetFileName(pOldLink->GetFileName());
		pHosterLink->SetFileSize(pOldLink->GetFileSize());
	}

	pFile->AddTransfer(pHosterLink);

	return true;
}
#endif

bool SlotHeapCmp(QVector<CUploadManager::SUploadSlot> &refArray, uint32 l, uint32 r)
{
	return refArray[l].pTransfer->GetUpLimit()->GetRate() > refArray[r].pTransfer->GetUpLimit()->GetRate();
}

void SlotHeapMov(QVector<CUploadManager::SUploadSlot> &refArray, uint32 r, uint32 l)
{
	CUploadManager::SUploadSlot Slot = refArray[r];
	refArray[r] = refArray[l];
	refArray[l] = Slot;
}


void CUploadManager::Process(UINT Tick)
{
	m_Uploads.clear();

#ifndef NO_HOSTERS
	bool bReUpload = theCore->Cfg()->GetBool("Hoster/AutoReUpload");
	bool bHosterCache = theCore->Cfg()->GetString("HosterCache/CacheMode") != "Off";
#endif

	// eMule Hording BEGIN
	int HordeUploads = 0;
	int HordeActive = 0;
	QMultiMap<double, CMuleSource*> HordePending;
	// eMule Hording END

	m_ProbabilityRange = 0;
	QMap<double, CP2PTransfer*> ReadyUploads;
	foreach(CFile* pFile, theCore->m_FileManager->GetFiles())
	{
		if(!pFile->IsStarted() || pFile->IsPaused())
			continue;

#ifndef NO_HOSTERS
		bool iReUpload = pFile->GetProperty("ReUpload").toInt();
		if(!iReUpload)
			iReUpload = bReUpload ? 1 : 0;
				
		if((Tick & EPerSec) != 0 && pFile->IsAutoShare())
		{
			pFile->SetProperty("HosterUl", bHosterCache);
		}
#endif

		int ActiveUploads = 0;
		int WaitingUploads = 0;
		int PendingUploads = 0;

		foreach(CTransfer* pTransfer, pFile->GetTransfers())
		{
#ifndef NO_HOSTERS
			if(iReUpload == 1 && pTransfer->IsDownload())
			{
				if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer)) // Hoster uplaods get a different threatment
				{
					if(pHosterLink->GetUploadInfo() == CHosterLink::eManualUpload)
					{
						if(pHosterLink->GetError() == "TaskFailed")
						{
							OrderReUpload(pHosterLink);
							pHosterLink->SetDeprecated();
						}
					}
				}
			}
#endif

			if(!pTransfer->IsUpload())
				continue;

			if(pTransfer->IsActiveUpload())
			{
				ActiveUploads++;
				m_Uploads.append(pTransfer);
			}
			else if(pTransfer->IsWaitingUpload())
				WaitingUploads++;

#ifndef NO_HOSTERS
			if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer)) // Hoster uplaods get a different threatment
			{
				if(pHosterLink->IsWaitingUpload() && theCore->m_WebManager->GetUploads() < theCore->Cfg()->GetInt("Hoster/MaxUploads"))
				{
					if(!pHosterLink->HasError()) // if its a IsWaitingUpload with an error its waiting for a error reset and retry
						pHosterLink->StartUpload();
				}

				PendingUploads++;
			}
			else 
#endif
				if(CP2PTransfer* pP2PTransfer = qobject_cast<CP2PTransfer*>(pTransfer))
			{
				if(pP2PTransfer->IsWaitingUpload())
				{
					m_ProbabilityRange += pP2PTransfer->GetProbabilityRange();

					ReadyUploads.insert(m_ProbabilityRange,pP2PTransfer);
				}

				// eMule Hording BEGIN
				if(CMuleSource* pMuleSource = qobject_cast<CMuleSource*>(pP2PTransfer))
				{
					if(CMuleClient* pMuleClient = pMuleSource->GetClient())
					{
						if(pMuleClient->ProtocolRevision() != 0 && (pTransfer->IsWaitingDownload() || pTransfer->IsActiveDownload()))
						{
							if(pTransfer->IsActiveUpload())
							{
								HordeUploads++;
								if(pMuleClient->GetHordeState() != CMuleClient::eRejected)
									HordeActive++; // count not yet asked horce clients as active
							}
							else if(pTransfer->IsWaitingUpload())
							{
								HordePending.insert(-pP2PTransfer->GetProbabilityRange(), pMuleSource); // conunt of clients potentialy capable of horde
							}
						}
					}
				}
				// eMule Hording END
			}
		}

		pFile->SetUploads(ActiveUploads, WaitingUploads);

#ifndef NO_HOSTERS
		if((Tick & EPer10Sec) != 0 && !pFile->IsRawArchive() && pFile->IsHosterUl() && PendingUploads == 0) // only every 10 sec
			InspectCache(pFile);
#endif
	}

	m_WaitingUploads = ReadyUploads.count();

	int MaxSlotSpeed = theCore->Cfg()->GetInt("Upload/SlotSpeed");
	if(!MaxSlotSpeed)
		MaxSlotSpeed = KB2B(3);
	//bool bSlotFocus = theCore->Cfg()->GetBool("Upload/SlotFocus");
	int TrickleVolume = theCore->Cfg()->GetInt("Upload/TrickleVolume");
	bool bDropBlocking = theCore->Cfg()->GetBool("Upload/DropBlocking");
	//int PromotionThreshold = MaxSlotSpeed * theCore->Cfg()->GetInt("Upload/PromotionThreshold") / 100;
	int TrickleSpeed = theCore->Cfg()->GetInt("Upload/TrickleSpeed");
	if(!TrickleSpeed)
		TrickleSpeed = KB2B(1);

	int TricklePrio = BW_PRIO_NORMAL * TrickleSpeed / MaxSlotSpeed; // BW_PRIO_LOWEST;
	if(TricklePrio < 1)
		TricklePrio = 1;
	if(TricklePrio >= BW_PRIO_NORMAL/2)
		TricklePrio = (BW_PRIO_NORMAL/2) - 1;
	

	int SlotCount = 0;
	int SpeedSumm = 0;
	int TricklSpeedSumm = 0;
	int TricklSlotCount = 0;

	QList<CTransfer*> NewUploads;
	foreach(CTransfer* pTransfer, m_Uploads)
		NewUploads.append(pTransfer);
	for(int i=0; i < m_UploadSlots.count(); i++)
	{
		SUploadSlot* pUploadSlot = &m_UploadSlots[i];

		if(!NewUploads.removeOne(pUploadSlot->pTransfer))
		{
			m_UploadSlots.remove(i--);
			continue;
		}

		CBandwidthLimit* pLimit = pUploadSlot->pTransfer->GetUpLimit();
		ASSERT(pLimit);

		if(!(pUploadSlot->pTransfer->PendingBytes() == 0))
			pUploadSlot->StalledTimeOut = -1;
		else if(pUploadSlot->StalledTimeOut == -1)
			pUploadSlot->StalledTimeOut = GetCurTick() + SEC2MS(60);

		if(pUploadSlot->StalledTimeOut < GetCurTick())
		{
			pUploadSlot->pTransfer->LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb, reason: client stalling")
				.arg(pUploadSlot->pTransfer->GetFile()->GetFileName()).arg(pUploadSlot->pTransfer->GetDisplayUrl()).arg((double)pUploadSlot->pTransfer->LastUploadedBytes()/1024.0));
			pUploadSlot->pTransfer->StopUpload(true);
			m_UploadSlots.remove(i--);
			continue;
		}

		if(!(pUploadSlot->pTransfer->IsBlocking() && pLimit->GetRate() < MaxSlotSpeed))
			pUploadSlot->BlockingTimeOut = -1;
		else if(pUploadSlot->BlockingTimeOut == -1)
			pUploadSlot->BlockingTimeOut = GetCurTick() + SEC2MS(30);

		if(pUploadSlot->BlockingTimeOut < GetCurTick())
		{
			if(bDropBlocking)
			{
				pUploadSlot->pTransfer->LogLine(LOG_DEBUG | LOG_INFO, tr("Stopping upload of %1 to %2 after %3 kb, reason: socket blocking")
					.arg(pUploadSlot->pTransfer->GetFile()->GetFileName()).arg(pUploadSlot->pTransfer->GetDisplayUrl()).arg((double)pUploadSlot->pTransfer->LastUploadedBytes()/1024.0));
				pUploadSlot->pTransfer->StopUpload();
				m_UploadSlots.remove(i--);
				continue;
			}
			else if(pUploadSlot->eType != SUploadSlot::eBlocking)
			{
				pUploadSlot->StrikeCount ++;
				pUploadSlot->eType = SUploadSlot::eBlocking;
				pLimit->SetPriority(BW_PRIO_NORMAL/2);
			}
		}

		switch(pUploadSlot->eType)
		{
		case SUploadSlot::eTrickle:
			if(pLimit->GetRate() >= MaxSlotSpeed - TrickleSpeed) // if a trickle is at high speed promote it to full
			{
				pUploadSlot->eType = SUploadSlot::eFull;
				pLimit->SetPriority(BW_PRIO_NORMAL);
			}
			else
			{
				TricklSpeedSumm += pLimit->GetRate();
				TricklSlotCount ++;
			}
			break;

		//case SUploadSlot::eFocus:
		//	if(!bSlotFocus)
		//	{
		//		pUploadSlot->eType = SUploadSlot::eFull;
		//		pLimit->SetPriority(BW_PRIO_NORMAL);
		//	}
		case SUploadSlot::eFull:
			SpeedSumm += pLimit->GetRate();
			SlotCount ++;
			break;

		case SUploadSlot::eBlocking:
			if(pUploadSlot->BlockingTimeOut == -1 && pLimit->GetRate() >= MaxSlotSpeed / 2 && pUploadSlot->StrikeCount < 3)
			{
				pUploadSlot->eType = SUploadSlot::eFull;
				pLimit->SetPriority(BW_PRIO_NORMAL);
			}
			break;
		}
		
	}
	foreach(CTransfer* pTransfer, NewUploads)
	{
		if(CP2PTransfer* pP2PTransfer = qobject_cast<CP2PTransfer*>(pTransfer))
		{
			m_UploadSlots.append(SUploadSlot(pP2PTransfer));
			TricklSlotCount++; // Note: new slots start as trickles

			CBandwidthLimit* pLimit = pP2PTransfer->GetUpLimit();
			ASSERT(pLimit);

			pLimit->SetPriority(TricklePrio);
		}
	}

	HeapSort(m_UploadSlots, SlotHeapMov, SlotHeapCmp); // sort slots by speed;

	//if(bSlotFocus)
	//{
	//	for(int i=0; i < m_UploadSlots.count(); i++)
	//	{
	//		SUploadSlot* pUploadSlot = &m_UploadSlots[i];
	//		if(pUploadSlot->eType == SUploadSlot::eBlocking) // blocking dotn bother
	//			continue;
	//
	//		CBandwidthLimit* pLimit = pUploadSlot->pTransfer->GetUpLimit();
	//		ASSERT(pLimit);
	//
	//		if(i == 0)
	//		{
	//			if(pUploadSlot->eType != SUploadSlot::eFocus)
	//			{
	//				pUploadSlot->eType = SUploadSlot::eFocus;
	//				pLimit->SetPriority(BW_PRIO_HIGHEST);
	//			}
	//		}
	//		else
	//		{
	//			if(pUploadSlot->eType == SUploadSlot::eFocus)
	//			{
	//				pUploadSlot->eType = SUploadSlot::eFull;
	//				pLimit->SetPriority(BW_PRIO_NORMAL);
	//			}
	//		}
	//	}
	//}
	//else
	{
		// trickle slowest slots
		int SlotUndershoot = SpeedSumm < (SlotCount * MaxSlotSpeed) ? (SlotCount * MaxSlotSpeed) - SpeedSumm: 0;
		int DowngradeSlots = SlotUndershoot / MaxSlotSpeed;
		if(DowngradeSlots > 0 && GetCurTick() - m_LastUploadDowngrade > AVG_INTERVAL)
		{
			m_LastUploadDowngrade = GetCurTick();

			for(int i=m_UploadSlots.count()-1; i >=0 && DowngradeSlots > 0; i--)
			{
				SUploadSlot* pUploadSlot = &m_UploadSlots[i];
				if(pUploadSlot->eType == SUploadSlot::eTrickle || pUploadSlot->eType == SUploadSlot::eBlocking) // already trickle, or even blocking
					continue;

				CBandwidthLimit* pLimit = pUploadSlot->pTransfer->GetUpLimit();
				ASSERT(pLimit);

				TricklSlotCount++;
				DowngradeSlots--;

				pUploadSlot->eType = SUploadSlot::eTrickle;
				pLimit->SetPriority(TricklePrio);
			}
		}

		m_LastFullSlotCount = SlotCount;
		m_LastTricklSlotCount = TricklSlotCount;

		// if promote some slots to full status
		int SlotOvershoot = SpeedSumm > (SlotCount * MaxSlotSpeed) ? SpeedSumm - (SlotCount * MaxSlotSpeed) : 0;
		int PromoteSlots1 = SlotOvershoot / MaxSlotSpeed;

		int TrickleOvershoot = TricklSpeedSumm > (TricklSlotCount * TrickleSpeed) ? TricklSpeedSumm - (TricklSlotCount * TrickleSpeed) : 0;
		int PromoteSlots2 = TrickleOvershoot / MaxSlotSpeed;

		int PromoteSlots = Max(PromoteSlots1, PromoteSlots2);
		if(PromoteSlots > 0 && GetCurTick() - m_LastUploadUpgrade > AVG_INTERVAL)
		{
			m_LastUploadUpgrade = GetCurTick();

			for(int i=0; i < m_UploadSlots.count() && PromoteSlots > 0; i++)
			{
				SUploadSlot* pUploadSlot = &m_UploadSlots[i];
				if(pUploadSlot->eType != SUploadSlot::eTrickle)
					continue;

				CBandwidthLimit* pLimit = pUploadSlot->pTransfer->GetUpLimit();
				ASSERT(pLimit);

				TricklSlotCount--;
				PromoteSlots--;

				pUploadSlot->eType = SUploadSlot::eFull;
				pLimit->SetPriority(BW_PRIO_NORMAL);
			}
		}
	}

	// check if we have enough slots to fill the bandwidth we have
	int UpLimit = theCore->m_Network->GetUpLimit()->GetLimit();
	int UpRate = theCore->m_Network->GetUpLimit()->GetRate();
	int AddSlots = UpRate < UpLimit ? (UpLimit - UpRate) / MaxSlotSpeed : 0;

	// if there was an abrupt change in max speed, block slot opening and wait for the BW to stabilizy
	if(abs(m_LastUpLimit - UpLimit) > MaxSlotSpeed)
		m_NextUploadStart = GetCurTick() + AVG_INTERVAL * 2;
	m_LastUpLimit = UpLimit;

	// check if we have enough trickles
	int AddTrickle = 0;
	if(TricklSlotCount < TrickleVolume)
		AddTrickle = TrickleVolume - TricklSlotCount;

	// eMule Hording BEGIN
	while(!HordePending.isEmpty() && HordeActive < theCore->Cfg()->GetInt("Ed2kMule/MinHordeSlots") && (HordeUploads == 0 || AcceptHorde()))
	{
		HordeActive++;
		AddSlots--;

		QMap<double, CMuleSource*>::iterator I = HordePending.begin();
		ASSERT(I != HordePending.end());
		CMuleSource* pMuleSource = I.value();
		ReadyUploads.remove(ReadyUploads.key(pMuleSource));

		AddSlots--;
		pMuleSource->GetFile()->LogLine(LOG_DEBUG | LOG_INFO, tr("Starting upload of %1 to %2 (hording)").arg(pMuleSource->GetFile()->GetFileName()).arg(pMuleSource->GetDisplayUrl()));
		pMuleSource->StartUpload();
	}
	// eMule Hording END

	if(TrickleVolume && (AddTrickle > 0 || (AddSlots > 0 && m_NextUploadStart <= GetCurTick())))
	{
		m_NextUploadStart = GetCurTick() + AVG_INTERVAL;
		if(AddSlots < AddTrickle)
			AddSlots = AddTrickle;
		
		while(!ReadyUploads.isEmpty() && AddSlots > 0)
		{
			// U-ToDo-Now: add wait time based uplad start falback for unlucky clients

			double dRand = double(GetRand64() & 0x00000000FFFFFFFF)/1000.0;
			double dMod = fmod(dRand, m_ProbabilityRange);
			QMap<double, CP2PTransfer*>::iterator I = ReadyUploads.upperBound(dMod);
			if(I == ReadyUploads.end()) // fallback
				I = ReadyUploads.find(ReadyUploads.keys().at(rand()%ReadyUploads.count()));
			ASSERT(I != ReadyUploads.end());
			CP2PTransfer* pP2PTransfer = I.value();
			ReadyUploads.erase(I);

			AddSlots--;
			pP2PTransfer->GetFile()->LogLine(LOG_DEBUG | LOG_INFO, tr("Starting upload of %1 to %2").arg(pP2PTransfer->GetFile()->GetFileName()).arg(pP2PTransfer->GetDisplayUrl()));
			pP2PTransfer->StartUpload();

			m_StartHistory.append(GetCurTick());
			while(m_StartHistory.count() > theCore->Cfg()->GetInt("Upload/HistoryDepth"))
				m_StartHistory.removeFirst();
		}
	}
}

bool CUploadManager::OfferHorde(CP2PTransfer* pP2PTransfer)
{
	for(int i=0; i < m_UploadSlots.count(); i++)
	{
		SUploadSlot* pUploadSlot = &m_UploadSlots[i];
		if(pUploadSlot->pTransfer == pP2PTransfer)
			// Note: we dont offer hording to trickles
			return pUploadSlot->eType == SUploadSlot::eFull;
	}
	return false;
}

bool CUploadManager::AcceptHorde()
{
	int TrickleVolume = theCore->Cfg()->GetInt("Upload/TrickleVolume");
	int TrickleExcess = m_LastTricklSlotCount > TrickleVolume ? m_LastTricklSlotCount - TrickleVolume : 0;

	if(TrickleExcess > m_LastFullSlotCount)
		return false; // we have more Excess trickles than actual full slots sorry but we cant open yet anoter one
	return true;
}

bool CUploadManager::AcceptHorde(CP2PTransfer* pP2PTransfer)
{
	if(pP2PTransfer->IsActiveUpload())
		return true; // we are already uploading booth ways ofcause YES!

	if(!AcceptHorde())
		return false;

	pP2PTransfer->GetFile()->LogLine(LOG_DEBUG | LOG_INFO, tr("Starting Upload of %1 to %2 (horde)").arg(pP2PTransfer->GetFile()->GetFileName()).arg(pP2PTransfer->GetDisplayUrl()));
	pP2PTransfer->StartUpload();
	return true;
}

#define isnan(x) (x != x)
#define isinf(x) (!isnan(x) && isnan(x - x))

struct SCachePart
{
	SCachePart(uint64 b, uint64 e, uint64 a, uint64 c, uint64 n)
		: uBegin(b), uEnd(e), uAvail(a), uCached(c), uNeeded(n) 
	{
		ASSERT(uAvail <= uEnd - uBegin);
	}

	uint64 uBegin;
	uint64 uEnd;
	uint64 uAvail;
	uint64 uCached;
	uint64 uNeeded;

	uint64 Length()	const {ASSERT(uEnd > uBegin); return uEnd - uBegin;}

	int Score(uint64 FileSize, uint64 PartSize) const 
	{
		double Score = 10000;
		if(uCached >= uNeeded)
			return 0;

		uint64 uLength = Length();

		// calculate score
		Score *= (double)uNeeded;
		if(uCached > 0)
			Score /= (double)uCached;
		else
			Score /= (double)uLength;

		// modify score with whats available
		ASSERT(uLength >= uAvail);
		double SpanMod = (double)uAvail/(double)uLength;
		if(SpanMod < 0.01)
			SpanMod = 0.01;
		Score *= SpanMod;

		// modify score prefer large parts
		ASSERT(PartSize >= uAvail);
		double PartMod = (double)uAvail/(double)PartSize;
		if(PartMod < 0.01)
			PartMod = 0.01;
		Score *= PartMod;

		ASSERT(!isinf(Score) || !isnan(Score));

		return (uint64)Score;
	}
};

#ifndef NO_HOSTERS
void CUploadManager::InspectRange(QMultiMap<int, SCachePart>& CacheQueue, uint64 uBegin, uint64 uEnd, bool bAvail, uint32 uCached, uint32 uNeeded, uint64 FileSize, uint64 PartSize)
{
	if(uNeeded > 10)
		uNeeded = 10;

	QMultiMap<int, SCachePart> TempQueue;
	while(uEnd - uBegin > PartSize)
	{
		SCachePart CurPart(uBegin, uBegin + PartSize, bAvail ? PartSize : 0, uCached * PartSize, uNeeded * PartSize);
		TempQueue.insert(CurPart.Score(FileSize, PartSize), CurPart);

		uBegin += PartSize;
	}

	uint64 uLength = uEnd - uBegin;
	SCachePart CurPart(uBegin, uEnd, bAvail ? uLength : 0, uCached * uLength, uNeeded * uLength);
	TempQueue.insert(CurPart.Score(FileSize, PartSize), CurPart);

	if(TempQueue.count() == 1)
	{
		for(QMultiMap<int, SCachePart>::iterator I = CacheQueue.begin(); I != CacheQueue.end(); I++) // maps are sorted 0 ... n
		{
			const SCachePart& OldPart = I.value();
			if(OldPart.uEnd == CurPart.uBegin)
			{
				if(OldPart.uBegin - CurPart.uEnd > PartSize)
					continue;

				SCachePart NewPart(OldPart.uBegin, CurPart.uEnd, OldPart.uAvail + CurPart.uAvail, OldPart.uCached + CurPart.uCached, OldPart.uNeeded + CurPart.uNeeded);
				TempQueue.insert(NewPart.Score(FileSize, PartSize), NewPart);
			}
		}
	}

	CacheQueue.unite(TempQueue);
	while(CacheQueue.count() > 100) // maps are sorted 0 ... n
	{
		QMultiMap<int, SCachePart>::iterator I = CacheQueue.begin();
		CacheQueue.erase(I);
	}
}

void CUploadManager::InspectCache(CFile* pFile)
{
	uint64 FileSize = pFile->GetFileSize();
	CPartMap* pParts = pFile->GetPartMap();
	ASSERT(pParts || !pFile->IsIncomplete()); // no parts means file is complete
	CCacheMap* pCacheMap = pFile->GetStats()->GetCacheMap();
	if(!pCacheMap)
		return;

	uint64 PartSize = theCore->Cfg()->GetUInt64("HosterCache/PartSize");
	QMultiMap<int, SCachePart> CacheQueue;

	for(CCacheMap::SIterator CacheIter;pCacheMap->IterateRanges(CacheIter);)
	{
		if(!pParts)
			InspectRange(CacheQueue, CacheIter.uBegin, CacheIter.uEnd, true, CacheIter.uState.uAvail, CacheIter.uState.uNeed, FileSize, PartSize);
		else
		{
			for(CPartMap::SIterator FileIter(CacheIter.uBegin, CacheIter.uEnd);pParts->IterateRanges(FileIter, Part::Available | Part::Verified);)
			{
				bool bAvail = (FileIter.uState & (Part::Available | Part::Verified)) != 0;
				InspectRange(CacheQueue, FileIter.uBegin, FileIter.uEnd, bAvail, CacheIter.uState.uAvail, CacheIter.uState.uNeed, FileSize, PartSize);
			}
		}
	}

	bool bAll = theCore->Cfg()->GetString("HosterCache/CacheMode") == "All";
	bool bHasNeeded = false; 
	CShareMapPtr pPartMap;
	int MaxRanges = 0;
	uint64 uTotalLength = 0;
	for(QMultiMap<int, SCachePart>::iterator I = CacheQueue.end();;) // maps are sorted 0 ... n
	{
		if(I == CacheQueue.begin())
			break;
		const SCachePart& CachePart = (--I).value();

		if(CachePart.uNeeded)
			bHasNeeded = true;
		else if(!bAll)
			continue;

		if(CachePart.uAvail == 0)// uAvail is how much we have and could cache
			continue;
		if(CachePart.uCached/CachePart.uAvail >= theCore->Cfg()->GetInt("HosterCache/MaxAvail"))
			continue;

		if(PartSize < uTotalLength + CachePart.uAvail)
			continue;

		if(!pPartMap)
			pPartMap = CShareMapPtr(new CSynced<CShareMap>(FileSize));
		if(!pParts) // if no parts than the file is completed
		{
			pPartMap->SetRange(CachePart.uBegin, CachePart.uEnd, Part::Selected);
			uTotalLength += CachePart.uEnd - CachePart.uBegin;
		}
		else // else mark only available parts for upload
		{
			for(CPartMap::SIterator FileIter(CachePart.uBegin, CachePart.uEnd); pParts->IterateRanges(FileIter, Part::Available | Part::Verified); )
			{
				if((FileIter.uState & (Part::Available | Part::Verified)) != 0)
				{
					pPartMap->SetRange(FileIter.uBegin, FileIter.uEnd, Part::Selected);
					uTotalLength += FileIter.uEnd - FileIter.uBegin;
				}
			}
		}

		if((uTotalLength >= PartSize / 2) || (++MaxRanges >= theCore->Cfg()->GetInt("HosterCache/MaxJoints")))
		{
			if(bHasNeeded)
				break;
			else // the entire part is not needed, try the next 100 mb
				pPartMap->Reset(FileSize);
		}
	}

	if(!bHasNeeded || pPartMap == NULL)
		return;

	QString Host = SelectUploadHost(pFile, pPartMap.data());
	if(Host.isEmpty())
		return;

	CHosterLink* pHosterLink = new CHosterLink(Host,CHosterLink::eAutoUpload);
	pHosterLink->setParent(pFile);
	pHosterLink->SetFileName(GetRand64Str());
	pHosterLink->SetPartMap(pPartMap);
	//pHosterLink->SetFileSize(CachePart.uAvail); // should not be nececery
	if(!NativeCryptoHost(Host))
		pHosterLink->InitCrypto();

	pFile->AddTransfer(pHosterLink);
}

QString CUploadManager::SelectUploadHost(CFile* pFile, CShareMap* pPartMap)
{
	CHosterMap* pHosters = pFile->GetStats()->GetHosterMap();

	QString Mode = theCore->Cfg()->GetString("HosterCache/SelectionMode");
	bool bAll = Mode.compare("All", Qt::CaseInsensitive) == 0;
	bool bSelected = Mode.compare("Selected", Qt::CaseInsensitive) == 0;
	
	QMultiMap<double, QString> Hosters; // smaller is better
	foreach(CWebScriptPtr pScript, theCore->m_WebManager->GetAllScripts())
	{
		if(!pScript->GetAPIs().contains("Upload"))
			continue; // can't upload to that one

		QString HostName = pScript->GetScriptName();

		// take whats selected
		bool NoLogin = theCore->m_WebManager->GetOption(HostName + "/Upload").toBool() && pScript->HasAnonUpload();
		QStringList Logins = theCore->m_LoginManager->GetUploadLogins(HostName);

		if(NoLogin || !Logins.isEmpty()) // do we have seomthing selected
		{
			if(!bAll && !bSelected) // auto - switch from all to selected
			{
				Hosters.clear();
				bSelected = true;
			}
		}
		else if(bSelected)
			continue;

		if(!bSelected) // we dont want selected
		{
			// take all we can
			NoLogin = pScript->HasAnonUpload();
			Logins = theCore->m_LoginManager->GetUploadLogins(HostName, false);
		}

		if(NoLogin)
			Logins.append("");
		if(Logins.isEmpty())
			continue;

		// Calculate an availability score for the entire part we would like to upload
		double Score = 0;
		int Count = 0;
		for(CShareMap::SIterator RangeIter; pPartMap->IterateRanges(RangeIter, Part::Selected); )
		{
			if(RangeIter.uState & Part::Selected)
			{
				Count++;
				
				// H-ToDo-Now: for all known domains!!!!!!!!!!!
				uint64 uLength = RangeIter.uEnd - RangeIter.uBegin;
				for(CHosterMap::SIterator HosterIter(RangeIter.uBegin, RangeIter.uEnd); pHosters->IterateRanges(HosterIter, CHosterMap::MkValue(HostName, "", -1)); )
				{
					if(!HosterIter.uState.isEmpty())
						Score += (double)(HosterIter.uEnd - HosterIter.uBegin) * HosterIter.uState.values().first().Total() / uLength;
				}
			}
		}
		if(!Count)
			continue;
		Score /= Count;

		foreach(const QString& Login, Logins)
			Hosters.insert(Score, Login.isEmpty() ? HostName : (Login + "@" + HostName));
	}

	if(Hosters.isEmpty() || Hosters.keys().first() >= 1) // dont double upload on hosters
		return "";
	QStringList FinalHosters = Hosters.values(Hosters.keys().first());
	return FinalHosters.at(qrand()%FinalHosters.count());
}

uint64 CUploadManager::GetPartSizeForHost(const QString& Host)
{
	if(CWebScriptPtr pScript = theCore->m_WebManager->GetScriptForUrl(Host, "Upload"))
		return pScript->GetMaxPartSize();
	return 0;
}

bool CUploadManager::NativeCryptoHost(const QString& Host)
{
	if(CWebScriptPtr pScript = theCore->m_WebManager->GetScriptForUrl(Host, "Upload"))
		return pScript->HasNativeCrypto();
	return false;
}

bool CUploadManager::UploadsPending(CFile* pFile)
{
	foreach(CTransfer* pTransfer, pFile->GetTransfers())
	{
		if(!pTransfer->IsUpload())
			continue;
		if(!pTransfer->inherits("CHosterLink"))
			continue;
		// X-ToDo-Now: is that so?
		if(!pTransfer->HasError())
			return true;
	}
	return false;
}
#endif
