#include "GlobalHeader.h"
#include "PartDownloader.h"
#include "../NeoCore.h"
#include "../FileList/Hashing/FileHashTree.h"
#include "../FileList/Hashing/FileHashSet.h"
#include "../FileList/FileStats.h"
#include "../FileList/IOManager.h"
#include "Transfer.h"
#include "../../Framework/OtherFunctions.h"
#include <math.h>

CPartDownloader::CPartDownloader(CFile* pFile)
: QObjectEx(pFile) 
{
}

//void CPartDownloader::Process(UINT Tick)
//{
//}

void CPartDownloader::OnChange(bool Purge)
{
	CFile* pFile = GetFile();
	// this is triggered when a significant change to the partmap occured
	for(QMap<QByteArray, SDownloadPlan>::iterator I = m_DownloadPlans.begin(); I != m_DownloadPlans.end(); I++)
	{
		if(CFileHashPtr pHash = pFile->GetHashPtrEx(I.value().Type, I.key()))
			UpdatePlan(pHash.data(), I.value(), true);
	}

	if(Purge)
	{
		foreach(CTransfer* pTransfer, pFile->GetTransfers())
		{
			if(pTransfer->IsActiveDownload())
				pTransfer->ResetSchedule();
		}
	}
}

const QVector<CPartDownloader::SPart>& CPartDownloader::GetDownloadPlan(CFileHash* pHash)
{
	// Z-ToDo: cleanup when a hash was removed
	SDownloadPlan& DownloadPlan = m_DownloadPlans[pHash->GetHash()];
	if(DownloadPlan.Type == HashUnknown)
		DownloadPlan.Type = pHash->GetType();
	else {
		ASSERT(DownloadPlan.Type == pHash->GetType());}
	UpdatePlan(pHash, DownloadPlan);
	return DownloadPlan.Plan;
}

struct SSortHolder
{
	SSortHolder(const SSortHolder& Copy){
		bErase = true;
		pScore = new double[Copy.size()];
		pParts = new QVector<CPartDownloader::SPart>;
		pParts->resize(Copy.size());
	}
	SSortHolder(QVector<CPartDownloader::SPart> &parts, double* score)
		:pParts(&parts), pScore(score), bErase(false) {}
	~SSortHolder(){
		if(bErase)
			delete pParts;
		delete pScore;
	}
	QVector<CPartDownloader::SPart> *pParts;
	int size() const {return pParts->size();}
	double* pScore;
	bool bErase;
};

bool PartCmp(SSortHolder& A, uint32 l, uint32 r)
{
	return A.pScore[l] >= A.pScore[r];
}

void PartCpy(SSortHolder& T, SSortHolder& S, uint32 t, uint32 s)
{
	T.pParts->replace(t, S.pParts->at(s));
	T.pScore[t] = S.pScore[s];
}

void PartSwap(SSortHolder &A, uint32 r, uint32 l)
{
	CPartDownloader::SPart Part = A.pParts->at(r);
	uint32 uScore = A.pScore[r];
	A.pParts->replace(r, A.pParts->at(l));
	A.pScore[r] = A.pScore[l];
	A.pParts->replace(l, Part);
	A.pScore[l] = uScore;
}

void CPartDownloader::UpdatePlan(CFileHash* pHash, SDownloadPlan& DownloadPlan, bool bNow)
{
	CFile* pFile = GetFile();
	CAvailMap* pAvail = pFile->GetStats()->GetAvailMap();
	bool bWasAllocating = DownloadPlan.bWasAllocating;
	DownloadPlan.bWasAllocating = pFile->IsAllocating();
	if(pAvail && (bNow || (
	 (DownloadPlan.bWasAllocating || bWasAllocating || DownloadPlan.uRevision != pAvail->GetRevision()) && DownloadPlan.uInvalidate < GetCurTick())
	 ))
	{
		uint64 uStamp = GetCurTick();

		uint64 PartSize = -1;
		uint32 PartCount = -1;
		if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pHash))
		{
			PartSize = pHashSet->GetPartSize();
			PartCount = pHashSet->GetPartCount();
		}
		else if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pHash))
		{
			PartSize = pHashTree->GetBlockSize();
			if(pHash->GetType() == HashNeo || pHash->GetType() == HashXNeo)
				PartSize *= 32;
			PartCount = DivUp(pHashTree->GetTotalSize(), PartSize);
		}

		if(PartCount != -1)
		{
			CPartMap* pParts = pFile->GetPartMap();
			ASSERT(pParts);

			if(!pParts || pParts->GetSize() != pAvail->GetSize())
			{
				ASSERT(0);
				return;
			}

			if(DownloadPlan.Plan.size() != PartCount)
				DownloadPlan.Plan.resize(PartCount);
			double* pScore = new double[PartCount];

			QVector<double> Bias;
			Bias.resize(PartCount);
			uint64 MaxPartSize = theCore->Cfg()->GetUInt64("HosterCache/PartSize");
			double ToGo = MaxPartSize / PartSize / 2;
			if(ToGo <= 0) // part count to go in each direction
				ToGo = 1;
			CPartMap::SIterator Iter;
			while(pParts->IterateRanges(Iter, Part::Available))
			{
				if((Iter.uState & Part::Available) == 0)
					continue;

				uint64 uLength = Iter.uEnd - Iter.uBegin;
				int Count = uLength / PartSize;

				// make sure the bias is the bigger the smaller the iland is
				double MaxBias = (Count <= ToGo) ? 100 - 100 * Count/ToGo : 0;
				if(MaxBias < 10) // any iland has a boost of at least 10
					MaxBias = 10;

				int First = DivUp(Iter.uBegin, PartSize) - 1;
				int Last = (Iter.uEnd / PartSize) + 1;
				for(int Cnt = 0, i=First, j=Last; Cnt < ToGo; Cnt++, i--, j++)
				{
					double Mod = pow(sqrt(MaxBias) - sqrt(MaxBias * ++Cnt/ToGo), 2);
					//x                 
					// x                
					//  x               
					//    x             
					//       x          
					//             x    
					// // // // // // // 
					//		(cnt)
					if(i >= 0) // befoure the iland
						Bias[i] += Mod;
					if(j < PartCount) // after the iland
						Bias[j] += Mod;
				}
			}

			uint32 uCompleteAvail = pFile->GetStats()->GetAvailStatsRaw(true);

			//bool bStream = false;
			uint32 PlanedParts = 0;
			for(uint32 i=0; i < PartCount; i++)
			{
				CPartDownloader::SPart Part;
				Part.uBegin = i * PartSize;
				Part.uEnd = Part.uBegin + PartSize;
				if(Part.uEnd > pParts->GetSize())
					Part.uEnd = pParts->GetSize();
				if(Part.uBegin >= Part.uEnd) // Z-ToDo: fix me
					continue;
				Part.iNumber = i;

				SPartRange Inter = pParts->GetRange(Part.uBegin, Part.uEnd, CPartMap::eInter);
				if((Inter & Part::Available) != 0)
					continue; // dont plan already completed parts
				if((Inter & Part::Allocated) == 0)
					continue; // dont plan parts for that no file was allocated yet
				if((Inter & Part::Disabled) != 0)
					continue; // dont plan parts that are dissabled
				SPartRange Union = pParts->GetRange(Part.uBegin, Part.uEnd, CPartMap::eUnion);

				// start with base score
				double uScore = 1.0;

				if((Union & Part::Required) != 0)
				{
					uScore += 1000 * 100;
				}
				else if((Union & Part::Stream) != 0)
				{
				//	bStream = true;
					if(Union.iPriority == 0)
						continue;

					uScore += 1000 * 10; // that must be bigger than the random part
				}
				else
				{
					uScore += 500.0 * qrand() / RAND_MAX;

					// stimulate island groth
					uScore += (Bias[i] >= 1000 ? 1000 : Bias[i]) / 10;
				}
				
				// add part priority
				if(Union.iPriority)
				{
					int iPriority2 = Union.iPriority*Union.iPriority; // ^2
					uScore += iPriority2; 
					uScore *= iPriority2; 
				}

				if((Union & Part::Stream) != 0 || (Union & Part::Required) != 0)
					Part.bStream = true;
				else
				{
					// rare parts have priority
					uint32 uAvail = pAvail->GetRange(Part.uBegin, Part.uEnd, CAvailMap::eUnion);
					if(uCompleteAvail)
					{
						if(uAvail > uCompleteAvail)
							uAvail -= uCompleteAvail;
						else // uCompleteAvail is out of date
							uAvail = 1;
					}
					if(uAvail < 10) 
						uScore *= 11 - uAvail; // * 1 - 10

					// partialy completed
					if((Union & Part::Available) != 0) 
						uScore *= 100; // incompete bust started parts have super high priority
				}

				DownloadPlan.Plan[PlanedParts] = Part;
				pScore[PlanedParts] = uScore;
				PlanedParts++;
			}
			DownloadPlan.Plan.resize(PlanedParts);

            SSortHolder Holder(DownloadPlan.Plan, pScore); // this deletes pScore
			//if(bStream)
				MergeSort(Holder, &PartCpy, &PartCmp);
			//else // use unstabile heapsort for better randomization
			//	HeapSort(Holder, &PartSwap, &PartCmp);

/*#ifdef _DEBUG
			QString PartsStr;
			for(int i=0; i < Min(10, PlanedParts); i++)
				PartsStr.append(QString::number(DownloadPlan.Plan[i].iNumber) + ", ");
			qDebug() << PartsStr;
#endif*/

			DownloadPlan.uRevision = pAvail->GetRevision();
			uint64 uDelay = (GetCurTick() - uStamp) * 100;
			DownloadPlan.uInvalidate = GetCurTick() + Min(SEC2MS(10), Max(SEC2MS(1), uDelay));
		}
	}
}
