#pragma once

#include "AbstractKey.h"

class NEOHELPER_EXPORT CCryptoKey: public CAbstractKey
{
public:
	CCryptoKey(UINT eAlgorithm);
	virtual ~CCryptoKey() {}

	virtual void			Reset()										{}
	virtual bool			Setup(byte* pIV = NULL, size_t uSize = 0) = 0;
	virtual bool			Seek(uint64 uPos)							{ASSERT(0); return false;}
	virtual bool			Discard(uint64 uPos)						{ASSERT(0); return false;}
#ifdef USING_QT
	virtual bool			Setup(const QByteArray& IV)					{return Setup((byte*)IV.data(),IV.size());}
	virtual bool			Process(QByteArray* pIn, QByteArray* pOut = NULL);
#endif
	virtual bool			Process(CBuffer* pIn, CBuffer* pOut = NULL);
	virtual bool			Process(byte* pIn, byte* pOut, size_t uSize) = 0;

	virtual bool			AdjustKey(const byte* pInKey, size_t uInSize, byte* pOutKey, size_t uOutSize);

	virtual	size_t			GetIVSize() = 0;
};

//////////////////////////////////////////////////////////////////////////
//

class NEOHELPER_EXPORT CEncryptionKey: public CCryptoKey
{
public:
	CEncryptionKey(UINT eAlgorithm) : CCryptoKey(eAlgorithm) {}

	virtual CAbstractKey*	Finish(size_t uSize = 0)					{ASSERT(0); return NULL;}

	virtual CAbstractKey*	GetStolenIV()								{ASSERT(0); return NULL;}

	static	CEncryptionKey*	Make(UINT eAlgorithm);

#ifdef USING_QT
	static QByteArray		Encrypt(QByteArray Data, UINT eAlgorithm, const QByteArray& Key, QByteArray& IV); // Note: IV may get changed
#endif

protected:
	template <class C>
	static	CEncryptionKey*	MakeM(UINT eAlgorithm);
};

//////////////////////////////////////////////////////////////////////////
//

class NEOHELPER_EXPORT CDecryptionKey: public CCryptoKey
{
public:
	CDecryptionKey(UINT eAlgorithm) : CCryptoKey(eAlgorithm) {}

	virtual bool			Verify(CAbstractKey* pMac)					{ASSERT(0); return false;}

	static	CDecryptionKey*	Make(UINT eAlgorithm);

#ifdef USING_QT
	static QByteArray		Decrypt(QByteArray Data, UINT eAlgorithm, const QByteArray& Key, const QByteArray& IV);
#endif

protected:
	template <class C>
	static	CDecryptionKey*	MakeM(UINT eAlgorithm);
};

//////////////////////////////////////////////////////////////////////////
//

class NEOHELPER_EXPORT CSymmetricKey: public CAbstractKey
{
public:
	CSymmetricKey(UINT eAlgorithm);
	virtual ~CSymmetricKey();

	virtual void			Reset()				{delete m_pEncryptor; m_pEncryptor = NULL; delete m_pDecryptor; m_pDecryptor = NULL;}
	virtual bool			Seek(uint64 uPos)	{bool bOK = SeekEncryption(uPos); bOK &= SeekDecryption(uPos); return bOK;}

	virtual bool			SetupEncryption(byte* pIV = NULL, size_t uSize = 0);
	virtual bool			SeekEncryption(uint64 uPos)					{return m_pEncryptor ? m_pEncryptor->Seek(uPos) : false;}
	virtual bool			DiscardEncryption(uint64 uPos)				{return m_pEncryptor ? m_pEncryptor->Discard(uPos) : false;}
#ifdef USING_QT
	virtual bool			SetupEncryption(const QByteArray& IV)		{return SetupEncryption((byte*)IV.data(),IV.size());}
	virtual bool			Encrypt(QByteArray* pIn, QByteArray* pOut = NULL)	{return m_pEncryptor ? m_pEncryptor->Process(pIn, pOut) : false;}
#endif
	virtual bool			Encrypt(CBuffer* pIn, CBuffer* pOut = NULL)		{return m_pEncryptor ? m_pEncryptor->Process(pIn, pOut) : false;}
	virtual bool			Encrypt(byte* pIn, byte* pOut, size_t uSize)	{return m_pEncryptor ? m_pEncryptor->Process(pIn, pOut, uSize) : false;}

	virtual bool			SetupDecryption(byte* pIV = NULL, size_t uSize = 0);
	virtual bool			SeekDecryption(uint64 uPos)					{return m_pDecryptor ? m_pDecryptor->Seek(uPos) : false;}
	virtual bool			DiscardDecryption(uint64 uPos)				{return m_pDecryptor ? m_pDecryptor->Discard(uPos) : false;}
#ifdef USING_QT
	virtual bool			SetupDecryption(const QByteArray& IV)		{return SetupDecryption((byte*)IV.data(),IV.size());}
	virtual bool			Decrypt(QByteArray* pIn, QByteArray* pOut = NULL)	{return m_pDecryptor ? m_pDecryptor->Process(pIn, pOut) : false;}
#endif
	virtual bool			Decrypt(CBuffer* pOut, CBuffer* pIn = NULL)		{return m_pDecryptor ? m_pDecryptor->Process(pIn, pOut) : false;}
	virtual bool			Decrypt(byte* pIn, byte* pOut, size_t uSize)	{return m_pDecryptor ? m_pDecryptor->Process(pIn, pOut, uSize) : false;}

protected:
	CEncryptionKey*			m_pEncryptor;
	CDecryptionKey*			m_pDecryptor;
};

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
class NEOHELPER_EXPORT CBlockKey: public T
{
public:
	CBlockKey(UINT eAlgorithm);
	virtual ~CBlockKey();

	virtual bool			SetKey(byte* pKey, size_t uSize);

	virtual void			Reset();
	virtual bool			Setup(byte* pIV, size_t uSize);
	virtual bool			Process(byte* pIn, byte* pOut, size_t uSize);
	virtual bool			Seek(uint64 uPos);

	virtual	size_t			GetIVSize()													{return P().IVSize();}

protected:
	P*						m_pProcessor;
};

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class E>
class NEOHELPER_EXPORT CAuthEnc: public CBlockKey<T, C, E>
{
public:
	CAuthEnc(UINT eAlgorithm);
	virtual ~CAuthEnc();

	virtual CAbstractKey*	Finish(size_t uSize = 0);

protected:
	CAbstractKey*			m_pMac;
};

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class D>
class NEOHELPER_EXPORT CAuthDec: public CBlockKey<T, C, D>
{
public:
	CAuthDec(UINT eAlgorithm) : CBlockKey<T, C, D>(eAlgorithm) {}

	virtual bool			Verify(CAbstractKey* pMac);
};

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
class NEOHELPER_EXPORT CCTSEnc: public CBlockKey<T, C, P>
{
public:
	CCTSEnc(UINT eAlgorithm);
	virtual ~CCTSEnc();

	virtual void			Reset();

	virtual bool			Seek(uint64 uPos)									{ASSERT(0); return false;}
	virtual bool			Process(byte* pIn, byte* pOut, size_t uSize);

	virtual CAbstractKey*	GetStolenIV() {return m_pStolenIV;}

protected:
	CAbstractKey*			m_pStolenIV;
};

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
class NEOHELPER_EXPORT CCTSDec: public CBlockKey<T, C, P>
{
public:
	CCTSDec(UINT eAlgorithm) : CBlockKey<T,C,P>(eAlgorithm) {}

	virtual bool			Seek(uint64 uPos)									{ASSERT(0); return false;}
	virtual bool			Process(byte* pIn, byte* pOut, size_t uSize);
};

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
class NEOHELPER_EXPORT CStreamKey: public CBlockKey<T, C, P>
{
public:
	CStreamKey(UINT eAlgorithm) : CBlockKey<T, C, P>(eAlgorithm) {}

	virtual bool			Discard(uint64 uToGo);
};

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
class NEOHELPER_EXPORT CStreamKeyEx: public CStreamKey<T, C, P>
{
public:
	CStreamKeyEx(UINT eAlgorithm);
	~CStreamKeyEx();

	virtual void			Reset();

	virtual bool			Setup(byte* pIV, size_t uSize);
	virtual bool			Process(byte* pIn, byte* pOut, size_t uSize);
	virtual bool			Seek(uint64 uPos);

protected:
	virtual void			SetIV(byte* pIV, size_t uSize);
	byte*					m_pIV;
	size_t					m_uIVSize;
	uint64					m_uPosition;
};

//////////////////////////////////////////////////////////////////////////
//

template <class T, class C, class P>
class NEOHELPER_EXPORT CRC4Key: public CStreamKeyEx<T, C, P>
{
public:
	CRC4Key(UINT eAlgorithm) : CStreamKeyEx<T, C, P>(eAlgorithm) {}

	virtual bool			Setup(byte* pIV, size_t uSize);
};
