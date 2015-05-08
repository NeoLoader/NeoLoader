#include "GlobalHeader.h"
#include "HashInspector.h"
#include "Transfer.h"
#include "../NeoCore.h"
#include "../FileList/Hashing/UntrustedFileHash.h"
#include "../FileList/Hashing/FileHashTree.h"
#include "../FileList/IOManager.h"
#include "./ed2kMule/MuleManager.h"
#include "CorruptionLogger.h"
#include "PartDownloader.h"
#include "../FileList/Hashing/HashingJobs.h"

CHashInspector::CHashInspector(QObject* qObject)
 : QObjectEx(qObject)
{
	m_IndexSource = HashNone;

	m_LastHashStart = 0;
	m_LastAvailable = 0;

	m_Update = false;
}

void CHashInspector::Process(UINT Tick)
{
	CFile* pFile = GetFile();
	CPartMapPtr pParts = pFile->GetPartMapPtr();
	if(!pParts)
		return;

	if(!pFile->IsMultiFile())
	{
		uint64 Available = pFile->GetStatusStats(Part::Available);
		uint64 NewAvailable = Available - m_LastAvailable;

		if(m_HashingJobs.isEmpty() 
			&& (Available == pFile->GetFileSize()
		 || NewAvailable > theCore->Cfg()->GetInt("Content/VerifySize") 
		 || (GetCurTick() - m_LastHashStart > SEC2MS(theCore->Cfg()->GetInt("Content/VerifyTime")) && NewAvailable > 0)
		))
		{
			if(StartValidation())
			{
				m_LastHashStart = GetCurTick();
				m_LastAvailable = pFile->GetStatusStats(Part::Available);
			}
		}
	}
	else if(CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(pParts.data()))
	{
		if(pFile->GetStatusStats(Part::Verified) == pFile->GetFileSize())
		{
			// check if all sub files are completed
			CFileList* pList = pFile->GetList();
			foreach(SPartMapLink* pLink, pJoinedParts->GetLinks())
			{
				CFile* pSubFile = pFile->GetList()->GetFileByID(pLink->ID);
				ASSERT(pSubFile); // we shouldnt arive at this point if the multi file is missing files
				if(!pSubFile || pSubFile->HasError())
				{
					pFile->SetError(tr("One or more sub files havean error"));
					return;
				}

				// we must wait for all files to offitially complete
				if(!pSubFile->IsComplete() || pSubFile->IsHashing())
					return;
			}

			QTimer::singleShot(0, pFile, SLOT(OnCompleteFile())); // OnCompleteFile() kills the inspector
			return;
		}
	}

	if(!m_Update)
		return;
	m_Update = false;

	AddHashesToFile();
}

bool CHashInspector::StartValidation(bool bRecovery)
{
	CFile* pFile = GetFile();
	CPartMapPtr pParts = pFile->GetPartMapPtr();

	// Master Hash instance of the current file - may be a parent hash refrence
	CFileHashPtr pFileHash = pFile->GetMasterHash();
	if(CFileHashPtr pFileHashEx = pFile->GetHashPtrEx(pFileHash->GetType(), pFileHash->GetHash()))
		pFileHash = pFileHashEx;
	if(!pFileHash)
		return false;

	// Actual Master Hash of type CFileHashEx
	uint64 ParentID = 0;
	CPartMapPtr pForignParts;
	uint64 Offset = 0;
	CFileHashPtr pMasterHash;
	CFileHashEx* pMasterHashEx = qobject_cast<CFileHashEx*>(pFileHash.data());
	if(pMasterHashEx)
		pMasterHash = pFileHash;
	else if(CSharedPartMap* pSharedParts = qobject_cast<CSharedPartMap*>(pParts.data()))
	{
		CFileList* pList = pFile->GetList();
		foreach(SPartMapLink* pLink, pSharedParts->GetLinks())
		{
			CFile* pParentFile = pList->GetFileByID(pLink->ID);
			if(!pParentFile)
				continue;

			if(CFileHashPtr pHash = pParentFile->GetHashPtrEx(pFileHash->GetType(), pFileHash->GetHash()))
			{
				pMasterHashEx = qobject_cast<CFileHashEx*>(pHash.data());
				if(pMasterHashEx)
				{
					pMasterHash = pHash;
					Offset = pLink->uShareBegin;

					ParentID = pLink->ID;
					pForignParts = pParentFile->GetPartMapPtr();
					break;
				}
			}
		}
	}

	bool bForceFinal = false;
	if(pMasterHashEx && !pMasterHashEx->CanHashParts())
	{
		if(pFile->GetStatusStats(Part::Available) < pFile->GetFileSize() || bRecovery) 
			return false;

		// file is complete but we dont have a working master hash, so we pick one
		EFileHashType HashPrio[3] = {HashNeo,HashXNeo,HashEd2k};
		for(int i=0; i<ARRSIZE(HashPrio); i++)
		{
			pFileHash = pFile->GetHashPtr(HashPrio[i]);
			if(CFileHashEx* pHash = qobject_cast<CFileHashEx*>(pFileHash.data()))
			{
				if(pHash->CanHashParts())
				{
					LogLine(LOG_INFO, tr("Auto switching master hash of file %1 from %2 to %3")
						.arg(pFile->GetFileName()).arg(CFileHash::HashType2Str(pHash->GetType())).arg(CFileHash::HashType2Str(pHash->GetType())));
					pFile->SetMasterHash(pFileHash);

					pMasterHashEx = pHash;
					pMasterHash = pFileHash;
					Offset = 0;

					bForceFinal = true;
					break;
				}
			}
		}
	}

	if(!pMasterHashEx)
	{
		if(pFileHash && (pParts->GetRange(0, -1) & Part::Available) != 0)
		{
			LogLine(LOG_INFO, tr("Completing file %1 without hashing").arg(pFile->GetFileName()));
			QTimer::singleShot(0, pFile, SLOT(OnCompleteFile())); // OnCompleteFile() kills the inspector
		}
		//pFile->SetError("NoValid MasterHash");
		return false;
	}

	if(!bRecovery && (bForceFinal || pFile->GetStatusStats(Part::Verified) == pFile->GetFileSize()))
	{
		LogLine(LOG_SUCCESS, tr("File %1 as been downloaded, starting final verification").arg(pFile->GetFileName()));

		CHashingJobPtr pHashingJob;
		if(ParentID)
			pHashingJob = CHashingJobPtr(new CVerifyPartsJob(ParentID, pMasterHash, pForignParts, Offset, pFile->GetFileSize(), eVerifyFile));
		else
			pHashingJob = CHashingJobPtr(new CVerifyPartsJob(pFile->GetFileID(), pMasterHash, pParts, 0, -1, eVerifyFile));
		connect(pHashingJob.data(), SIGNAL(Finished()), this, SLOT(OnFileVerified()));
		pFile->ProtectIO(); // set file read only for final verification
		m_HashingJobs.insert(pHashingJob.data(), pHashingJob);
		theCore->m_Hashing->AddHashingJob(pHashingJob);
	}
	else
	{
		if(ParentID)
		{
			CHashingJobPtr pHashingJob = CHashingJobPtr(new CVerifyPartsJob(ParentID, pMasterHash, pForignParts, Offset, pFile->GetFileSize()));
			if(bRecovery)
				connect(pHashingJob.data(), SIGNAL(Finished()), this, SLOT(OnPartsRecovered()));
			else
				connect(pHashingJob.data(), SIGNAL(Finished()), this, SLOT(OnPartsVerified()));
			m_HashingJobs.insert(pHashingJob.data(), pHashingJob);
			theCore->m_Hashing->AddHashingJob(pHashingJob);
		}

		CHashingJobPtr pHashingJob = CHashingJobPtr(new CVerifyPartsJob(pFile->GetFileID(), pFile->GetListForHashing(), pParts));
		if(ParentID) // we dont care to much for the results of other hashes, but we need to clear the job anyways
			connect(pHashingJob.data(), SIGNAL(Finished()), this, SLOT(OnVerifiedAux()));
		else if(bRecovery)
			connect(pHashingJob.data(), SIGNAL(Finished()), this, SLOT(OnPartsRecovered()));
		else
			connect(pHashingJob.data(), SIGNAL(Finished()), this, SLOT(OnPartsVerified()));
		m_HashingJobs.insert(pHashingJob.data(), pHashingJob);
		theCore->m_Hashing->AddHashingJob(pHashingJob);
	}

	return true;
}

void CHashInspector::AddHashesToFile()
{
	foreach(CFileHashPtr pHash, m_AuxHashMap)
	{
		// Note: HashTorrent as well as HashArchive are not unique and those are always added
		AddHashesToFile(pHash);
	}
}

bool CHashInspector::AddHashesToFile(CFileHashPtr pHash, bool bForce)
{
	CFile* pFile = GetFile();
	if(pHash->GetType() == HashTorrent)
	{
		if(pFile->IsTorrent() != 1 && !bForce)
			return false;
		
		if(!pFile->GetTorrents().contains(pHash->GetHash()))
			pFile->AddHash(pHash);
	}
#ifndef NO_HOSTERS
	else if(pHash->GetType() == HashArchive)
	{
		if(!pFile->GetArchives().contains(pHash->GetHash()))
			pFile->AddHash(pHash);
	}
#endif
	else if(!pFile->GetHash(pHash->GetType())) // this hashes are unique
		pFile->AddHash(pHash);
	else
		return false;
	m_AuxHashMap.remove(pHash->GetType(), pHash);
	return true;
}

CFileHashPtr FindHashInMapEx(EFileHashType Type, const QByteArray& Hash, QMultiMap<EFileHashType,CFileHashPtr>& Map)
{
	foreach(CFileHashPtr pFoundHash, Map.values(Type))
	{
		if(pFoundHash->Compare((byte*)Hash.data()))
			return pFoundHash;
	}
	return CFileHashPtr();
}

bool FindHashInMap(CFileHashPtr pFileHash, QMultiMap<EFileHashType,CFileHashPtr>& Map)
{
	foreach(CFileHashPtr pFoundHash, Map.values(pFileHash->GetType()))
	{
		if(pFoundHash->Compare(pFileHash.data()))
			return true;
	}
	return false;
}

void CHashInspector::SelectHash(EFileHashType Type, const QByteArray& Hash)
{
	if(CFileHashPtr pFileHash = FindHashInMapEx(Type, Hash, m_AuxHashMap))
		AddHashesToFile(pFileHash, true);
}

void CHashInspector::UnSelectHash(EFileHashType Type, const QByteArray& Hash)
{
	CFile* pFile = GetFile();
	if(CFileHashPtr pFileHash = pFile->GetHashPtrEx(Type, Hash))
	{
		pFile->DelHash(pFileHash);

		if(!FindHashInMap(pFileHash, m_AuxHashMap))
			m_AuxHashMap.insert(pFileHash->GetType(), pFileHash);
	}
}

void CHashInspector::BanHash(EFileHashType Type, const QByteArray& Hash)
{
	CFile* pFile = GetFile();
	if(CFileHashPtr pFileHash = pFile->GetHashPtrEx(Type, Hash))
		BlackListHash(pFileHash);
	else if(CFileHashPtr pFileHash = FindHashInMapEx(Type, Hash, m_AuxHashMap))
	{
		if(!FindHashInMap(pFileHash, m_BlackListMap))
			m_BlackListMap.insert(pFileHash->GetType(), pFileHash);

		m_AuxHashMap.remove(pFileHash->GetType(), pFileHash);
	}
}

void CHashInspector::UnBanHash(EFileHashType Type, const QByteArray& Hash)
{
	if(CFileHashPtr pFileHash = FindHashInMapEx(Type, Hash, m_BlackListMap))
	{
		if(!FindHashInMap(pFileHash, m_AuxHashMap))
			m_AuxHashMap.insert(pFileHash->GetType(), pFileHash);

		m_BlackListMap.remove(pFileHash->GetType(), pFileHash);
	}
}

void CHashInspector::OnPartsVerified()
{
	CHashingJob* pHashingJob = (CHashingJob*)sender();
	m_HashingJobs.remove(pHashingJob);

	ValidateParts();
}

void CHashInspector::OnVerifiedAux()
{
	CHashingJob* pHashingJob = (CHashingJob*)sender();
	m_HashingJobs.remove(pHashingJob);
}

void CHashInspector::OnPartsRecovered()
{
	CHashingJob* pHashingJob = (CHashingJob*)sender();
	m_HashingJobs.remove(pHashingJob);

	ValidateParts(true);
}

void CHashInspector::OnFileVerified()
{
	CHashingJob* pHashingJob = (CHashingJob*)sender();
	m_HashingJobs.remove(pHashingJob);

	ValidateParts();

	CFile* pFile = GetFile();
	CPartMap* pParts = pFile->GetPartMap();
	ASSERT(pParts);

	if((pParts->GetRange(0, -1) & Part::Verified) == 0)
	{
		pFile->ProtectIO(false); // something is wrong make file writable again
		if((pParts->GetRange(0, -1) & Part::Available) != 0)
			LogLine(LOG_SUCCESS, tr("Downlaoded File %1 failed verification, redownlaoding corrupt ranges").arg(pFile->GetFileName()));
	}
	else if(!pFile->IsHashing())
		QTimer::singleShot(0, pFile, SLOT(OnCompleteFile())); // OnCompleteFile() kills the inspector
}

void CHashInspector::ValidateParts(bool bRecovery)
{	
	// evaluate corruption log
	if(!bRecovery)
	{
		foreach(CCorruptionLogger* pLogger, m_Loggers)
			pLogger->Evaluate(); // this puls all hashing related infos in automatically
	}

	CFile* pFile = GetFile();
	CPartMap* pParts = pFile->GetPartMap();
	ASSERT(pParts);
	uint64 uFileSize = pFile->GetFileSize();

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Prepare master hash and optional parent offset
	//
	//	We set all file payload related flags always in the single/shared part maps,
	//		this means that if we are her in a multi file we must chose what sub files are affected by the result
	//		and call ValidateParts on thair particular inspectory
	//
	//		If we are a sub file we must resolve the parent hash our proper start offset and inspect the range that is relevant for us
	//

	CFileHashPtr pHash = pFile->GetMasterHash();
	if(!pHash)
	{
		ASSERT(0);
		return;
	}

	if(CFileHashPtr pHashEx = pFile->GetHashPtrEx(pHash->GetType(), pHash->GetHash()))
		pHash = pHashEx;

	uint64 Offset = 0;
	CFileHashEx* pMasterHash = qobject_cast<CFileHashEx*>(pFile->GetHashPtrEx(pHash->GetType(), pHash->GetHash()).data());

	QMap<CFile*, QPair<CFileHashPtr, uint64> > ParentFiles; // This lists all parent file master hashes
	CSharedPartMap* pSharedParts = qobject_cast<CSharedPartMap*>(pParts);

	if(pSharedParts)
	{
		CFileList* pList = pFile->GetList();
		foreach(SPartMapLink* pLink, pSharedParts->GetLinks())
		{
			CFile* pParentFile = pList->GetFileByID(pLink->ID);
			if(!pParentFile)
				continue;

			if(!pMasterHash) // Note: the forign master must not nececerly be the masterhash of a parrent
			{
				if(CFileHashPtr pFileHash = pParentFile->GetHashPtrEx(pHash->GetType(), pHash->GetHash()))
				{
					Offset = pLink->uShareBegin;
					pMasterHash = qobject_cast<CFileHashEx*>(pFileHash.data());
				}
			}

			if(CFileHashPtr pFileHash = pParentFile->GetMasterHash())
				ParentFiles[pParentFile] = QPair<CFileHashPtr, uint64>(pFileHash, pLink->uShareBegin);
		}
	}

	CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(pParts);
	
	if(!pMasterHash) // this should have been handled by the scheduling function
	{
		ASSERT(0);
		return;
	}

	// Look for local hash conflicts
	foreach(CFileHashPtr pHash, pFile->GetAllHashes())
	{
		CFileHashEx* pFileHash = qobject_cast<CFileHashEx*>(pHash.data());
		if(!pFileHash || pFileHash == pMasterHash)
			continue;

		if(FindHashConflicts(uFileSize, pMasterHash, Offset, pFileHash))
			BlackListHash(pHash);
	}

	// look for not maching parent files
	foreach(CFile* pParentFile, ParentFiles.keys())
	{
		QPair<CFileHashPtr, uint64> Pair = ParentFiles[pParentFile];

		CFileHashEx* pFileHash = qobject_cast<CFileHashEx*>(Pair.first.data());
		if(!pFileHash || pFileHash == pMasterHash)
			continue;

		if(FindHashConflicts(uFileSize, pMasterHash, Offset, pFileHash, Pair.second))
		{
			pParentFile->SetError("Sub File Invalid");

			pSharedParts->BreakLink(pParentFile->GetFileID());
			if(CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(pParentFile->GetPartMap()))
				pJoinedParts->BreakLink(pParentFile->GetFileID());
		}
	}

	bool bNotify = bRecovery;

	if(!pJoinedParts)
	{
		// update the range maps
		QBitArray StatusMap = pMasterHash->GetStatusMap();
		QList<TPair64> CorruptionSet = pMasterHash->GetCorruptionSet();

		CPartMap::SIterator FileIter;
		while(pParts->IterateRanges(FileIter, Part::Available | Part::Corrupt | Part::Verified))
		{
			if(!bRecovery && !((FileIter.uState & (Part::Available | Part::Corrupt)) != 0 && (FileIter.uState & Part::Verified) == 0))
				continue; // this range is not interesting, its eider already verifyed or empty

			// Note: The Part::Verified must always be set by the sub file in its own map,
			//		Therefor the Part validation must always be delegated to the sub file
			bool bUpdateSub = false;

			// Note: we must proeprly take the parent file offset into account !!!
			QPair<uint32, uint32> Range = pMasterHash->IndexRange(FileIter.uBegin + Offset, FileIter.uEnd + Offset);
			for(int Index = Range.first; Index < Range.second; Index ++)
			{
				uint64 uCurBegin = pMasterHash->IndexOffset(Index);
				uint64 uBegin = uCurBegin;
				if(uBegin < Offset)	uBegin = 0;
				else if(Offset)		uBegin -= Offset;

				uint64 uCurEnd = pMasterHash->IndexOffset(Index + 1);
				uint64 uEnd = uCurEnd;
				if(Offset)			uEnd -= Offset;
				if(uEnd > uFileSize)uEnd = uFileSize;

				// do not recheck this range
				FileIter.uBegin = uBegin;
				FileIter.uEnd = uEnd;

				if(StatusMap.testBit(Index)) // verifyed
				{
					// if this part was thought to be not available and scheduled, this happens only on file recovery
					if((pParts->GetRange(uBegin, uEnd, CPartMap::eUnion).uStates & Part::Scheduled) != 0)
						pFile->CancelRequest(uBegin, uEnd);

					// mark part as verifyed
					pParts->SetRange(uBegin, uEnd, Part::Corrupt, CPartMap::eClr);
					if((pParts->GetRange(uBegin, uEnd).uStates & Part::Available) == 0)
					{
						ASSERT(bRecovery);
						pParts->SetRange(uBegin, uEnd, Part::Available, CPartMap::eAdd);
					}
					pParts->SetRange(uBegin, uEnd, Part::Verified, CPartMap::eAdd);
				}
				// Note: We must use here the right master hash offsets!!!!
				else if(testRange(CorruptionSet, qMakePair(uCurBegin, uCurEnd))) // corruped
				{
					if((pParts->GetRange(uBegin, uEnd, CPartMap::eUnion).uStates & Part::Available) == 0)
						continue; // ignore we already know that
				
					ASSERT(!bRecovery);
					LogLine(LOG_DEBUG, tr("Range %1 - %2 of file %3 is corrupted").arg(uBegin).arg(uEnd).arg(pFile->GetFileName()));

					// Clear the part, mark it as empty
					pParts->SetRange(uBegin, uEnd, Part::Available | Part::Verified, CPartMap::eClr);
					pParts->SetRange(uBegin, uEnd, Part::Corrupt, CPartMap::eAdd);
					bNotify = true;  // make sure we wil reschedule this part as we normaly ommit already complete parts

					// Reset all hashing results for this range
					ResetRange(uBegin, uEnd);
				}
				//else // incomplete
			}

			if((FileIter.uState & (Part::Available || Part::Corrupt)) != 0)
			{
				// Note: here we request on demand eMules AICH hash set for the affected range - single file only
				if(CFileHashTree* pAICHHash = qobject_cast<CFileHashTree*>(pFile->GetHash(HashMule)))
				{
					if(!pAICHHash->IsFullyResolved(FileIter.uBegin, FileIter.uEnd))
						theCore->m_MuleManager->RequestAICHData(pFile, FileIter.uBegin, FileIter.uEnd);
				}
			}
		}
	}

	// synchromize Downloaded Bytes 
	//pFile->m_DownloadedBytes = pFile->GetStatusStats(Part::Available);

	if(bNotify)
		pParts->NotifyChange(); // notify aboult a substantial change to the part map

	foreach(CFile* pParentFile, ParentFiles.keys())
	{
		CHashInspector* pInspector = pParentFile->GetInspector();
		pInspector->ValidateParts(bRecovery);
	}
}

void CHashInspector::ResetRange(uint64 uBegin, uint64 uEnd)
{
	CFile* pFile = GetFile();
	ResetRange(pFile, uBegin, uEnd);

	CPartMap* pParts = pFile->GetPartMap();
	if(CSharedPartMap* pSharedParts = qobject_cast<CSharedPartMap*>(pParts))
	{
		CFileList* pList = pFile->GetList();
		foreach(SPartMapLink* pLink, pSharedParts->GetLinks())
		{
			CFile* pParentFile = pList->GetFileByID(pLink->ID);
			if(!pParentFile)
				continue;

			uint64 uCurBegin = uBegin + pLink->uShareBegin;

			uint64 uCurEnd = uEnd + pLink->uShareBegin;
			ASSERT(uCurEnd <= pLink->uShareEnd);

			ResetRange(pParentFile, uCurBegin, uCurEnd);
		}
	}
	else if(CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(pParts))
	{
		CFileList* pList = pFile->GetList();
		foreach(SPartMapLink* pLink, pJoinedParts->GetLinks())
		{
			CFile* pSubFile = pList->GetFileByID(pLink->ID);
			if(!pSubFile)
				continue;

			ASSERT(pLink->uShareBegin < pLink->uShareEnd);
			if(pLink->uShareBegin < uEnd && pLink->uShareEnd > uBegin)
			{
				uint64 uCurBegin = (uBegin > pLink->uShareBegin) ? (uBegin - pLink->uShareBegin) : 0;
				uint64 uCurEnd = (uEnd > pLink->uShareEnd) ? pSubFile->GetFileSize() : (uEnd - pLink->uShareBegin);
				ASSERT(uCurEnd <= pSubFile->GetFileSize());

				ResetRange(pSubFile, uCurBegin, uCurEnd);
			}
		}
	}
}

void CHashInspector::ResetRange(CFile* pFile, uint64 uBegin, uint64 uEnd)
{
	foreach(CFileHashPtr pHash, pFile->GetAllHashes())
	{
		CFileHashEx* pFileHash = qobject_cast<CFileHashEx*>(pHash.data());
		if(!pFileHash)
			continue;

		pFileHash->ClearResult(uBegin, uEnd);
	}
}

void CHashInspector::OnRecoveryData()
{
	// Force Hashing
	m_LastHashStart = 0;
	m_LastAvailable = 0;
}

void CHashInspector::AddAuxHash(CFileHashPtr pFileHash)
{
	CFile* pFile = GetFile();
	if(!pFileHash->IsValid())
		return;

	if(FindHashInMap(pFileHash, m_BlackListMap) || pFile->CompareHash(pFileHash.data()))
		return;

	if(CSharedPartMap* pSharedParts = qobject_cast<CSharedPartMap*>(pFile->GetPartMap()))
	{
		CFileList* pList = pFile->GetList();
		foreach(SPartMapLink* pLink, pSharedParts->GetLinks())
		{
			CFile* pParentFile = pList->GetFileByID(pLink->ID);
			if(pParentFile && pParentFile->CompareHash(pFileHash.data()))
				return;
		}
	}

	m_Update = true;
	if(!FindHashInMap(pFileHash, m_AuxHashMap))
		m_AuxHashMap.insert(pFileHash->GetType(), pFileHash);
}

void CHashInspector::BlackListHash(CFileHashPtr pFileHash)
{
	CFile* pFile = GetFile();
	CFileHashPtr pMasterHash = pFile->GetMasterHash();
	if(pFileHash->Compare(pMasterHash.data()))
	{
		ASSERT(0);
		return; // never drop master
	}

	if(pFileHash->GetType() == HashXNeo && m_IndexSource == HashXNeo && (!pMasterHash.isNull() && pMasterHash->GetType() == HashTorrent))
		m_IndexSource = HashTorrent; // fall back to what we know for sure

	delete m_Loggers.take(pFileHash->GetHash()); // dint forget to remove the associated logger

	m_Update = true;
	if(!FindHashInMap(pFileHash, m_BlackListMap))
		m_BlackListMap.insert(pFileHash->GetType(), pFileHash);

	pFile->DelHash(pFileHash);
}

bool CHashInspector::BadMetaData(CFileHashPtr pFileHash)
{
	CFile* pFile = GetFile();
	CPartMap* pPartMap = pFile->GetPartMap();
	CFileHashPtr pMasterHash = pFile->GetMasterHash();
	if(pFileHash->Compare(pMasterHash.data())) // if the current hash is master drop the old data
	{
		LogLine(LOG_DEBUG | LOG_ERROR, tr("Replacing not suitable metaData for file %1").arg(pFile->GetFileName()));

		EFileHashType BadSource = pFile->GetInspector()->GetIndexSource();
		if(BadSource && BadSource != HashXNeo)
		{
			if(CFileHashPtr pBadHash = pFile->GetHashPtr(BadSource))
				pFile->GetInspector()->BlackListHash(pBadHash);
		}

		pFile->CloseIO(); // Close IO

		// Break links and cleanup subfiles
		if(CJoinedPartMap* pJoinedParts = qobject_cast<CJoinedPartMap*>(pPartMap))
		{
			uint64 ID = pFile->GetFileID();

			CFileList* pList = pFile->GetList();
			foreach(SPartMapLink* pLink, pJoinedParts->GetLinks())
			{
				pJoinedParts->BreakLink(pLink->ID);
				if(CLinkedPartMap* pParts = qobject_cast<CLinkedPartMap*>(pLink->pMap.data()))
					pParts->BreakLink(ID);

				CFile* pSubFile = pList->GetFileByID(pLink->ID); // remove a sub file that are incomplete, and dont have a master hash
				if(pSubFile && pSubFile->IsIncomplete() && !pSubFile->CompareHash(pSubFile->GetMasterHash().data()))
					pSubFile->Remove(true, false);
			}
		}
		return true;
	}
	
	LogLine(LOG_DEBUG | LOG_ERROR, tr("Resived not suitable metaData for file %1").arg(pFile->GetFileName()));
	pFile->GetInspector()->BlackListHash(pFileHash);
	return false;
}

void CHashInspector::AddUntrustedHash(CFileHashPtr pHash, const CAddress& Address)
{
	CFile* pFile = GetFile();
	if(pFile->GetHash(pHash->GetType()))
		return; // already found a good cadidate

	if(FindHashInMap(pHash, m_BlackListMap))
		return; // this hash is on the blacklist drop it

	CUntrustedFileHash* pUntrustedHash = qobject_cast<CUntrustedFileHash*>(m_UntrustedMap.value(pHash->GetType()).data());
	if(!pUntrustedHash)
	{
		pUntrustedHash = new CUntrustedFileHash(pHash->GetType());
		m_UntrustedMap.insert(pHash->GetType(), CFileHashPtr(pUntrustedHash));
	}
	pUntrustedHash->AddHash(pHash, Address);

	if(pUntrustedHash->SelectTrustedHash(theCore->Cfg()->GetInt("HashInspector/Majority"), theCore->Cfg()->GetInt("HashInspector/Quorum")))
	{
		CFileHashPtr pFullHash = CFileHashPtr(CFileHash::FromArray(pUntrustedHash->GetHash(), pHash->GetType(), pFile->GetFileSize()));
		pFile->AddHash(pFullHash);

		m_UntrustedMap.remove(pHash->GetType());
	}
}

CCorruptionLogger* CHashInspector::GetLogger(CFileHashPtr pHash)
{
	ASSERT(!pHash.isNull());
	CCorruptionLogger* pLogger = m_Loggers.value(pHash->GetHash());
	if(!pLogger)
	{
		ASSERT(!GetFile()->GetHashPtrEx(pHash->GetType(), pHash->GetHash()).isNull());

		pLogger = new CCorruptionLogger(pHash, GetFile()->GetFileSize(), this);
		m_Loggers.insert(pHash->GetHash(), pLogger);
	}
	return pLogger;

}

bool CHashInspector::AdjustIndexRange(uint64& uFrom, uint64& uTo, const uint64 uFileSize, const uint64 Offset, uint64 SubOffset)
{
	if(uFrom < Offset)	
		uFrom = 0;
	else if(Offset)		
		uFrom -= Offset;
	if(uFrom > uFileSize)	
		return false; // that part begins after this sub file
	uFrom += SubOffset;

	if(uTo < Offset)		
		return false; // that part ends befoure this sub file
	if(Offset)				
		uTo -= Offset;
	if(uTo > uFileSize)	
		uTo = uFileSize;
	uTo += SubOffset;

	ASSERT(uFrom < uTo);

	return true;
}

bool CHashInspector::FindHashConflicts(uint64 uFileSize, CFileHashEx* pMasterHash, uint64 Offset, CFileHashEx* pFileHash, uint64 SubOffset)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Check for conflicts
	//
	// Note: not all conflicts mean that there is somethign wrong with hash associations.
	// A hash conflict may be a result of partially overlaping parts.
	//
	// this one is OK (C is the real corruption c is the seen due to part resolution)
	// 0              9                 18                27                36             45
	// + vvvvv   vvvv + v   vvvvv   vvv + cc   cCCcc   cc + aaa   uuuuu   u + uuuu   uuuuu +
	// + vvvvv + vvvv   v + vvvvv + vvv   vv + cCCcc + aa   aaa + uuuuu + u   uuuu + uuuuu +
	// 0       5          10      15         20      25         30      35         40      45
	//
	// this one is NOT OK 
	// 0              9                 18                27                36             45
	// + vvvvv   vvvv + v   vvvvv   vvv + CC   ccccc   cc + aaa   uuuuu   u + uuuu   uuuuu +
	// + vvvvv + vvvv   v + vvvvv + vvv   vv + vvvvv + vv   vvv + uuuuu + u   uuuu + uuuuu +
	// 0       5          10      15         20      25         30      35         40      45
	//
	// We check for a corrupted part that is fully emerged in a range considdered as valid.
	// Than we can call a confirmed conflict situation.
	//

	QBitArray StatusMap = pMasterHash->GetStatusMap();
	QList<TPair64> CorruptionSet = pMasterHash->GetCorruptionSet();

	QBitArray AuxStatusMap = pFileHash->GetStatusMap();
	QList<TPair64> AuxCorruptionSet = pFileHash->GetCorruptionSet();

	// Check Hash -> Master
	foreach(const TPair64 &Corruption, AuxCorruptionSet)
	{
		uint64 uFrom = Corruption.first;
		uint64 uTo = Corruption.second;
		if(!AdjustIndexRange(uFrom, uTo, uFileSize, SubOffset, Offset))
			continue;

		// Get the indexes of parts in master that overlap fully pr partialy with the currupted part
		QPair<uint32, uint32> Range = pMasterHash->IndexRange(uFrom, uTo);
		if(testBits(StatusMap, Range)) // test if all bits are set to true
			return true;
	}

	// Check Master -> Hash
	foreach(const TPair64 &Corruption, CorruptionSet)
	{
		uint64 uFrom = Corruption.first;
		uint64 uTo = Corruption.second;
		if(!AdjustIndexRange(uFrom, uTo, uFileSize, Offset, SubOffset))
			continue;

		// Get the indexes of parts in master that overlap fully pr partialy with the currupted part
		QPair<uint32, uint32> Range = pFileHash->IndexRange(uFrom, uTo);
		if(testBits(AuxStatusMap, Range)) // test if all bits are set to true
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Load/Store
//

QVariantMap CHashInspector::Store()
{
	CFile* pFile = GetFile();

	QVariantMap Data;
	if(m_IndexSource != HashNone)
		Data["IndexSource"] = CFileHash::HashType2Str(m_IndexSource);

	QVariantList HashMap;
	foreach(CFileHashPtr pHash, pFile->GetAllHashes())
	{
		CFileHashEx* pFileHash = qobject_cast<CFileHashEx*>(pHash.data());
		if(!pFileHash)
			continue;

		QVariantMap HashEntry;
		HashEntry["Type"] = CFileHash::HashType2Str(pFileHash->GetType());
		HashEntry["Value"] = QString(pFileHash->ToString());

		HashEntry["Verifyed"] = QString(CShareMap::Bits2Bytes(pFileHash->GetStatusMap()).toHex());
		/*QStringList Corrupted;
		foreach(uint32 Index, pFileHash->GetCorruptionSet())
			Corrupted.append(QString::number(Index));
		HashEntry["Corrupted"] = Corrupted.join(";");*/

		HashMap.append(HashEntry);
	}
	Data["HashMap"] = HashMap;

	return Data;
}

void CHashInspector::Load(const QVariantMap& Data)
{
	CFile* pFile = GetFile();

	m_IndexSource = CFileHash::Str2HashType(Data["IndexSource"].toString());

	foreach(const QVariant& vHashEntry, Data["HashMap"].toList())
	{
		QVariantMap HashEntry = vHashEntry.toMap();

		EFileHashType Type= CFileHash::Str2HashType(HashEntry["Type"].toString());
		QByteArray HashValue = CFileHash::DecodeHash(Type, HashEntry["Value"].toString().toLatin1());
		CFileHashPtr pHash = pFile->GetHashPtrEx(Type, HashEntry["Value"].toByteArray());
		CFileHashEx* pFileHash = qobject_cast<CFileHashEx*>(pHash.data());
		if(!pFileHash)
			continue;

		QBitArray StatusMap = CShareMap::Bytes2Bits(QByteArray::fromHex(HashEntry["Verifyed"].toString().toLatin1()), pFileHash->GetStatusCount());
		/*QSet<uint32> CorruptionSet;
		foreach(const QString& sIndex, HashEntry["Corrupted"].toStringList())
			CorruptionSet.insert(sIndex.toULong());*/
		pFileHash->SetStatus(StatusMap/*, CorruptionSet*/);
	}
}


