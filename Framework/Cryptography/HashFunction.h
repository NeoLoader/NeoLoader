#pragma once

#include "AbstractKey.h"
#include "../Buffer.h"

class NEOHELPER_EXPORT CHashFunction: public CAbstractKey
{
public:
	CHashFunction(UINT eAlgorithm);
	virtual ~CHashFunction();

	static CryptoPP::HashFunction*	GetHashFunction(UINT eAlgorithm);

	virtual bool			IsValid()						{return m_pFunction != NULL;}

	virtual void			Reset();
	virtual bool			Add(const byte* pData, size_t uLen);
	virtual bool			Add(CBuffer* pPayload)			{return Add(pPayload->GetBuffer(),pPayload->GetSize());}
#ifdef USING_QT
	virtual bool			Add(const QByteArray &Data)		{return Add((byte*)Data.data(),Data.size());}
#endif
	virtual bool			Finish();

	virtual void			Fold(byte* pOutKey, size_t uOutSize);
#ifdef USING_QT
	virtual QByteArray		Folded(size_t uOutSize)			{QByteArray Data(uOutSize, 0); Fold((byte*)Data.data(), uOutSize); return Data;}
#endif

	virtual byte*			GetKey()						{return m_pKey;}

#ifdef USING_QT
	static QByteArray		Hash(const QByteArray &Data, UINT eAlgorithm);
#endif

protected:

	CryptoPP::HashFunction*	m_pFunction;
};
