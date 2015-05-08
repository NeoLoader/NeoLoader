#include "GlobalHeader.h"
#include "KeyExchange.h"

using namespace CryptoPP;


CAbstractKey* CKeyExchange::MakeMaterials(UINT eAlgorithm, size_t uSize)
{
	AutoSeededRandomPool rng;
	SimpleKeyAgreementDomain*	pExchange;
	try
	{
		switch(eAlgorithm & eKeyExchange)
		{
			case eDH:			pExchange = new DH(*(RandomNumberGenerator*)&rng,(int)(uSize*8)); break;
			case eLUCDH:		pExchange = new LUC_DH(*(RandomNumberGenerator*)&rng,(int)(uSize*8)); break;
			default: throw 0;
		}
	}
	catch (...)
	{
		ASSERT(0);
		return NULL;
	}

	byte MaterialArr[10240];
	ArraySink MaterialSink(MaterialArr, ARRSIZE(MaterialArr));
	pExchange->DEREncode(MaterialSink);

	CAbstractKey* pKey = new CAbstractKey();
	pKey->SetKey(MaterialArr, MaterialSink.TotalPutLength());

	return pKey;
}

CKeyExchange::CKeyExchange(UINT eAlgorithm, const void* pMaterials, size_t uMaterials)
{
	try
	{
		switch(eAlgorithm & eKeyExchange)
		{
			case eDH:
			{
				ArraySource MaterialSource((byte*)pMaterials,uMaterials,true,0);
				m_pExchange = new DH(MaterialSource);
				break;
			}
			case eECDH:
			{
				pOID pCurve = GetECCurve((char*)pMaterials);
				if(!pCurve)	throw 0;
				switch(eAlgorithm & eAsymMode)
				{
					case 0:
					case eECP:	m_pExchange = new ECDH<ECP>::Domain(pCurve()); break;
					case eEC2N: m_pExchange = new ECDH<EC2N>::Domain(pCurve()); break;
					default: throw 0;
				}
				break;
			}
			case eLUCDH:
			{
				ArraySource MaterialSource((byte*)pMaterials,uMaterials,true,0);
				m_pExchange = new LUC_DH(MaterialSource);
				break;
			}
			case eNone:
				m_pExchange = NULL;
				m_uSize = uMaterials;
				break;
		}
		m_eAlgorithm = eAlgorithm;
	}
	catch (...)
	{
		ASSERT(0);
		m_eAlgorithm = eUndefined;
		m_pExchange = NULL;
	}

	m_pIV = NULL;
}

CKeyExchange::~CKeyExchange()
{
	delete m_pExchange;
	delete m_pIV;
}

CAbstractKey* CKeyExchange::FinaliseKeyExchange(CAbstractKey* pPubKey)
{
	if(!m_pExchange) // temp mode
	{
		ASSERT(!pPubKey);
		return new CAbstractKey(m_pKey, m_uSize);
	}

	CAbstractKey* pKey = new CAbstractKey(m_pExchange->AgreedValueLength());
	try
	{
		if(!pPubKey || !m_pExchange->Agree(pKey->GetKey(), m_pKey, pPubKey->GetKey()))
		{
			delete pKey;
			pKey = NULL;
		}
	}
	catch(...)
	{
		ASSERT(0);
	}

	m_uSize = 0;
	delete m_pKey;
	m_pKey = NULL;

	return pKey;
}

CAbstractKey* CKeyExchange::InitialsieKeyExchange()
{
	if(!m_pExchange) // temp mode
	{
		ASSERT(m_uSize > 0);
		if(m_pKey == NULL)
			m_pKey = CAbstractKey::GenerateRandomKey(m_uSize);
		return new CAbstractKey(m_pKey, m_uSize);
	}

	AutoSeededRandomPool rng;

	ASSERT(m_pKey == NULL);
	m_uSize = m_pExchange->PrivateKeyLength();
	m_pKey = new byte[m_uSize];
	CAbstractKey* pPubKey = new CAbstractKey(m_pExchange->PublicKeyLength());
	try
	{
		m_pExchange->GenerateKeyPair(rng, m_pKey, pPubKey->GetKey());
	}
	catch(...)
	{
		ASSERT(0);
	}

	return pPubKey;
}
