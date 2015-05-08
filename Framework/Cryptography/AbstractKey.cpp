#include "GlobalHeader.h"
#include "AbstractKey.h"
#include "HashFunction.h"
#include "../Strings.h"


using namespace CryptoPP;

CAbstractKey::CAbstractKey()
{
	m_eAlgorithm = 0;
	m_pKey = NULL;
	m_uSize = 0;
}

CAbstractKey::CAbstractKey(size_t uSize, bool bRand)
{
	m_eAlgorithm = 0;
	m_pKey = NULL;
	if(bRand)
	{
		m_pKey = GenerateRandomKey(uSize);
		m_uSize = uSize;
	}
	else
		AllocKey(uSize);
}

CAbstractKey::CAbstractKey(byte* pKey, size_t uSize, UINT eAlgorithm)
{
	m_pKey = NULL;
	AllocKey(uSize);
	memcpy(m_pKey,pKey,uSize);
	m_eAlgorithm = eAlgorithm;
}

CAbstractKey::~CAbstractKey()
{
	FreeKey();
}

void CAbstractKey::AllocKey(size_t uSize)
{
	ASSERT(m_pKey == NULL);
	m_pKey = new byte[uSize];
	m_uSize = uSize;
}

bool CAbstractKey::SetupKey(byte* pKey, size_t uSize)
{
	AllocKey(uSize);
	memcpy(m_pKey,pKey,uSize);
	return true;
}

void CAbstractKey::FreeKey()
{
	if(!m_pKey)
		return;
	delete m_pKey;
	m_pKey = NULL;
	m_uSize = 0;
}

bool CAbstractKey::SetKey(byte* pKey, size_t uSize)
{
	FreeKey();
	if(pKey == NULL)
		m_pKey = NULL;
	else if(!SetupKey(pKey, uSize))
		return false;
	m_uSize = uSize;
	return true;
}

bool CAbstractKey::CompareTo(const CAbstractKey* Key) const
{
	if(m_uSize != Key->m_uSize)
		return false;
	return memcmp(m_pKey, Key->m_pKey, m_uSize) == 0;
}

byte* CAbstractKey::GenerateRandomKey(size_t uSize)
{
	if(!uSize)
		return NULL;

	byte* pKey = new byte[uSize];
	AutoSeededRandomPool rng;
	rng.GenerateBlock(pKey, uSize);
	return pKey;
}

bool CAbstractKey::AdjustKey(const byte* pInKey, size_t uInSize, byte* pOutKey, size_t uOutSize, UINT eAlgorithm)
{
	CHashFunction Hash(eAlgorithm);
	if(uOutSize > uInSize)
	{
		if(!Hash.IsValid()) // not supported
			return false;
		if(Hash.GetSize() < uOutSize) // fail
		{
			ASSERT(0);
			return false;
		}

		Hash.Add(pInKey, uInSize);
		Hash.Finish();

		pInKey = Hash.GetKey();
		uInSize = Hash.GetSize();
	}

	if(uInSize == uOutSize)
		memcpy(pOutKey, pInKey, uOutSize);
	else if(uOutSize < uInSize)
		Fold(pInKey, uInSize, pOutKey, uOutSize);
	return true;
}

void CAbstractKey::Fold(const byte* pInKey, size_t uInSize, byte* pOutKey, size_t uOutSize)
{
	//ASSERT(uOutSize <= uInSize);
	memset(pOutKey, 0, uOutSize);
	for(size_t i = 0; i < uInSize; i += uOutSize)
	{
		for(size_t j = 0; j < uOutSize && (i + j) < uInSize; j ++)
			pOutKey[j] ^= pInKey[i + j];
	}
}

CBuffer* CAbstractKey::DumpInteger(const CryptoPP::Integer& Value)
{
	size_t Size = Value.ByteCount();
	CBuffer* pBuffer = new CBuffer(Size, true);
	byte* Ptr = pBuffer->GetBuffer();
	for (size_t i=0; i < Size; i++)
		Ptr[i] = Value.GetByte(Size - (i+1)); 
	return pBuffer;
}

string CAbstractKey::Algorithm2Str(UINT eAlgorithm)
{
	if(eAlgorithm == eNone)
		return "None";
	if(eAlgorithm == eUndefined)
		return "Undefined";

	string Str;

	switch(eAlgorithm & eKeyExchange)
	{
		// Key exchange
		case 0: break;
		case eECDH:			Str += "/ECDH"; break;
		case eDH:			Str += "/DH"; break;
		case eLUCDH:		Str += "/LUCDH"; break;
		//case eDH2:			Str += "/DH2"; break;
		//case eECMQV:		Str += "/ECMQV"; break;
		//case eMQV:			Str += "/MQV"; break;
		default: ASSERT(0);
	}

	switch(eAlgorithm & eAsymCipher)
	{
		// Asymetric ciphers
		case 0: break;
		case eECP:			Str += "/ECP"; break;
		case eEC2N:			Str += "/EC2N"; break;
		
		case eRSA:			Str += "/RSA"; break;
		case eLUCX:			Str += "/LUCX"; break;
		case eRabin:		Str += "/Rabin"; break;

		case eLUC:			Str += "/LUC"; break;

		case eDL:			Str += "/DL"; break;
		default: ASSERT(0);
	}

	switch(eAlgorithm & eAsymMode)
	{
		// Asymetric modes
		case 0: break;
		case eDSA:			Str += "/DSA"; break;
		case eNR:			Str += "/NR"; break;
		case eIES:			Str += "/IES"; break;

		case ePKCS:			Str += "/PKCS"; break;
		case eOAEP:			Str += "/OAEP"; break;
		case ePSSR:			Str += "/PSSR"; break;
		case ePSS:			Str += "/PSS"; break;
		default: ASSERT(0);
	}

	switch(eAlgorithm & eSymCipher)
	{
		// Block Ciphers
		case 0: break;
		case eAES:			Str += "/AES"; break;
		case eSerpent:		Str += "/Serpent"; break;
		case eTwofish:		Str += "/Twofish"; break;
		case eMARS:			Str += "/MARS"; break;
		case eRC6:			Str += "/RC6"; break;

		// Stream Ciphers
		case eRC4:			Str += "/RC4"; break;
		case eSosemanuk:	Str += "/Sosemanuk"; break;
		case eSalsa20:		Str += "/Salsa20"; break;
		case eXSalsa20:		Str += "/XSalsa20"; break;
		default: ASSERT(0);
	}

	switch(eAlgorithm & eSymMode)
	{
		// Block Modes
		case 0: break;
		case eECB:			Str += "/ECB"; break;
		case eCBC:			Str += "/CBC"; break;
		case eCFB:			Str += "/CFB"; break;
		case eOFB:			Str += "/OFB"; break;
		case eCTR:			Str += "/CTR"; break;
		case eCTS:			Str += "/CTS"; break; // CBC_CTS
		// Authenticated Block Modes
		case eGCM:			Str += "/GCM"; break;
		case eCCM:			Str += "/CCM"; break;
		case eEAX:			Str += "/EAX"; break;
		default: ASSERT(0);
	}

	switch(eAlgorithm & eHashFunkt)
	{
		// Hash Functions
		case 0: break;
		case eSHA1:			Str += "/SHA1"; break;
		case eSHA224:		Str += "/SHA224"; break;
		case eSHA256:		Str += "/SHA256"; break;
		case eSHA384:		Str += "/SHA384"; break;
		case eSHA512:		Str += "/SHA512"; break;
		case eTiger:		Str += "/Tiger"; break;
		case eWhirlpool:	Str += "/Whirlpool"; break;
		case eRIPEMD128:	Str += "/RIPEMD128"; break;
		case eRIPEMD160:	Str += "/RIPEMD160"; break;
		case eRIPEMD256:	Str += "/RIPEMD256"; break;
		case eRIPEMD320:	Str += "/RIPEMD320"; break;
		case eMD5:			Str += "/MD5"; break;
		case eMD4:			Str += "/MD4"; break;
		default: ASSERT(0);
	}

	if(Str.empty())
		return "Undefined";
	return Str.substr(1);
}

size_t CAbstractKey::Algorithm2HashSize(UINT eAlgorithm)
{
	switch(eAlgorithm & eHashFunkt)
	{
		// Hash Functions
		case 0:				return 0;
		case eSHA1:			return KEY_160BIT;
		case eSHA224:		return KEY_224BIT;
		case eSHA256:		return KEY_256BIT;
		case eSHA384:		return KEY_384BIT;
		case eSHA512:		return KEY_512BIT;
		case eTiger:		return KEY_192BIT;
		case eWhirlpool:	return KEY_512BIT;
		case eRIPEMD128:	return KEY_128BIT;
		case eRIPEMD160:	return KEY_160BIT;
		case eRIPEMD256:	return KEY_256BIT;
		case eRIPEMD320:	return KEY_320BIT;
		default: ASSERT(0);	return 0;
	}
}

UINT CAbstractKey::Str2Algorithm(const string& Str)
{
	UINT eAlgorithm = eNone;

	vector<string> Tags = SplitStr(Str, "/", false);
	for(vector<string>::iterator I = Tags.begin(); I != Tags.end(); I++)
	{
		pair<string,string> NameSize = Split2(*I, "-");

		UINT eCur = eNone;

		// Block Ciphers
		if(NameSize.first == "AES")
			eCur = eAES;
		else if(NameSize.first == "Serpent")
			eCur = eSerpent;
		else if(NameSize.first == "Twofish")
			eCur = eTwofish;
		else if(NameSize.first == "MARS")
			eCur = eMARS;
		else if(NameSize.first == "RC6")
			eCur = eRC6;
		else 

		// Stream Ciphers
		if(NameSize.first == "RC4")
			eCur = eRC4;
		else if(NameSize.first == "Sosemanuk")
			eCur = eSosemanuk;
		else if(NameSize.first == "Salsa20")
			eCur = eSalsa20;
		else if(NameSize.first == "XSalsa20")
			eCur = eXSalsa20;
		else 

		// Block Modes
		if(NameSize.first == "ECB")
			eCur = eECB;
		else if(NameSize.first == "CBC")
			eCur = eCBC;
		else if(NameSize.first == "CFB")
			eCur = eCFB;
		else if(NameSize.first == "OFB")
			eCur = eOFB;
		else if(NameSize.first == "CTR")
			eCur = eCTR;
		else 
		
		if(NameSize.first == "CTS")
			eCur = eCTS;
		else 

		if(NameSize.first == "GCM")
			eCur = eGCM;
		else if(NameSize.first == "CCM")
			eCur = eCCM;
		else if(NameSize.first == "EAX")
			eCur = eEAX;
		else 

		if(NameSize.first == "ECP")
			eCur = eECP;
		else if(NameSize.first == "EC2N")
			eCur = eEC2N;
		else
			if(NameSize.first == "RSA")
			eCur = eRSA;
		else if(NameSize.first == "Rabin")
			eCur = eRabin;
		else if(NameSize.first == "LUCX")
			eCur = eLUCX;
		else 
			if(NameSize.first == "LUC")
			eCur = eLUC;
		else
			if(NameSize.first == "DL")
			eCur = eDL;
		else
		// Asymetric modes
		if(NameSize.first == "DSA")
			eCur = eDSA;
		else if(NameSize.first == "NR")
			eCur = eNR;
		else if(NameSize.first == "IES")
			eCur = eIES;
		else
			if(NameSize.first == "PKCS")
			eCur = ePKCS;
		else if(NameSize.first == "OAEP")
			eCur = eOAEP;
		else if(NameSize.first == "PSSR")
			eCur = ePSSR;
		else if(NameSize.first == "PSS")
			eCur = ePSS;
		else 

		// Key exchange
		if(NameSize.first == "DH")
			eCur = eDH;
		else if(NameSize.first == "ECDH")
			eCur = eECDH;
		else if(NameSize.first == "LUCDH")
			eCur = eLUCDH;
		//else if(NameSize.first == "DH2")
		//	eCur = eDH2;
		//else if(NameSize.first == "ECMQV")
		//	eCur = eECMQV;
		//else if(NameSize.first == "MQV")
		//	eCur = eMQV;
		else 

		// Hash Functions
		if(NameSize.first == "SHA1")
			eCur = eSHA1;
		else if(NameSize.first == "SHA224")
			eCur = eSHA224;
		else if(NameSize.first == "SHA256")
			eCur = eSHA256;
		else if(NameSize.first == "SHA384")
			eCur = eSHA384;
		else if(NameSize.first == "SHA512")
			eCur = eSHA512;
		else if(NameSize.first == "Tiger")
			eCur = eTiger;
		else if(NameSize.first == "Whirlpool")
			eCur = eWhirlpool;
		else if(NameSize.first == "RIPEMD128")
			eCur = eRIPEMD128;
		else if(NameSize.first == "RIPEMD160")
			eCur = eRIPEMD160;
		else if(NameSize.first == "RIPEMD256")
			eCur = eRIPEMD256;
		else if(NameSize.first == "RIPEMD320")
			eCur = eRIPEMD320;

		else 
		{
			ASSERT(0);
			return eUndefined;
		}

		eAlgorithm |= eCur;
	}

	return eAlgorithm;
}

pOID GetECCurve(const string& Curve)
{
	// Crypto++ supplies a set of standard curves approved by ANSI, Brainpool, and NIST.

	// first curves based on GF(p)
	if(Curve == "secp112r1")		return ASN1::secp112r1;
	if(Curve == "secp112r2")		return ASN1::secp112r2;
	if(Curve == "secp128r1")		return ASN1::secp128r1;
	if(Curve == "secp128r2")		return ASN1::secp128r2;
	if(Curve == "secp160r1")		return ASN1::secp160r1;
	if(Curve == "secp160k1")		return ASN1::secp160k1;
	if(Curve == "brainpoolP160r1")	return ASN1::brainpoolP160r1;
	if(Curve == "secp160r2")		return ASN1::secp160r2;
	if(Curve == "secp192k1")		return ASN1::secp192k1;
	if(Curve == "secp192r1")		return ASN1::secp192r1;			// P-192
	if(Curve == "brainpoolP192r1")	return ASN1::brainpoolP192r1;
	if(Curve == "secp224k1")		return ASN1::secp224k1;
	if(Curve == "secp224r1")		return ASN1::secp224r1;			// P-224
	if(Curve == "brainpoolP224r1")	return ASN1::brainpoolP224r1;
	if(Curve == "secp256k1")		return ASN1::secp256k1;
	if(Curve == "secp256r1")		return ASN1::secp256r1;			// P-256
	if(Curve == "brainpoolP256r1")	return ASN1::brainpoolP256r1;
	if(Curve == "brainpoolP320r1")	return ASN1::brainpoolP320r1;
	if(Curve == "secp384r1")		return ASN1::secp384r1;			// P-384
	if(Curve == "brainpoolP384r1")	return ASN1::brainpoolP384r1;
	if(Curve == "brainpoolP512r1")	return ASN1::brainpoolP512r1;
	if(Curve == "secp521r1")		return ASN1::secp521r1;			// P-521
	// then curves based on GF(2^n)
	if(Curve == "sect113r1")		return ASN1::sect113r1;
	if(Curve == "sect113r2")		return ASN1::sect113r2;
	if(Curve == "sect131r1")		return ASN1::sect131r1;
	if(Curve == "sect131r2")		return ASN1::sect131r2;
	if(Curve == "sect163k1")		return ASN1::sect163k1;			// K-163
	if(Curve == "sect163r1")		return ASN1::sect163r1;
	if(Curve == "sect163r2")		return ASN1::sect163r2;			// B-163
	if(Curve == "sect193r1")		return ASN1::sect193r1;
	if(Curve == "sect193r2")		return ASN1::sect193r2;
	if(Curve == "sect233k1")		return ASN1::sect233k1;			// K-233
	if(Curve == "sect233r1")		return ASN1::sect233r1;			// B-233
	if(Curve == "sect239k1")		return ASN1::sect239k1;
	if(Curve == "sect283k1")		return ASN1::sect283k1;			// K-283
	if(Curve == "sect283r1")		return ASN1::sect283r1;			// B-283
	if(Curve == "sect409k1")		return ASN1::sect409k1;			// K-409
	if(Curve == "sect409r1")		return ASN1::sect409r1;			// B-409
	if(Curve == "sect571k1")		return ASN1::sect571k1;			// K-571
	if(Curve == "sect571r1")		return ASN1::sect571r1;			// B-571
	return NULL;
}
