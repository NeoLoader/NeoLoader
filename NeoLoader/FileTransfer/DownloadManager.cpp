#include "GlobalHeader.h"
#include "DownloadManager.h"
#include "../FileList/File.h"
#include "../FileList/FileStats.h"
#include "Transfer.h"
#include "../NeoCore.h"
#include "../FileList/FileManager.h"
#ifndef NO_HOSTERS
#include "./HosterTransfer/HosterLink.h"
#include "./HosterTransfer/ArchiveDownloader.h"
#include "./HosterTransfer/WebManager.h"
#include "./HosterTransfer/LoginManager.h"
#include "./HosterTransfer/ConnectorManager.h"
#include "./HosterTransfer/HosterTask.h"
#endif
#include "./BitTorrent/Torrent.h"
#include "./BitTorrent/TorrentInfo.h"
#include "HashInspector.h"
#include "CorruptionLogger.h"

CDownloadManager::CDownloadManager(QObject* qObject)
 : QObjectEx(qObject)
{
	m_WaitingDownloads = 0;
}

void CDownloadManager::UpdateQueue()
{
	QMultiMap<int, QPair<CFile*, int> > Queue;
	foreach(CFile* pFile, theCore->m_FileManager->GetFiles())
	{
		if(pFile->IsRemoved())
			continue;

		int QueuePos = pFile->GetQueuePos();
		if(!QueuePos) // put new added downloads on the end of the queue, always
			QueuePos = INT_MAX;
		Queue.insert(QueuePos, qMakePair(pFile, QueuePos));
	}

	int Counter = 0;
	for(QMultiMap<int, QPair<CFile*, int> >::iterator I = Queue.begin(); I != Queue.end(); I++)
		I->first->SetQueuePos(++Counter, false);
}

void CDownloadManager::Process(UINT Tick)
{
	m_Downloads.clear();
	m_WaitingDownloads = 0;

	if((Tick & EPerSec) != 0)
		UpdateQueue();

	time_t uNow = GetTime();
	int iMaxHosterDownloads = theCore->Cfg()->GetInt("Hoster/MaxDownloads");

	bool bAutoP2P = theCore->Cfg()->GetBool("Content/AutoP2P");
	bool bEd2kShare = theCore->Cfg()->GetBool("Ed2kMule/ShareDefault");
#ifndef NO_HOSTERS
	//bool bHosterDl = theCore->Cfg()->GetBool("Hoster/Enable");
	bool bCaptchaOK = theCore->Cfg()->GetBool("Hoster/UseCaptcha");
#endif 

	foreach(CFile* pFile, theCore->m_FileManager->GetFiles())
	{
		if(!pFile->IsStarted() || pFile->IsPaused() || pFile->IsComplete())
			continue;

		// Download Management
		if((Tick & EPerSec) != 0 && pFile->IsAutoShare())
		{
			pFile->SetProperty("NeoShare", true);
#ifndef NO_HOSTERS
			pFile->SetProperty("HosterDl", (bCaptchaOK ? 2 : 1)); 
#endif
			switch(pFile->GetMasterHash()->GetType())
			{
				case HashTorrent: // Master = Torrent -> bt 1, ed2k 0
					pFile->SetProperty("Torrent", 1);
					if(bEd2kShare)
						pFile->SetProperty("Ed2kShare", true);
					break;
				case HashMule:
				case HashEdPr:
					ASSERT(0);
				case HashEd2k: // Master = Ed2k -> bt 1, ed2k 1
					pFile->SetProperty("Torrent", 1);
					pFile->SetProperty("Ed2kShare", true);
					break;
				default: // Master = NeoX | Neo | Arch -> bt 0, ed2k 0
					pFile->SetProperty("Torrent", 1);
					if(bEd2kShare)
						pFile->SetProperty("Ed2kShare", true);
			}
		}

		if((Tick & EPer10Sec) != 0 && pFile->IsAutoShare())
		{
			if(bAutoP2P && !pFile->IsPending() && !pFile->MetaDataMissing()) // dont do that to often
			{
				time_t uStart = pFile->GetProperty("StartTime").toDateTime().toTime_t();
				CFileHashPtr pFileHash = pFile->GetMasterHash();
				bool bStalled = uStart < uNow && uNow - uStart > 20 && pFile->GetStatusStats(Part::Available) == 0;

				bool bIncomplete = false;
				time_t uComplete = pFile->GetProperty("LastComplete").toDateTime().toTime_t();
				if(pFile->GetStats()->GetAvailStats() >= 1)
					pFile->SetProperty("LastComplete", (uint64)uNow);
				else if(uComplete < uNow && uNow - uComplete > MIN2S(20))
					bIncomplete = true;

				bool bIsNeoFile = pFileHash->GetType() == (pFile->IsMultiFile() ? HashXNeo : HashNeo);
				// Note: forign master hash  means file is a sub file
				bool bHasForighHash = pFile->GetHashPtrEx(pFileHash->GetType(), pFileHash->GetHash()) == NULL; 

#ifndef NO_HOSTERS
				if(!pFile->IsArchive())
				{
					if(bStalled && bIsNeoFile && !bHasForighHash
					 && pFile->GetStats()->GetTransferCount(eBitTorrent) == 0
					 && pFile->GetStats()->GetTransferCount(eEd2kMule) == 0
					 && pFile->GetStats()->GetTransferCount(eNeoShare) == 0
					 && !pFile->GetArchives().isEmpty())
						pFile->SetProperty("HosterDl", 3); 
				}
			
				if(!pFile->IsArchive())
				// file must not have a forign hash - we dont want sub files to always end up in ed2k etc...
				//if(theCore->Cfg()->GetInt("Content/P2PFallback") == 2 && !bHasForighHash && (bStalled || bIncomplete))
#endif
					if(bIsNeoFile && !bHasForighHash && (bStalled || bIncomplete))
				{
					CTorrent* pTorrent = pFile->GetTopTorrent();
					if(!pTorrent || !pTorrent->GetInfo()->IsPrivate())
						pFile->SetProperty("Torrent", 1);
					pFile->SetProperty("Ed2kShare", true);
				}
			}
		}

#ifndef NO_HOSTERS
		if(pFile->IsArchive())
		{
			if(theCore->m_ArchiveDownloader->ProcessArchive(pFile))
				continue; 

			//archive couldnt be handled, go back to p2p
			pFile->SetProperty("HosterDl", (bCaptchaOK ? 2 : 1)); 
			//if(CFileHashPtr pHash = pFile->GetHashPtr(pFile->IsMultiFile() ? HashXNeo : HashNeo))
			//	pFile->SetMasterHash(pHash); 
		} 
#endif
		//


		int ActiveDownloads = 0;
		int WaitingDownloads = 0;

#ifndef NO_HOSTERS
		int iHosterDl = pFile->IsHosterDl();

		bool bBusy = theCore->m_WebManager->GetDownloads() >= iMaxHosterDownloads; // we cant download anythign right now, all slots are busy

		QList<CHosterLink*> HosterLinks;
		QList<CHosterLink*> HosterLinksAux;

		// Stats
		int TransferringCount = 0;
		int ActiveCount = 0;
		uint64 uTimeToWait = -1;
		uint64 uBlockedTime = -1;
		QString HosterError;
		// /Stats
#endif

		foreach(CTransfer* pTransfer, pFile->GetTransfers())
		{
			if(!pTransfer->IsDownload())
				continue;

			if(pTransfer->IsActiveDownload())
			{
				ActiveDownloads++;
				m_Downloads.append(pTransfer);
			}
			else if(pTransfer->IsWaitingDownload())
				WaitingDownloads++;

#ifndef NO_HOSTERS
			if(CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer))
			{
				if(!iHosterDl)
					continue;

				bool bSolid = false;
				if(!pHosterLink->GetPartMap())
				{
					if(!pHosterLink->GetArchive() && pHosterLink->IsChecked())
						bSolid = true;
					else
						continue; // thats an archive link
				}

				if(pHosterLink->HasError())
				{
					// Stats
					if(HosterError.isEmpty()) // take the first links error only
						HosterError = pHosterLink->GetError();
					// /Stats
				}
				else
				{
					// Stats
					if(pHosterLink->IsDownloading())
						TransferringCount++;
					else if(CHosterTask* pTask = qobject_cast<CHosterTask*>(pHosterLink->GetTask()))
					{
						ActiveCount++;

						uint64 uCurTimeToWait = pTask->GetTimeToWait();
						if(uCurTimeToWait && uCurTimeToWait < uTimeToWait)
							uTimeToWait = uCurTimeToWait;
					}
					// /Stats

					if(pHosterLink->GetStatus(CHosterLink::eDown) != CHosterLink::eReady)
						continue;

					if(bBusy || !pHosterLink->IsChecked())
					{
						bBusy = true;
						continue;
					}

					CWebScriptPtr pScript = theCore->m_WebManager->GetScriptByName(pHosterLink->GetHoster());

					bool bFree = !theCore->m_LoginManager->HaveLogin(pHosterLink->GetHoster());
					int MaxActive = pScript ? pScript->GetMaxActive("Download", bFree) : CWebScript::GetMaxActiveDefault("Download");
					uint64 uCurBlockedTime = 0;
					if(theCore->m_ConnectorManager->GetIP(pScript ? pScript->GetScriptName() : CWebManager::GetRootHost(pHosterLink->GetRawUrl()), QString("Download") + (bFree ? "" : "-Free"), MaxActive, &uCurBlockedTime).isEmpty())
					{
						// Stats
						if(uCurBlockedTime && uCurBlockedTime < uBlockedTime)
							uBlockedTime = uCurBlockedTime;
						// /Stats
						continue; // this hoster is not available right now
					}

					// Note: Solid Files anr archives are always ok with captcha, part downloads depand on preset
					if(!bSolid && iHosterDl >= 2 && pScript && bFree && !pScript->HasNoCaptcha())
						continue; // we dont want this download we would need to solve a captcha
				}

				if(bSolid)
					HosterLinksAux.append(pHosterLink);
				else
					HosterLinks.append(pHosterLink);
			}
#endif
		}

		pFile->SetDownloads(ActiveDownloads, WaitingDownloads);

		m_WaitingDownloads += WaitingDownloads;

		CPartMap* pFileParts = pFile->GetPartMap();

#ifndef NO_HOSTERS
		if(!bBusy)
		{
			if(pFileParts)
			{
				do
				{
					bool bPrem = false;
					uint64 BestVolume = 0;
					CHosterLink* pBestLink = NULL;
					foreach(CHosterLink* pHosterLink, HosterLinks)
					{
						uint64 CurVolume = 0;
						bool bFree = !theCore->m_LoginManager->HaveLogin(pHosterLink->GetHoster());
						if(bPrem && bFree)
							continue;

						CShareMap* pParts = pHosterLink->GetPartMap();
						CShareMap::SIterator SrcIter;
						while(pParts->IterateRanges(SrcIter))
						{
							if((SrcIter.uState & Part::Available) == 0 || (SrcIter.uState & Part::Verified) == 0) // if the reange is not available its irrelevant
								continue;
							ASSERT((SrcIter.uState & Part::Marked) == 0);

							CPartMap::SIterator FileIter(SrcIter.uBegin, SrcIter.uEnd);
							while(pFileParts->IterateRanges(FileIter))
							{
								// check if this part has already been downlaoded, or is blocked
								if((FileIter.uState & (Part::Available | Part::Cached | Part::Disabled)) != 0)
									continue; 

								// check if this part is marked for hoster download
								if((FileIter.uState & Part::Marked) != 0)
									continue; 

								CurVolume += FileIter.uEnd - FileIter.uBegin;
							}
						}

						if(CurVolume == 0)
							continue;
						if(!bPrem && !bFree)
						{
							bPrem = true;
							BestVolume = 0;
						}

						if(CurVolume > BestVolume)
						{
							BestVolume = CurVolume;
							pBestLink = pHosterLink;
						}
					}

					if(!pBestLink)
						break;
					ASSERT(BestVolume > 0);
					HosterLinks.removeOne(pBestLink);

					CShareMap* pParts = pBestLink->GetPartMap();
					CShareMap::SIterator SrcIter;
					while(pParts->IterateRanges(SrcIter))
					{
						if((SrcIter.uState & Part::Available) == 0 || (SrcIter.uState & Part::Verified) == 0) // if the reange is not available its irrelevant
							continue;
						ASSERT((SrcIter.uState & Part::Marked) == 0);

						CPartMap::SIterator FileIter(SrcIter.uBegin, SrcIter.uEnd);
						while(pFileParts->IterateRanges(FileIter))
						{
							// check if this part has already been downlaoded, or is blocked
							if((FileIter.uState & (Part::Available | Part::Cached | Part::Disabled)) != 0)
								continue; 

							pBestLink->ReserveRange(FileIter.uBegin, FileIter.uEnd, Part::Marked);
						}
					}

					if(pBestLink->StartDownload())
						ActiveCount++;
				}
				while(theCore->m_WebManager->GetDownloads() < theCore->Cfg()->GetInt("Hoster/MaxDownloads"));
			}

			if(ActiveCount == 0 && TransferringCount == 0 && (!pFileParts || (pFileParts->GetRange(0, -1) & Part::Available) == 0))
			{
				bool bPrem = false;
				CHosterLink* pBestLink = NULL;
				foreach(CHosterLink* pHosterLink, HosterLinksAux)
				{
					if(pHosterLink->HasError())
						continue;

					bool bFree = !theCore->m_LoginManager->HaveLogin(pHosterLink->GetHoster());
					if(bPrem && bFree)
						continue;

					if(!bPrem && !bFree)
					{
						bPrem = true;
						pBestLink = NULL;
					}

					if(!pBestLink)
						pBestLink = pHosterLink;
				}

				if(pBestLink)
				{
					if(pBestLink->StartDownload())
						ActiveCount++;
				}
			}
		}

		// Stats
		if(!HosterLinks.isEmpty() || !HosterLinksAux.isEmpty())
		{
			if(TransferringCount > 0)
				pFile->SetProperty("HosterStatus", "Loading");
			else if(uTimeToWait != -1)
				pFile->SetProperty("HosterStatus", QString("Waiting:%1").arg(uTimeToWait));
			else if(ActiveCount > 0)
				pFile->SetProperty("HosterStatus", "Active");
			else if(!HosterError.isEmpty())
				pFile->SetProperty("HosterStatus", QString("Error:%1").arg(HosterError));
			else if(uBlockedTime != -1)
				pFile->SetProperty("HosterStatus", QString("Waiting:%1").arg(uBlockedTime));
		}
		else if((Tick & EPer5Sec) != 0 && pFile->GetProperty("HosterStatus").isValid())
			pFile->SetProperty("HosterStatus", QVariant());
		// /Stats
#endif
	}
}
