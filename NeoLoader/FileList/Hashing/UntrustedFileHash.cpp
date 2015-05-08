#include "GlobalHeader.h"
#include "UntrustedFileHash.h"
#include "../FileManager.h"
#include "../../NeoCore.h"
#include "FileHashTree.h"


CUntrustedFileHash::CUntrustedFileHash(EFileHashType eType)
 : CFileHash(eType)
{
}

CUntrustedFileHash::~CUntrustedFileHash()
{
	foreach(SFileHash* pHash, m_Hashes)
		delete pHash;
}

void CUntrustedFileHash::AddHash(CFileHashPtr pHash, int Count)
{
	SFileHash* pCurHash = AddHash(pHash);
	pCurHash->Referees.append(new SFileHash::SReferee(Count));
}

void CUntrustedFileHash::AddHash(CFileHashPtr pHash, const CAddress& Address)
{
	SFileHash* pCurHash = AddHash(pHash);
	foreach(SFileHash::SReferee* pReferee, pCurHash->Referees)
	{
		if(pReferee->Type == SFileHash::SReferee::eIndividual)
		{
			if(*pReferee->By.pAddr == Address)
				return;
		}
	}
	pCurHash->Referees.append(new SFileHash::SReferee(Address));
}

CUntrustedFileHash::SFileHash* CUntrustedFileHash::AddHash(CFileHashPtr pHash)
{
	foreach(SFileHash* pCurHash, m_Hashes)
	{
		if(pCurHash->FileHash->Compare(pHash.data()))
			return pCurHash;
	}

	SFileHash* pCurHash = new SFileHash(pHash);
	m_Hashes.insert(pCurHash);
	return pCurHash;
}

bool CUntrustedFileHash::SelectTrustedHash(int Majority, int Quorum)
{
	CFileHashPtr pTrustedHash;
	int BestCount = 0;
	int TotalCount = 0;
	foreach(SFileHash* pCurHash, m_Hashes)
	{
		int CurCount = 0;
		foreach(SFileHash::SReferee* pReferee, pCurHash->Referees)
		{
			switch(pReferee->Type)
			{
				case SFileHash::SReferee::eIndividual:	CurCount ++;					break;
				case SFileHash::SReferee::eCollective:	CurCount += pReferee->By.Count;	break;
			}
		}

		TotalCount += CurCount;
		if(BestCount < CurCount)
		{
			BestCount = CurCount;
			pTrustedHash = pCurHash->FileHash;
		}
	}

	ASSERT(BestCount > 0);
	if((BestCount * 100 / TotalCount) >= Majority && TotalCount >= Quorum)
	{
		SetHash(pTrustedHash->GetHash());
		return true;
	}
	return false;
}
