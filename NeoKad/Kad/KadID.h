#pragma once

#include "UIntX.h"
#include "../../Framework/Cryptography/AsymmetricKey.h"

class CKadID: public CUInt128
{
public:
	CKadID();
	CKadID(const CUInt128& ID);

	virtual bool			SetKey(CHolder<CPublicKey>& pKey, UINT eAlgorithm = CAbstractKey::eUndefined);

	virtual CPublicKey*		GetKey() const					{return m_PublicKey;}

	static	void			MakeID(CPublicKey* pKey, byte* pID, size_t uSize, UINT eAlgorithm = CAbstractKey::eUndefined);

protected:
	CHolder<CPublicKey>		m_PublicKey;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// MyID

class CMyKadID: public CKadID
{
public:
	CMyKadID(CPrivateKey* pKey);

	virtual bool			SetKey(CHolder<CPublicKey>& pKey){ASSERT(0); return false;}

	virtual CPublicKey*		GetKey() const					{return m_PublicKey;}

	virtual CPrivateKey*	GetPrivateKey() const			{return m_PrivateKey;}

protected:
	CPrivateKey*			m_PrivateKey; // Note: the key is storred externaly and not to be deleted
};
