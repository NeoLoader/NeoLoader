#include "GlobalHeader.h"
#include "HashFunction.h"

CHashFunction::CHashFunction(UINT eAlgorithm)
{
	m_eAlgorithm = eAlgorithm;
	m_pFunction = GetHashFunction(eAlgorithm);
	if(!m_pFunction)
		return;

	AllocKey(m_pFunction->DigestSize());
	m_pFunction->Restart();
}

CHashFunction::~CHashFunction()
{
	delete m_pFunction;
}

CryptoPP::HashFunction* CHashFunction::GetHashFunction(UINT eAlgorithm)
{
	switch(eAlgorithm & eHashFunkt)
	{
		case eSHA1:			return new CryptoPP::SHA1;
		case eSHA224:		return new CryptoPP::SHA224;
		case eSHA256:		return new CryptoPP::SHA256;
		case eSHA384:		return new CryptoPP::SHA384;
		case eSHA512:		return new CryptoPP::SHA512;
		case eTiger:		return new CryptoPP::Tiger;
		case eWhirlpool:	return new CryptoPP::Whirlpool;
		case eRIPEMD128:	return new CryptoPP::RIPEMD128;
		case eRIPEMD160:	return new CryptoPP::RIPEMD160;
		case eRIPEMD256:	return new CryptoPP::RIPEMD256;
		case eRIPEMD320:	return new CryptoPP::RIPEMD320;
		case eMD5:			return new CryptoPP::Weak1::MD5;
		case eMD4:			return new CryptoPP::Weak1::MD4;
		case eCRC:			return new CryptoPP::CRC32;
		default:			return NULL;
	}
}

void CHashFunction::Reset()
{
	if(m_pFunction)
		m_pFunction->Restart();
}

bool CHashFunction::Add(const byte* pData, size_t uLen)
{
	if(m_pFunction == NULL)
		return false;
	
	m_pFunction->Update(pData,uLen);
	return true;
}

bool CHashFunction::Finish()
{
	if(m_pFunction == NULL)
		return false;

	m_pFunction->Final(GetKey());
	return true;
}

void CHashFunction::Fold(byte* pOutKey, size_t uOutSize)
{
	CAbstractKey::Fold(m_pKey, m_uSize, pOutKey, uOutSize);
}

#ifdef USING_QT
QByteArray CHashFunction::Hash(const QByteArray &Data, UINT eAlgorithm)
{
	CHashFunction HashFx(eAlgorithm);
	HashFx.Add(Data);
	HashFx.Finish();
	return HashFx.ToByteArray();
}
#endif