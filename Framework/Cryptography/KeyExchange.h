#pragma once

#include "AbstractKey.h"

class NEOHELPER_EXPORT CKeyExchange: public CAbstractKey
{
public:
	static CAbstractKey*	MakeMaterials(UINT eAlgorithm, size_t uSize);

	CKeyExchange(UINT eAlgorithm, const void* pMaterials, size_t uMaterials = 0);
	virtual ~CKeyExchange();

	virtual CAbstractKey*	FinaliseKeyExchange(CAbstractKey* pPubKey);
	virtual CAbstractKey*	InitialsieKeyExchange();

	virtual void			SetIV(CAbstractKey* pIV)	{ASSERT(m_pIV == NULL); m_pIV = pIV;}
	virtual CAbstractKey*	GetIV()						{return m_pIV;}

protected:
	CryptoPP::SimpleKeyAgreementDomain*	m_pExchange;

	CAbstractKey*			m_pIV;
};
