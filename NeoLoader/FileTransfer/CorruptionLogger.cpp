#include "GlobalHeader.h"
#include "CorruptionLogger.h"
#include "Transfer.h"
#include "../NeoCore.h"
#include "../../Framework/Cryptography/AbstractKey.h"

CCorruptionLogger::CCorruptionLogger(CFileHashPtr pHash, uint64 uFileSize, QObject* qObject)
 : QObjectEx(qObject), m_Records(uFileSize)
{
	m_pHash = pHash;
}

void CCorruptionLogger::Record(uint64 uBegin, uint64 uEnd, const QPair<const byte*, size_t>& ID)
{
	m_Records.SetRange(uBegin, uEnd, SRecord(QByteArray((const char*)ID.first, ID.second)));
}

void CCorruptionLogger::Evaluate()
{
	CFileHashEx* pHash = qobject_cast<CFileHashEx*>(m_pHash.data());
	ASSERT(pHash);
	if(!pHash)
		return;

	/////////////////////////////////////////////////////////
	// Get hashing results

	QBitArray StatusMap = pHash->GetStatusMap();
	QList<TPair64> CorruptionSet = pHash->GetCorruptionSet();

	CRecordMap::SIterator FileIter;
	while(m_Records.IterateRanges(FileIter))
	{
		SRecord State = FileIter.uState;
		if(State.eStatus != SRecord::eUnknown) // we pull tle results only for ranges we dont know the status of yet
			continue;

		QPair<uint32, uint32> Range = pHash->IndexRange(FileIter.uBegin, FileIter.uEnd);
		for(int Index = Range.first; Index < Range.second; Index ++)
		{
			uint64 uBegin = pHash->IndexOffset(Index);
			uint64 uEnd = pHash->IndexOffset(Index + 1);

			if(StatusMap.testBit(Index))
			{
				State.eStatus = SRecord::eVerified;
				m_Records.SetRange(uBegin, uEnd, State);
			}
			else if(testRange(CorruptionSet, qMakePair(uBegin, uEnd)))
			{
				State.eStatus = SRecord::eCorrupted;
				m_Records.SetRange(uBegin, uEnd, State);
			}
		}
	}

	/////////////////////////////////////////////////////////
	// Do the evaluation

	uint64 uNow = GetCurTick();
	uint64 uClearTime = uNow + SEC2MS(theCore->Cfg()->GetInt("CorruptionLogger/MonitorTime"));

	CRecordMap::SIterator Iter;
	while(m_Records.IterateRanges(Iter, SRecord(SRecord::eAligned))) // this gives us Verifyed/Corrupt ranges, disregarding the different ID's that may be inside
	{
		if(Iter.uState.eStatus == SRecord::eNone || Iter.uState.eStatus == SRecord::eUnknown)
			continue; // range empty or not yet verifyed

		QPair<uint32, uint32> Range = pHash->IndexRange(Iter.uBegin, Iter.uEnd, false);
		for(uint32 Index = Range.first; Index < Range.second; Index++) // for each part in range
		{
			QMap<TID, int> Temp;

			CRecordMap::SIterator SubIter(pHash->IndexOffset(Index), pHash->IndexOffset(Index + 1));
			while(m_Records.IterateRanges(SubIter)) // for each contributor to the range
			{
				ASSERT(SubIter.uState.eStatus == Iter.uState.eStatus);
				TID tID;
				CAbstractKey::Fold((byte*)SubIter.uState.ID.data(), SubIter.uState.ID.size(), tID.data(), tID.size());
				SSuspect &Suspect = m_Suspects[tID];
				Temp[tID] += SubIter.uEnd - SubIter.uBegin;	
			}

			if(Iter.uState.eStatus == SRecord::eVerified)
			{
				for(QMap<TID, int>::const_iterator I = Temp.begin(); I != Temp.end(); I++)
				{
					SSuspect &Suspect = m_Suspects[I.key()];
					Suspect.uVerified += I.value();
					Suspect.uClearTime = uClearTime;
				}
			}
			else
			{
				for(QMap<TID, int>::const_iterator I = Temp.begin(); I != Temp.end(); I++)
				{
					SSuspect &Suspect = m_Suspects[I.key()];
					Suspect.uCorrupted += I.value();
					Suspect.uClearTime = uClearTime;
				}
			}
		}

		m_Records.SetRange(Iter.uBegin, Iter.uEnd, SRecord()); // clear the range
	}

	int Percentage = theCore->Cfg()->GetInt("CorruptionLogger/DropRatio");

	for(QMap<TID, SSuspect>::iterator I = m_Suspects.begin(); I != m_Suspects.end();)
	{
		SSuspect& Suspect = *I;

		Suspect.uDropped = Suspect.uCorrupted * 100 > (Suspect.uCorrupted + Suspect.uVerified) * Percentage;

		//if(Suspect.uCorrupted > 0)
		//	LogLine(LOG_DEBUG | LOG_WARNING, tr("Client %1 send corrupted data %2/%3, %4").arg("").arg(Suspect.uCorrupted).arg(Suspect.uVerified).arg(Suspect.uDropped ? tr("droped"): tr("ok")));

		if(Suspect.uClearTime < uNow)
			I = m_Suspects.erase(I);
		else
			I++;
	}
}

bool CCorruptionLogger::IsDropped(const QPair<const byte*, size_t>& ID) const
{
	TID tID;
	CAbstractKey::Fold(ID.first, ID.second, tID.data(), tID.size());
	QMap<TID, SSuspect>::const_iterator I = m_Suspects.find(tID);
	if(I == m_Suspects.end())
		return false;
	return I->uDropped;
}