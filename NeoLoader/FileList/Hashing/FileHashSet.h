#pragma once

#include "FileHash.h"

class CFileHashSet: public CFileHashEx
{
	Q_OBJECT

public:
	CFileHashSet(EFileHashType eType, uint64 TotalSize, uint64 PartSize = -1);
	virtual ~CFileHashSet();

	virtual uint64				GetPartSize()	{return m_PartSize;}
	virtual uint32				GetPartCount();

	virtual bool				CanHashParts()	{QReadLocker Locker(&m_SetMutex); return !m_HashSet.isEmpty();}

	//virtual QList<SPart>		GetRanges(uint64 uBegin, uint64 uEnd, bool bEnvelope = true);
	virtual bool				IsComplete()	{return IsValid() && CanHashParts();}
	virtual bool				IsLoaded()		{QReadLocker Locker(&m_SetMutex); return !m_HashSet.isEmpty();}
	virtual void				Unload();
	virtual bool				SetHashSet(const QList<QByteArray>& HashSet);
	virtual QList<QByteArray>	GetHashSet();

	virtual uint64				GetTotalSize()	{return m_TotalSize;}

	virtual CFileHash*			Clone(bool bFull);

	////////////////////////////////////////////////////////////////////////////////
	// operative part
	virtual bool				Verify(QIODevice* pFile, CPartMap* pPartMap, uint64 uFrom, uint64 uTo, EHashingMode Mode);
	virtual bool 				Calculate(QIODevice* pFile);

	virtual QByteArray			SaveBin();
	virtual bool 				LoadBin(const QByteArray& Array);

protected:
	virtual uint64				GetPartSize(int Index);

	virtual byte*				CalculatePart(QIODevice* pFile, uint64 uBegin, uint64 uEnd);
	virtual bool				CalculateRoot();

	uint64						m_PartSize;
	QVector<byte*>				m_HashSet;

	mutable QReadWriteLock		m_SetMutex;
};
