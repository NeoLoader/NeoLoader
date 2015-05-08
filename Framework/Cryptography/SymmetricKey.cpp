#include "GlobalHeader.h"
#include "SymmetricKey.h"

using namespace CryptoPP;

CCryptoKey::CCryptoKey(UINT eAlgorithm)
{
	m_eAlgorithm = eAlgorithm;
}

bool CCryptoKey::AdjustKey(const byte* pInKey, size_t uInSize, byte* pOutKey, size_t uOutSize)
{
	if((m_eAlgorithm & eHashFunkt) == eNone)
	{
		ASSERT(0);
		return false;
	}
	return CAbstractKey::AdjustKey(pInKey, uInSize, pOutKey, uOutSize, (m_eAlgorithm & eHashFunkt));
}

bool CCryptoKey::Process(CBuffer* pIn, CBuffer* pOut)
{
	if(pOut == NULL)
		pOut = pIn;
	else if(pOut->GetSize() == 0)
		pOut->AllocBuffer(pIn->GetSize(), true);
	else if(pOut->GetSize() < pIn->GetSize())
		return false;

	return Process(pIn->GetBuffer(), pOut->GetBuffer(), pIn->GetSize());
}

#ifdef USING_QT
bool CCryptoKey::Process(QByteArray* pIn, QByteArray* pOut) 
{
	if(pOut)
	{
		ASSERT(pOut->isEmpty());
		pOut->resize(pIn->size());
	}
	return Process((byte*)pIn->data(), pOut ? (byte*)pOut->data() : (byte*)pIn->data(), pIn->size());
}
#endif

/*class CRYPTOPP_NO_VTABLE XARC4_Base : public Weak1::ARC4_Base
{
public:
	static const char *StaticAlgorithmName() {return "XARC4";}

	typedef SymmetricCipherFinal<XARC4_Base> Encryption;
	typedef SymmetricCipherFinal<XARC4_Base> Decryption;

protected:
	unsigned int GetDefaultDiscardBytes() const {return 1024;}
};

//! Modified ARC4: it discards the first 1024 bytes of keystream which may be weaker than the rest
DOCUMENTED_TYPEDEF(SymmetricCipherFinal<XARC4_Base>, XARC4)*/

///////////////////////////////////////////////////////////////////////////////
//

template <class C>
CEncryptionKey* CEncryptionKey::MakeM(UINT eAlgorithm)
{
	switch(eAlgorithm & eSymMode)
	{
		case eECB:	return new CBlockKey<CEncryptionKey, C, typename ECB_Mode<C>::Encryption>(eAlgorithm);
		case eCBC:	return new CBlockKey<CEncryptionKey, C, typename CBC_Mode<C>::Encryption>(eAlgorithm);
		case eCFB:	return new CBlockKey<CEncryptionKey, C, typename CFB_Mode<C>::Encryption>(eAlgorithm);
		case eOFB:	return new CStreamKeyEx<CEncryptionKey, C, typename OFB_Mode<C>::Encryption>(eAlgorithm);
		case eCTR:	return new CStreamKey<CEncryptionKey, C, typename CTR_Mode<C>::Encryption>(eAlgorithm);
		case eCCM:	return new CAuthEnc<CEncryptionKey, C, typename CCM<C>::Encryption>(eAlgorithm);
		case eGCM:	return new CAuthEnc<CEncryptionKey, C, typename GCM<C>::Encryption>(eAlgorithm);
		case eEAX:	return new CAuthEnc<CEncryptionKey, C, typename EAX<C>::Encryption>(eAlgorithm);
		case eCTS:	return new CCTSEnc<CEncryptionKey, C, typename CBC_CTS_Mode<C>::Encryption>(eAlgorithm);
		default:	ASSERT(0); return NULL;
	}
}

CEncryptionKey* CEncryptionKey::Make(UINT eAlgorithm)
{
	switch(eAlgorithm & eSymCipher)
	{
		case eAES:			return MakeM<Rijndael>(eAlgorithm);
		case eSerpent:		return MakeM<Serpent>(eAlgorithm);
		case eTwofish:		return MakeM<Twofish>(eAlgorithm);
		case eMARS:			return MakeM<MARS>(eAlgorithm);
		case eRC6:			return MakeM<RC6>(eAlgorithm);
		case eSosemanuk:	return new CStreamKeyEx<CEncryptionKey, Sosemanuk, Sosemanuk::Encryption>(eAlgorithm);
		case eSalsa20:		return new CStreamKey<CEncryptionKey, Salsa20, Salsa20::Encryption>(eAlgorithm);
		case eXSalsa20:		return new CStreamKey<CEncryptionKey, XSalsa20, XSalsa20::Encryption>(eAlgorithm);
		case eRC4:			return new CRC4Key<CEncryptionKey, Weak::ARC4, Weak::ARC4::Encryption>(eAlgorithm);
		case eWeakRC4:		return new CRC4Key<CEncryptionKey, Weak::ARC4, Weak::ARC4::Encryption>(eAlgorithm);
		default:			ASSERT(0); return NULL;
	}
}

#ifdef USING_QT
QByteArray CEncryptionKey::Encrypt(QByteArray Data, UINT eAlgorithm, const QByteArray& Key, QByteArray& IV)
{
	if((eAlgorithm & CAbstractKey::eHashFunkt) == 0)
		eAlgorithm |= eSHA256;
	CEncryptionKey* pKey = Make(eAlgorithm);
	pKey->SetKey(Key);
	pKey->Setup(IV.isEmpty() ? Key : IV);
	QByteArray Buffer;
	pKey->Process(&Data,&Buffer);
	if((eAlgorithm & eSymMode) == eCTS)
	{
		if(CAbstractKey* pStolenIV = pKey->GetStolenIV())
			IV = pStolenIV->ToByteArray();
	}
	delete pKey;
	return Buffer;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//

template <class C>
CDecryptionKey* CDecryptionKey::MakeM(UINT eAlgorithm)
{
	switch(eAlgorithm & eSymMode)
	{
		case eECB:	return new CBlockKey<CDecryptionKey, C, typename ECB_Mode<C>::Decryption>(eAlgorithm);
		case eCBC:	return new CBlockKey<CDecryptionKey, C, typename CBC_Mode<C>::Decryption>(eAlgorithm);
		case eCFB:	return new CBlockKey<CDecryptionKey, C, typename CFB_Mode<C>::Decryption>(eAlgorithm);
		case eOFB:	return new CStreamKeyEx<CDecryptionKey, C, typename OFB_Mode<C>::Decryption>(eAlgorithm);
		case eCTR:	return new CStreamKey<CDecryptionKey, C, typename CTR_Mode<C>::Decryption>(eAlgorithm);
		case eCCM:	return new CAuthDec<CDecryptionKey, C, typename CCM<C>::Decryption>(eAlgorithm);
		case eGCM:	return new CAuthDec<CDecryptionKey, C, typename GCM<C>::Decryption>(eAlgorithm);
		case eEAX:	return new CAuthDec<CDecryptionKey, C, typename EAX<C>::Decryption>(eAlgorithm);
		case eCTS:	return new CCTSDec<CDecryptionKey, C, typename CBC_CTS_Mode<C>::Decryption>(eAlgorithm);
		default:	ASSERT(0); return NULL;
	}
}

CDecryptionKey* CDecryptionKey::Make(UINT eAlgorithm)
{
	switch(eAlgorithm & eSymCipher)
	{
		case eAES:			return MakeM<Rijndael>(eAlgorithm);
		case eSerpent:		return MakeM<Serpent>(eAlgorithm);
		case eTwofish:		return MakeM<Twofish>(eAlgorithm);
		case eMARS:			return MakeM<MARS>(eAlgorithm);
		case eRC6:			return MakeM<RC6>(eAlgorithm);
		case eSosemanuk:	return new CStreamKeyEx<CDecryptionKey, Sosemanuk, Sosemanuk::Decryption>(eAlgorithm);
		case eSalsa20:		return new CStreamKey<CDecryptionKey, Salsa20, Salsa20::Decryption>(eAlgorithm);
		case eXSalsa20:		return new CStreamKey<CDecryptionKey, XSalsa20, XSalsa20::Decryption>(eAlgorithm);
		case eRC4:			return new CRC4Key<CDecryptionKey, Weak::ARC4, Weak::ARC4::Decryption>(eAlgorithm);
		case eWeakRC4:		return new CRC4Key<CDecryptionKey, Weak::ARC4, Weak::ARC4::Decryption>(eAlgorithm);
		default:			ASSERT(0); return NULL;
	}
}

#ifdef USING_QT
QByteArray CDecryptionKey::Decrypt(QByteArray Data, UINT eAlgorithm, const QByteArray& Key, const QByteArray& IV)
{
	if((eAlgorithm & CAbstractKey::eHashFunkt) == 0)
		eAlgorithm |= eSHA256;
	CDecryptionKey* pKey = Make(eAlgorithm);
	pKey->SetKey(Key);
	pKey->Setup(IV.isEmpty() ? Key : IV);
	QByteArray Buffer;
	pKey->Process(&Data,&Buffer);
	delete pKey;
	return Buffer;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//

CSymmetricKey::CSymmetricKey(UINT eAlgorithm)
{
	m_eAlgorithm = eAlgorithm;
	m_pEncryptor = NULL;
	m_pDecryptor = NULL;
}

CSymmetricKey::~CSymmetricKey()
{
	delete m_pEncryptor;
	delete m_pDecryptor;
}

bool CSymmetricKey::SetupEncryption(byte* pIV, size_t uSize)
{
	ASSERT(m_pEncryptor == NULL);
	m_pEncryptor = CEncryptionKey::Make(m_eAlgorithm);
	if(m_pKey && !m_pEncryptor->SetKey(this))// for RC4 keys we only set an IV 
		return false;
	if(!m_pEncryptor->Setup(pIV, uSize))
		return false;
	return true;
}

bool CSymmetricKey::SetupDecryption(byte* pIV, size_t uSize)
{
	ASSERT(m_pDecryptor == NULL);
	m_pDecryptor = CDecryptionKey::Make(m_eAlgorithm);
	if(m_pKey && !m_pDecryptor->SetKey(this)) // for RC4 keys we only set an IV 
		return false;
	if(!m_pDecryptor->Setup(pIV, uSize))
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
CBlockKey<T, C,P>::CBlockKey(UINT eAlgorithm)
 : T(eAlgorithm)
{
	m_pProcessor = NULL;
}

template <class T, class C, class P>
CBlockKey<T, C,P>::~CBlockKey()
{
	delete m_pProcessor;
}

template <class T, class C, class P>
void CBlockKey<T,C,P>::Reset()
{
	CCryptoKey::Reset();

	delete m_pProcessor;
	m_pProcessor = NULL;
}

template <class T, class C, class P>
bool CBlockKey<T,C,P>::SetKey(byte* pKey, size_t uSize)
{
	ASSERT(m_pProcessor == 0);

	size_t uValidSize = C::StaticGetValidKeyLength(uSize);

	byte* Temp = NULL;
	if(uSize != uValidSize)
	{
		Temp = new byte[uValidSize];
		if(!CCryptoKey::AdjustKey(pKey, uSize, Temp, uValidSize))
			return false;
		pKey = Temp;
		uSize = uValidSize;
	}

	bool bRet = CAbstractKey::SetKey(pKey,uSize);
	delete Temp;
	return bRet;
}

template <class T, class C, class P>
bool CBlockKey<T,C,P>::Setup(byte* pIV, size_t uSize)
{
	ASSERT(m_pKey);
	if(!T::m_pKey)
		return false;

	byte* pTemp = NULL;
	try
	{
		ASSERT(m_pProcessor == NULL);
		m_pProcessor = new P();
		if(uSize == 0) // only valid for ECB mode, others will throw
			m_pProcessor->SetKey(T::m_pKey, T::m_uSize, g_nullNameValuePairs);
		else
		{
			if(uSize > m_pProcessor->MaxIVLength() || uSize < m_pProcessor->MinIVLength())
			{
				size_t uAdjSize = uSize < m_pProcessor->MinIVLength() ? m_pProcessor->MinIVLength() : m_pProcessor->MaxIVLength();
				pTemp = new byte[uAdjSize];
				if(!CCryptoKey::AdjustKey(pIV, uSize, pTemp, uAdjSize)) // if no IV is given us a hash of the key
					throw 0;
				pIV = pTemp;
				uSize = uAdjSize;
			}
			
			m_pProcessor->SetKeyWithIV(this->m_pKey, this->m_uSize, pIV, uSize);
		}
	}
	catch(...)
	{
		delete m_pProcessor;
		m_pProcessor = NULL;
		delete pTemp;
		ASSERT(0);
		return false;
	}
	delete pTemp;
	return true;
}

template <class T, class C, class P>
bool CBlockKey<T,C,P>::Seek(uint64 uPos)
{
	ASSERT(m_pProcessor);
	if(!m_pProcessor)
		return false;

	try
	{
		m_pProcessor->Seek(uPos);
	}
	catch(...)
	{
		ASSERT(0);
		return false;
	}
	return true;
}

template <class T, class C, class P>
bool CBlockKey<T,C,P>::Process(byte* pIn, byte* pOut, size_t uSize)
{
	ASSERT(m_pProcessor);
	if(!m_pProcessor)
		return false;
	try
	{
		m_pProcessor->ProcessData(pOut,pIn,uSize);
	}
	catch(...)
	{
		ASSERT(0);
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
//
template <class T, class C, class E>
CAuthEnc<T,C,E>::CAuthEnc(UINT eAlgorithm) 
 : CBlockKey<T, C, E>(eAlgorithm)
{
	m_pMac = NULL;
}

template <class T, class C, class E>
CAuthEnc<T,C,E>::~CAuthEnc()
{
	delete m_pMac;
}

template <class T, class C, class E>
CAbstractKey* CAuthEnc<T,C,E>::Finish(size_t uSize)
{
	if(!CBlockKey<T, C, E>::m_pProcessor)
	{
		ASSERT(0);
		return NULL;
	}

	delete m_pMac;
	m_pMac = new CAbstractKey(uSize ? uSize : CBlockKey<T, C, E>::m_pProcessor->TagSize());
	try
	{
		CBlockKey<T, C, E>::m_pProcessor->TruncatedFinal(m_pMac->GetKey(), m_pMac->GetSize());

		delete CBlockKey<T, C, E>::m_pProcessor;
		CBlockKey<T, C, E>::m_pProcessor = NULL;
	}
	catch(...)
	{
		ASSERT(0);
		delete m_pMac;
		m_pMac = NULL;
	}
	return m_pMac;
}

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class D>
bool CAuthDec<T,C,D>::Verify(CAbstractKey* pMac)
{
	if(!CBlockKey<T, C, D>::m_pProcessor)
	{
		ASSERT(0);
		return NULL;
	}

	bool Ret;
	try
	{
		Ret = CBlockKey<T, C, D>::m_pProcessor->TruncatedVerify(pMac->GetKey(), pMac->GetSize());

		delete CBlockKey<T, C, D>::m_pProcessor;
		CBlockKey<T, C, D>::m_pProcessor = NULL;
	}
	catch(...)
	{
		return false;
	}
	return Ret;
}

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class E>
CCTSEnc<T,C,E>::CCTSEnc(UINT eAlgorithm)
: CBlockKey<T,C,E>(eAlgorithm)
{
	m_pStolenIV = NULL;
}

template <class T, class C, class E>
CCTSEnc<T,C,E>::~CCTSEnc()
{
	delete m_pStolenIV;
}

template <class T, class C, class E>
void CCTSEnc<T, C,E>::Reset()
{
	CBlockKey<T,C,E>::Reset();
	delete m_pStolenIV;
	m_pStolenIV = NULL;
}

template <class T, class C, class E>
bool CCTSEnc<T,C,E>::Process(byte* pIn, byte* pOut, size_t uSize)
{
	if(!CBlockKey<T,C,E>::m_pProcessor)
	{
		ASSERT(0);
		return false;
	}

	try
	{
		size_t Blocks = (uSize + C::BLOCKSIZE - 1) / C::BLOCKSIZE;
		if(Blocks > 2)
		{
			size_t Blocks = (uSize + C::BLOCKSIZE - 1) / C::BLOCKSIZE;
			size_t uToGo = (Blocks - 2) * C::BLOCKSIZE; // process all blocks but 2
			CBlockKey<T,C,E>::m_pProcessor->ProcessData(pOut, pIn, uToGo);
			pOut += uToGo;
			pIn += uToGo;
			uSize -= uToGo;
		}
		else if(Blocks == 1)
		{
			delete m_pStolenIV;
			m_pStolenIV = new CAbstractKey(CBlockKey<T,C,E>::m_pProcessor->IVSize());
			CBlockKey<T,C,E>::m_pProcessor->SetStolenIV(m_pStolenIV->GetKey());
		}

		byte pTemp[C::BLOCKSIZE*2];
		ASSERT(uSize <= C::BLOCKSIZE*2);
		CBlockKey<T,C,E>::m_pProcessor->ProcessLastBlock(pTemp,pIn,uSize);
		memcpy(pOut, pTemp, uSize);

		delete CBlockKey<T,C,E>::m_pProcessor;
		CBlockKey<T,C,E>::m_pProcessor = NULL;
	}
	catch(...)
	{
		ASSERT(0);
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class D>
bool CCTSDec<T,C,D>::Process(byte* pOut, byte* pIn, size_t uSize)
{
	if(!CBlockKey<T,C,D>::m_pProcessor)
	{
		ASSERT(0);
		return false;
	}

	try
	{
		size_t Blocks = (uSize + C::BLOCKSIZE - 1) / C::BLOCKSIZE;
		if(Blocks > 2)
		{
			size_t uToGo = (Blocks - 2) * C::BLOCKSIZE; // process all blocks but 2
			CBlockKey<T,C,D>::m_pProcessor->ProcessData(pIn,pOut, uToGo);
			pOut += uToGo;
			pIn += uToGo;
			uSize -= uToGo;
		}

		byte pTemp[C::BLOCKSIZE*2];
		ASSERT(uSize <= C::BLOCKSIZE*2);
		CBlockKey<T,C,D>::m_pProcessor->ProcessLastBlock(pTemp,pOut,uSize);
		memcpy(pIn, pTemp, uSize);

		delete CBlockKey<T,C,D>::m_pProcessor;
		CBlockKey<T,C,D>::m_pProcessor = NULL;
	}
	catch(...)
	{
		ASSERT(0);
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
bool CStreamKey<T,C,P>::Discard(uint64 uToGo)
{
	ASSERT(m_pProcessor);
	if(!this->m_pProcessor)
		return false;

	try
	{
		this->m_pProcessor->DiscardBytes(uToGo);
	}
	catch(...)
	{
		ASSERT(0);
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
CStreamKeyEx<T,C,P>::CStreamKeyEx(UINT eAlgorithm)
: CStreamKey<T,C,P>(eAlgorithm)
{
	m_pIV = NULL;
	m_uIVSize = 0;
	m_uPosition = 0;
}

template <class T, class C, class P>
CStreamKeyEx<T,C,P>::~CStreamKeyEx()
{
	delete m_pIV;
}

template <class T, class C, class P>
void CStreamKeyEx<T,C,P>::Reset()
{
	CStreamKey<T,C,P>::Reset();

	delete m_pIV;
	m_pIV = NULL;
	m_uIVSize = 0;
	m_uPosition = 0;
}

template <class T, class C, class P>
bool CStreamKeyEx<T,C,P>::Setup(byte* pIV, size_t uSize)
{
	SetIV(pIV, uSize);
	return CStreamKey<T, C, P>::Setup(pIV, uSize);
}

template <class T, class C, class P>
void CStreamKeyEx<T,C,P>::SetIV(byte* pIV, size_t uSize)
{
	if(uSize > 0 && pIV != m_pIV) // IV set and is nto a reset
	{
		ASSERT(pIV);
		ASSERT(!m_pIV);
		m_uIVSize = uSize;
		m_pIV = new byte[m_uIVSize];
		memcpy(m_pIV, pIV, m_uIVSize);
	}
}

template <class T, class C, class P>
bool CStreamKeyEx<T,C,P>::Process(byte* pIn, byte* pOut, size_t uSize)
{
	ASSERT(m_pProcessor);
	if(!this->m_pProcessor)
		return false;
	try
	{
		this->m_pProcessor->ProcessData(pOut,pIn,uSize);
		m_uPosition += uSize;
	}
	catch(...)
	{
		ASSERT(0);
		return false;
	}
	return true;
}

template <class T, class C, class P>
bool CStreamKeyEx<T,C,P>::Seek(uint64 uPos)
{
	if(uPos == m_uPosition)
		return true;
	if(uPos < m_uPosition)
	{
		this->Reset();
		this->Setup(m_pIV, m_uIVSize);
	}

	this->Discard(uPos - m_uPosition); // forward only seeking
	m_uPosition = uPos;
	return true;
}

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
bool CRC4Key<T,C,P>::Setup(byte* pIV, size_t uSize)
{
	this->SetIV(pIV, uSize);
	byte* pTemp = NULL;
	try
	{
		ASSERT(m_pProcessor == 0);
		this->m_pProcessor = new P();

		if(uSize && this->m_uSize)
		{
			pTemp = new byte[this->m_uSize];
			for(size_t i=0; i < max(this->m_uSize,uSize) ; i++)
				pTemp[i % this->m_uSize] = this->m_pKey[i % this->m_uSize] ^ pIV[i % uSize];
			this->m_pProcessor->SetKey(pTemp, this->m_uSize);
		}
		else if(this->m_uSize != 0) // no IV as it should be
			this->m_pProcessor->SetKey(this->m_pKey, this->m_uSize);
		else if(uSize != 0) // no Key, IV acts as key
		{
			ASSERT(uSize == C::StaticGetValidKeyLength(uSize)); // Note: this operation mode does not support internak key adjutement, proepr keys must be used
			this->m_pProcessor->SetKey(pIV, uSize);
		}
		else // nider key nor IV is bad
			throw 0; 

		// in not so weak RC4 mode we always discard the first KB of the data
		if((this->m_eAlgorithm & CStreamKey<T,C,P>::eSymCipher) == CStreamKey<T,C,P>::eRC4) // and not eWeakRC4
			this->Discard(1024);
	}
	catch(...)
	{
		delete this->m_pProcessor;
		this->m_pProcessor = NULL;
		delete pTemp;
		ASSERT(0);
		return false;
	}
	delete pTemp;
	return true;
}
