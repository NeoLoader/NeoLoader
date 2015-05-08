#pragma once

#include "FileHash.h"
#include "../../../Framework/Address.h"
class CFile;

class CUntrustedFileHash: public CFileHash
{
	Q_OBJECT

public:
	CUntrustedFileHash(EFileHashType eType);
	virtual ~CUntrustedFileHash();

	virtual void		AddHash(CFileHashPtr pHash, int Count);
	virtual void		AddHash(CFileHashPtr pHash, const CAddress& Address);

	virtual bool		SelectTrustedHash(int Majority, int Quorum);

protected:
	struct SFileHash
	{
		SFileHash(CFileHashPtr pHash)
		{
			FileHash = pHash;
		}
		~SFileHash()
		{
			foreach(SReferee* pReferee, Referees)
				delete pReferee;
		}

		struct SReferee
		{
			enum EType
			{
				eIndividual,
				eCollective
			}	Type;
			union SBy
			{
				CAddress*	pAddr;
				UINT			Count;
			}	By;

			SReferee(const CAddress& Address)
			{
				Type = eIndividual;
				By.pAddr = new CAddress(Address);
			}
			SReferee(int Count)
			{
				Type = eCollective;
				By.Count = Count;
			}
			~SReferee()
			{
				switch(Type)
				{
					case eIndividual:	delete By.pAddr;	break;
					case eCollective:						break;
				}
			}
		};
		QList<SReferee*>		Referees;
		CFileHashPtr			FileHash;
	};

	virtual SFileHash*	AddHash(CFileHashPtr pHash);

	QSet<SFileHash*>	m_Hashes;
};