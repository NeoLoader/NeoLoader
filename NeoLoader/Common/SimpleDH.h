#pragma once
#include "../Framework/Cryptography/AbstractKey.h"

class CSimpleDH: public CAbstractKey
{
public:
	CSimpleDH(size_t DH_Size, byte* DH_Prime = 0, byte DH_Base = 0)
	{
		m_eAlgorithm = CAbstractKey::eDH;

		AllocKey(DH_Size);
		m_Prime = CryptoPP::Integer(DH_Prime, DH_Size);
		m_Base	= CryptoPP::Integer(&DH_Base, 1);
	}

	CAbstractKey*		FinaliseKeyExchange(CAbstractKey* pPubKey)
	{
		CAbstractKey* pKey = new CAbstractKey(GetSize());
		CryptoPP::Integer cryptDHPriv((byte*)GetKey(), GetSize());
		CryptoPP::Integer cryptDHPub((byte*)pPubKey->GetKey(), GetSize());
		CryptoPP::Integer cryptResult = a_exp_b_mod_c(cryptDHPub, cryptDHPriv, m_Prime);
		cryptResult.Encode(pKey->GetKey(), GetSize());	
		return pKey;
	}

	CAbstractKey*		InitialsieKeyExchange()
	{
		CAbstractKey* pPubKey = new CAbstractKey(GetSize());
		CryptoPP::AutoSeededRandomPool rng;
		CryptoPP::Integer cryptDHPriv(rng,GetSize()*8); // Note: we could use much less bits 128 would suffice
		CryptoPP::Integer cryptDHPub = a_exp_b_mod_c(m_Base, cryptDHPriv, m_Prime);
		cryptDHPriv.Encode(GetKey(), GetSize());	
		cryptDHPub.Encode(pPubKey->GetKey(), GetSize());
		return pPubKey;
	}

private:
	CryptoPP::Integer	m_Prime;
	CryptoPP::Integer	m_Base;
};


/* Note: the DH prime used in the specifications is not accepted by Crypto++
*	Using a simple DH implementation that seams to work just fine, 
*		howeever the security might be compromised.
*/

/*int TestDH()
{
	CryptoPP::AutoSeededRandomPool rng;
	CryptoPP::Integer Prime = CryptoPP::Integer(BT_DH_Prime, BT_DH_Size);
	bool Test = CryptoPP::VerifyPrime(rng, Prime, 2);
	ASSERT(Test);

	//CKeyExchange* pExchange1 = CKeyExchange::Make(eDH, BT_DH_Size, BT_DH_Prime, BT_DH_Base);
	CKeyExchange* pExchange1 = new CSimpleDH(BT_DH_Size, BT_DH_Prime, BT_DH_Base);
	CKeyExchange* pExchange2 = new CSimpleDH(BT_DH_Size, BT_DH_Prime, BT_DH_Base);

	CAbstractKey* pPubKey1 = pExchange1->InitialsieKeyExchange();
	CAbstractKey* pPubKey2 = pExchange2->InitialsieKeyExchange();

	CAbstractKey* pKey1 = pExchange1->FinaliseKeyExchange(pPubKey2);
	CAbstractKey* pKey2 = pExchange2->FinaliseKeyExchange(pPubKey1);

	return 0;
}
int _TestDH = TestDH();*/