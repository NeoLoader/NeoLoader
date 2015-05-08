#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadID.h"
#include "../../Framework/Cryptography/HashFunction.h"

CKadID::CKadID()
{
}

CKadID::CKadID(const CUInt128& ID)
: CUInt128(ID)
{
	m_PublicKey = NULL;
}

bool CKadID::SetKey(CHolder<CPublicKey>& pKey, UINT eAlgorithm)
{
	if(m_PublicKey)
		return m_PublicKey->GetSize() == pKey->GetSize() && memcmp(m_PublicKey->GetKey(), pKey->GetKey(), m_PublicKey->GetSize()) == 0;

	CUInt128 ID;
	MakeID(pKey, ID.GetData(), ID.GetSize(), eAlgorithm);
	if(CompareTo(ID) != 0)
		return false;

	m_PublicKey = pKey;
	return true;
}

void CKadID::MakeID(CPublicKey* pKey, byte* pID, size_t uSize, UINT eAlgorithm)
{
	if(eAlgorithm == CAbstractKey::eUndefined)
	{
		if(uSize <= KEY_256BIT)
			eAlgorithm = CAbstractKey::eSHA256;
		else if(uSize <= KEY_512BIT)
			eAlgorithm = CAbstractKey::eSHA512;
	}

	CHashFunction Hash(eAlgorithm);
	Hash.Add(pKey->GetKey(), pKey->GetSize());
	Hash.Finish();
	CAbstractKey::Fold(Hash.GetKey(), Hash.GetSize(), pID, uSize);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// MyID

CMyKadID::CMyKadID(CPrivateKey* pKey)
{
	m_PrivateKey = pKey;
	m_PublicKey = pKey->PublicKey();

	MakeID(m_PublicKey, GetData(), GetSize());
}
