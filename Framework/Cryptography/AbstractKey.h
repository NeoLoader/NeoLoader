#pragma once

#include "../NeoHelper/neohelper_global.h"

//#ifdef __APPLE__
#ifdef _std_fix_
#undef map
#undef multimap
#include "../../crypto++/dll.h"
#define map _map_fix_
#define multimap _multimap_fix_
#else
#include "../../crypto++/dll.h"
#endif

// Key Bits to Key Bytes
#define KEY_32BIT	4
#define KEY_64BIT	8
#define KEY_128BIT	16
#define KEY_160BIT	20
#define KEY_192BIT	24
#define KEY_224BIT	28
#define KEY_256BIT	32
#define KEY_320BIT	40
#define KEY_384BIT	48
#define KEY_512BIT	64
#define KEY_1024BIT	128
#define KEY_2048BIT	256
#define KEY_4096BIT	512
#define KEY_8192BIT	1024
//

#include "../Buffer.h"

class NEOHELPER_EXPORT CAbstractKey
{
public:
	CAbstractKey();
	CAbstractKey(size_t uSize, bool bRand = false);
	CAbstractKey(byte* pKey, size_t uSize, UINT eAlgorithm = 0);
	virtual ~CAbstractKey();

	// Note: this values are for internal use only, thair meanign may change between versions, use always the strings
	enum ECrypto
	{
		// Block Ciphers
		eAES		= 0x00000001,
		eSerpent	= 0x00000002, 
		eTwofish	= 0x00000003, 
		eMARS		= 0x00000004, 
		eRC6		= 0x00000005, 

		// Block Modes
		eECB		= 0x00000010,
		eCBC		= 0x00000020,
		eCFB		= 0x00000030,
		eOFB		= 0x00000040, 
		eCTR		= 0x00000050,	// counter mode
		eCTS		= 0x00000060,	// CBC ciphertext stealing
		// Authenticated Block Modes
		eCCM		= 0x000000A0,
		eGCM		= 0x000000B0,
		eEAX		= 0x000000C0,

		// Stream Ciphers
		eSosemanuk	= 0x0000000A,
		eSalsa20	= 0x0000000B,
		eXSalsa20	= 0x0000000C,
		eWeakRC4	= 0x0000000D,	// for internal use only not convertible into a string
		eRC4		= 0x0000000E,

		// Asymetric ciphers
		eECP		= 0x00000100,
		eEC2N		= 0x00000200,

		eRSA		= 0x00000300,
		eRabin		= 0x00000400,
		eLUCX		= 0x00000500,

		eLUC		= 0x00000600,	// no specific modes

		eDL			= 0x00000700,

		// Asymetric modes
		eDSA		= 0x00001000,
		eNR			= 0x00002000,
		eIES		= 0x00003000,

		ePKCS		= 0x00004000,
		eOAEP		= 0x00005000,
		ePSSR		= 0x00006000,
		ePSS		= 0x00007000,

		// Key exchange
		eECDH		= 0x00010000,	// requirers eECP or eEC2N
		eDH			= 0x00020000,
		eLUCDH		= 0x00030000,
		// Authenticated Key exchange
		//eDH2		= 0x00040000,
		//eECMQV		= 0x00050000,	// requirers eECP or eEC2N
		//eMQV		= 0x00060000,

		// Hash Functions
		eSHA1		= 0x00100000,
		eSHA224		= 0x00200000,
		eSHA256		= 0x00300000,
		eSHA384		= 0x00400000,
		eSHA512		= 0x00500000,
		eTiger		= 0x00600000,
		eWhirlpool	= 0x00700000,
		eRIPEMD128	= 0x00800000,
		eRIPEMD160	= 0x00900000,
		eRIPEMD256	= 0x00A00000,
		eRIPEMD320	= 0x00B00000,
		eMD5		= 0x00C00000,
		eMD4		= 0x00D00000,	// WARNING: This is Broken, for legacy compatibility only
		eCRC		= 0x00E00000,	// just a check summ

		// Markers and Masks
		eUndefined	= 0xFFFFFFFF,

		eSymCipher	= 0x0000000F,
		eSymMode	= 0x000000F0,
		eAsymCipher	= 0x00000F00,
		eAsymMode	= 0x0000F000,
		eKeyExchange= 0x000F0000,
		eHashFunkt	= 0x00F00000,
		//e			= 0x0F000000,
		//e			= 0xF0000000,

		eNone		= 0x00000000
	};

	virtual byte*			GetKey() const						{return m_pKey;}
	virtual size_t			GetSize() const						{return m_uSize;}
	virtual void			SetAlgorithm(UINT eAlgorithm)		{m_eAlgorithm = eAlgorithm;}
	virtual UINT			GetAlgorithm()	const				{return m_eAlgorithm;}
	//virtual string			GetAlgorithmStr()					{return GetAlgorithmStr(GetAlgorithm());}
	static	string			Algorithm2Str(UINT eAlgorithm);
	static	UINT			Str2Algorithm(const string& Str);
	static	size_t			Algorithm2HashSize(UINT eAlgorithm);
	virtual void			AllocKey(size_t uSize);
	virtual bool			SetupKey(byte* pKey, size_t uSize);
	virtual void			FreeKey();
	virtual bool			SetKey(byte* pKey, size_t uSize);
	virtual bool			SetKey(CAbstractKey* pKey)			{return SetKey(pKey->GetKey(),pKey->GetSize());}
#ifdef USING_QT
	virtual bool			SetKey(const QByteArray& Key)		{return SetKey((byte*)Key.data(), Key.size());}
	virtual QByteArray		ToByteArray()						{return QByteArray((char*)GetKey(), (int)GetSize());}
#endif
	virtual bool			CompareTo(const CAbstractKey* Key) const;

	static byte*			GenerateRandomKey(size_t uSize);
	static bool				AdjustKey(const byte* pInKey, size_t uInSize, byte* pOutKey, size_t uOutSize, UINT eAlgorithm = eSHA256);
	static void				Fold(const byte* pInKey, size_t uInSize, byte* pOutKey, size_t uOutSize);

	static CBuffer*			DumpInteger(const CryptoPP::Integer& Value);

protected:
	byte*					m_pKey;
	size_t					m_uSize;
	UINT					m_eAlgorithm;
};

typedef CryptoPP::OID (*pOID)(void);
pOID NEOHELPER_EXPORT GetECCurve(const string& Curve);
