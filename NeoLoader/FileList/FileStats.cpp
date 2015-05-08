#include "GlobalHeader.h"
#include "FileStats.h"
#include "../NeoCore.h"
#include "./Hashing/FileHashTree.h"
#include "./Hashing/FileHashSet.h"
#include "../FileTransfer/Transfer.h"
#ifndef NO_HOSTERS
#include "../FileTransfer/HosterTransfer/ArchiveDownloader.h"
#include "../FileTransfer/HosterTransfer/HosterLink.h"
#include "../FileTransfer/HosterTransfer/ArchiveSet.h"
#endif
#include "../../Framework/Maths.h"

CFileStats::CFileStats(CFile* pFile)
: QObjectEx(pFile) 
{
}

//void CFileStats::Process(UINT Tick)
//{
//}

void CFileStats::SetupAvailMap()
{
	uint64 FileSize = GetFile()->GetFileSize();
	if(FileSize && (!m_Availability || m_Availability->GetSize() != FileSize))
	{
		m_Availability = CAvailMapPtr(new CAvailMap(FileSize));
#ifndef NO_HOSTERS
		m_HosterCache = CCacheMapPtr(new CCacheMap(FileSize));
		m_HosterMap = CHosterMapPtr(new CHosterMap(FileSize));
#endif
	}
}

#ifndef NO_HOSTERS
bool UpdateCahce(CTransfer* pTransfer)
{
	// X-ToDo-Now:  handle teh case where the feature got dissabled or changed
	if(CP2PTransfer* pP2PTransfer = qobject_cast<CP2PTransfer*>(pTransfer))
	{
		if(pP2PTransfer->SupportsHostCache() && pTransfer->GetFile() && pTransfer->GetFile()->IsHosterUl())
			return true;
	}
	return false;
}

void CFileStats::AddRange(uint64 uBegin, uint64 uEnd, bool bTest, bool bUpdateHosted, bool bUpdateCache, CHosterLink* pHosterLink)
{
	if(bTest)
	{
		if(bUpdateHosted)
		{
			m_HosterMap->SetRange(uBegin, uEnd, CHosterMap::MkValue(pHosterLink->GetHoster(), pHosterLink->GetUploadAcc()), CHosterMap::eAdd);
			m_HosterCache->SetRange(uBegin, uEnd, SCacheRange(1,0), CCacheMap::eAdd);
		}
	}
	else if(bUpdateCache)
		m_HosterCache->SetRange(uBegin, uEnd, SCacheRange(0,1), CCacheMap::eAdd);
}

void CFileStats::DelRange(uint64 uBegin, uint64 uEnd, bool bTest, bool bUpdateHosted, bool bUpdateCache, CHosterLink* pHosterLink)
{
	if(bTest)
	{
		if(bUpdateHosted)
		{
			m_HosterMap->SetRange(uBegin, uEnd, CHosterMap::MkValue(pHosterLink->GetHoster(), pHosterLink->GetUploadAcc()), CHosterMap::eClr);
			m_HosterCache->SetRange(uBegin, uEnd, SCacheRange(1,0), CCacheMap::eClr);
		}
	}
	else if(bUpdateCache)
		m_HosterCache->SetRange(uBegin, uEnd, SCacheRange(0,1), CCacheMap::eClr);
}
#endif

void CFileStats::AddTransfer(CTransfer* pTransfer)
{
#ifndef NO_HOSTERS
	CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer);
#endif
	if (CShareMap* pParts = pTransfer->GetPartMap())
	{
		if (m_Availability)
		{
#ifndef NO_HOSTERS
			bool bUpdateCache = UpdateCahce(pTransfer);
			bool bUpdateHosted = pHosterLink != NULL && !pTransfer->HasError();
#endif

			CShareMap::SIterator DiffIter;
			while (pParts->IterateRanges(DiffIter))
			{
				if ((DiffIter.uState & Part::Available) != 0)
					m_Availability->SetRange(DiffIter.uBegin, DiffIter.uEnd, 1, CAvailMap::eAdd);
#ifndef NO_HOSTERS
				AddRange(DiffIter.uBegin, DiffIter.uEnd, (DiffIter.uState & Part::Available) != 0, bUpdateHosted, bUpdateCache, pHosterLink);
#endif
			}
		}
	}

#ifndef NO_HOSTERS
	if (pHosterLink)
	{
		m_HosterCount[pHosterLink->GetHoster()]++;
		m_HosterShare.clear();
	}
#endif

	ETransferType eType = pTransfer->GetType();
	m_CountMap[eType]++;
}

void CFileStats::RemoveTransfer(CTransfer* pTransfer)
{
#ifndef NO_HOSTERS
	CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer);
#endif
	if (CShareMap* pParts = pTransfer->GetPartMap())
	{
		if (m_Availability)
		{
#ifndef NO_HOSTERS
			bool bUpdateCache = UpdateCahce(pTransfer);
			bool bUpdateHosted = pHosterLink != NULL && !pTransfer->HasError();
#endif

			CShareMap::SIterator DiffIter;
			while (pParts->IterateRanges(DiffIter))
			{
				if ((DiffIter.uState & Part::Available) != 0)
					m_Availability->SetRange(DiffIter.uBegin, DiffIter.uEnd, 1, CAvailMap::eClr);
#ifndef NO_HOSTERS
				DelRange(DiffIter.uBegin, DiffIter.uEnd, (DiffIter.uState & Part::Available) != 0, bUpdateHosted, bUpdateCache, pHosterLink);
#endif
			}
		}
	}

#ifndef NO_HOSTERS
	if (pHosterLink)
	{
		int &Count = m_HosterCount[pHosterLink->GetHoster()];
		if (--Count <= 0)
			m_HosterCount.remove(pHosterLink->GetHoster());
		m_HosterShare.clear();
	}
#endif

	ETransferType eType = pTransfer->GetType();
	m_CountMap[eType]--;
}

void CFileStats::UpdateAvail(CTransfer* pTransfer, const CAvailDiff& AvailDiff, bool b1st)
{
#ifndef NO_HOSTERS
	bool bUpdateCache = UpdateCahce(pTransfer);
	CHosterLink* pHosterLink = qobject_cast<CHosterLink*>(pTransfer);
	bool bUpdateHosted = pHosterLink != NULL;
#endif

	for(size_t i=0; i < AvailDiff.Count(); i++)
	{
		const SAvailDiff& Diff = AvailDiff.At(i);

		if((Diff.uNew & Part::Available) != 0)
			m_Availability->SetRange(Diff.uBegin, Diff.uEnd, 1, CAvailMap::eAdd);

#ifndef NO_HOSTERS
		AddRange(Diff.uBegin, Diff.uEnd, (Diff.uNew & Part::Available) != 0, bUpdateHosted, bUpdateCache, pHosterLink);
#endif
		if(!b1st) // if this is not the first time we have a part mal clear the old state
		{
			if((Diff.uOld & Part::Available) != 0)
				m_Availability->SetRange(Diff.uBegin, Diff.uEnd, 1, CAvailMap::eClr);

#ifndef NO_HOSTERS
			DelRange(Diff.uBegin, Diff.uEnd, (Diff.uOld & Part::Available) != 0, bUpdateHosted, bUpdateCache, pHosterLink);
#endif
		}
	}

#ifndef NO_HOSTERS
	if(pHosterLink)
		m_HosterShare.clear();
#endif
}

double CFileStats::GetAvailStats()
{
	CFile* pFile = GetFile();
	double Sources = 0.0;

#ifndef NO_HOSTERS
	if(!pFile->GetArchives().isEmpty())
		Sources += CArchiveDownloader::GetEncAvail(pFile); // integer
#endif

	Sources += GetAvailStatsRaw();

	if(pFile->IsComplete())
		Sources += 1.0;

	return Sources;
}

double CFileStats::GetAvailStatsRaw(bool bUpdate)
{
	if(!m_Availability)
		return 0;

	if(bUpdate || (m_AvailStat.uRevision != m_Availability->GetRevision() && m_AvailStat.uInvalidate < GetCurTick()))
	{
		UINT Availability = -1;

		QMap<UINT, uint64> Availabilities;

		CAvailMap::SIterator AvailIter;
		while(m_Availability->IterateRanges(AvailIter))
		{
			Availabilities[AvailIter.uState] += AvailIter.uEnd - AvailIter.uBegin;

			if(Availability == -1 || Availability > AvailIter.uState)
				Availability = AvailIter.uState;
		}

		Availabilities.remove(Availability);

		uint64 uSumm = 0;
		for(QMap<UINT, uint64>::iterator I = Availabilities.begin(); I != Availabilities.end(); I++)
			uSumm += I.value();

		m_AvailStat.Availability = Availability + ((double)uSumm / m_Availability->GetSize());
		if(m_AvailStat.Availability > 0.999 && m_AvailStat.Availability < 1)
			m_AvailStat.Availability = 0.999;

		m_AvailStat.uRevision = m_Availability->GetRevision();
		m_AvailStat.uInvalidate = GetCurTick() + SEC2MS(3);
	}

	return m_AvailStat.Availability;
}

int CFileStats::GetTransferCount(ETransferType eType)
{
	if(eType == eTypeUnknown)
	{
		int TotalCount = 0;
		foreach(int Count, m_CountMap)
			TotalCount += Count;
		return TotalCount;
	}
	return m_CountMap.value(eType);
}

CFileStats::STemp CFileStats::GetTempCount(ETransferType eType)
{
	if(eType == eTypeUnknown)
	{
		STemp TotalCount = {0,0};
		foreach(const STemp& Count, m_TempMap)
		{
			TotalCount.Checked += Count.Checked;
			TotalCount.Connected += Count.Connected;
			TotalCount.Complete += Count.Complete;
			TotalCount.All += Count.All;
		}
		return TotalCount;
	}
	return m_TempMap.value(eType);
}

#ifndef NO_HOSTERS
bool CFileStats::HasHosters()
{
	if(!m_HosterMap)
		return false;

	CHosterMap::SIterator Iterator;
	while(m_HosterMap->IterateRanges(Iterator))
	{
		if(Iterator.uState.count() > 0)
			return true;
	}
	return false;
}

QStringList CFileStats::GetHosterList()
{
	return m_HosterCount.keys();
}

// Keys:
//=====================
// Any/All				- total stare ratio on all hosters
// Any/Me				- my share ratio on all hosters
// [Hoster]/All			- total share ratio on this hoster
// [Hoster]/Me			- my total ratio on this hoster
// [Hoster]/[User]		- my account share ratio on hoster for given account
// [Hoster]/Anonymouse	- my account less share ratio on hoster

QMap<QString, QMap<QString, double> > CFileStats::GetHostingInfo()
{
	if(m_HosterShare.isEmpty())
	{
		if(m_HosterMap)
		{
			QMap<QString, QMap<int, uint64> > TempMap;
			CHosterMap::SIterator Iterator;
			while(m_HosterMap->IterateRanges(Iterator))
			{
				uint64 uLength = Iterator.uEnd - Iterator.uBegin;
				int Count = 0;
				int MyCount = 0;
				for(QMap<QString, SHosterRange>::iterator I = Iterator.uState.begin(); I != Iterator.uState.end(); I++)
				{
					int MyTemp = 0;
					for(QMap<QString, int>::iterator K = I.value().MyCounts.begin(); K != I.value().MyCounts.end(); K++)
					{
						MyTemp += K.value();
						TempMap[I.key() + "/" + K.key()][K.value()] += uLength;
					}
					MyCount += MyTemp;
					int Temp = I.value().PubCount + MyTemp;
					Count += Temp;

					TempMap[I.key() + "/Me"][MyTemp] += uLength;
					TempMap[I.key() + "/All"][Temp] += uLength;
				}
				TempMap["Any/Me"][MyCount] += uLength;
				TempMap["Any/All"][Count] += uLength;
			}

			for(QMap<QString, QMap<int, uint64> >::iterator I = TempMap.begin(); I != TempMap.end(); I++)
			{
				double Share = 0.0;
				QMap<int, uint64>::iterator J = I.value().begin();
				int Limit = J.key() + 1;
				for(; J != I.value().end(); J++)	
				{
					if (J.key() > Limit)
						Share += J.value() * Limit;
					else
						Share += J.value() * J.key();
				}

				StrPair DomainAcc = Split2(I.key(), "/");
				m_HosterShare[DomainAcc.first][DomainAcc.second] = Share / m_HosterMap->GetSize();
			}
		}
	}

	return m_HosterShare;
}
#endif
