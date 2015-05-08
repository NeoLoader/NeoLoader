#pragma once
//#include "GlobalHeader.h"

#include "../Common/ValueMap.h"
class CBuffer;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CAvailDiff

class CShareMap;

struct SAvailDiff
{
	SAvailDiff(uint64 b = 0, uint64 e = 0, uint32 n = 0, uint32 o = 0) 
		: uBegin(b), uEnd(e), uNew(n), uOld(o) {}
	uint64	uBegin;
	uint64	uEnd;
	uint32	uNew;
	uint32	uOld;
};

class CAvailDiff
{
public:
	CAvailDiff();

	size_t	Count() const {return m_DiffList.size();}
	const SAvailDiff& At(int i) const {return m_DiffList.at(i);}

	void	Update(uint64 uBegin, uint64 uEnd, uint32 uNew, CShareMap* pMap, uint32 uMask);
	void	Add(uint64 uBegin, uint64 uEnd, uint32 uNew, uint32 uOld);

protected:

	QVector<SAvailDiff> m_DiffList;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CPartMap

namespace Part
{
	enum EStatus
	{
		NotAvailable		= 0x0000,	// 0000 0000 0000 0000

		Available			= 0x0001,	// 0000 0000 0000 0001	// Available on HDD, or advertized remotly (usually after verification)
		Verified			= 0x0002,	// 0000 0000 0000 0010	// downlaoded and successfuly Verified with default master hash
		Corrupt				= 0x0004,	// 0000 0000 0000 0100	// the part was hashed and considdered bad, we remember this to be mor cautious when downloading it next time
		Cached				= 0x0008,	// 0000 0000 0000 1000	// data is available in cache

		Scheduled			= 0x0010,	// 0000 0000 0001 0000	// scheduled for downlaod
		Requested			= 0x0020,	// 0000 0000 0010 0000	// requested for download
		Allocated			= 0x0040,	// 0000 0000 0100 0000
		Disabled			= 0x0080,	// 0000 0000 1000 0000	// excluded form download, used in linked part maps

		Selected			= 0x0100,	// 0000 0001 0000 0000	// selected for hoster upload
		//					= 0x0200,	// 0000 0010 0000 0000
		Marked				= 0x0400,	// 0000 0100 0000 0000	// marked for hoster download
		//					= 0x0800,	// 0000 1000 0000 0000	

		Stream				= 0x1000,	// 0001 0000 0000 0000
		Required			= 0x2000,	// 0010 0000 0000 0000
		//					= 0x4000,	// 0100 0000 0000 0000
		//					= 0x8000,	// 1000 0000 0000 0000

		//					= 0x00010000,
		//					= 0x00020000,
		//					= 0x00040000,
		//					= 0x00080000,
		//					= 0x00100000,
		//					= 0x00200000,
		//					= 0x00400000,
		//					= 0x00800000,

		//					= 0x01000000,
		//					= 0x02000000,
		//					= 0x04000000,
		//					= 0x08000000,
		//					= 0x10000000,
		//					= 0x20000000,
		//					= 0x40000000,
		//					= 0x80000000,
	};
};

#define NUM_ADD(a,b)	a += b;
#define NUM_CLR(a,b)	if(a > b) a -= b; else a = 0;

#define NUM_OR(a,b)		if(b > a) a = b; // Min
#define NUM_AND(a,b)	if(b < a) a = b; // Mxa

struct SPartRange
{
	SPartRange(UINT s = 0)
	{
		bStates = true;
		uStates = s;
		uScheduled = ((uStates & Part::Scheduled) != 0) ? 1 : 0;
		uRequested = ((uStates & Part::Requested) != 0) ? 1 : 0;
		uMarked = ((uStates & Part::Marked) != 0) ? 1 : 0;

		bPriority = true;
		iPriority = 0;
	}
	static SPartRange Priority(int iPriority)
	{
		SPartRange New;
		
		New.bStates = false;

		New.bPriority = true;
		New.iPriority = iPriority;
		
		return New;
	}
	uint8	bStates		: 1,
			bPriority	: 1;

	UINT	uStates;
	uint16	uScheduled;
	uint16	uRequested;
	uint16	uMarked;
	int		iPriority;

	UINT operator&(const UINT uValue) const {return uStates & uValue;}
};

#define STAT_UPD(a) if(a.uScheduled) a.uStates |= Part::Scheduled; else a.uStates &= ~Part::Scheduled; \
					if(a.uRequested) a.uStates |= Part::Requested; else a.uStates &= ~Part::Requested; \
					if(a.uMarked) a.uStates |= Part::Marked; else a.uStates &= ~Part::Marked;

class CPartMap: public QObject, public CRangeMap<SPartRange>
{
	Q_OBJECT

public:
	CPartMap(uint64 Size) : CRangeMap<SPartRange>(Size) {}

	/*virtual QString		GetRanges(ValueType uTest) const;
	virtual bool		SetRanges(const QString& Ranges, ValueType uState);
	virtual void		FromBitArray(const QBitArray& Array, uint64 uPartSize, ValueType uState, CAvailDiff& AvailDiff);
	//virtual void		FromBitArray(const QBitArray& Array, uint64 uPartSize, ValueType uState);*/
	virtual QBitArray	ToBitArray(uint64 uPartSize, uint64 uBlockSize, ValueType uTest) const;

	virtual QVariantMap	Store();
	virtual bool		Load(const QVariantMap& Map);

	virtual void		NotifyChange(bool bPurge = false)	{emit Change(bPurge);}

signals:
	void				Change(bool bPurge = false);

protected:
	virtual bool		StateSet(ValueType uCur) const
	{
		return uCur.uStates || uCur.uScheduled || uCur.uRequested || uCur.uMarked || uCur.iPriority;
	}

	virtual	bool		MatchState(ValueType uCur, ValueType uState) const
	{
		return uCur.uStates == uState.uStates && uCur.uScheduled == uState.uScheduled && uCur.uRequested == uState.uRequested && uCur.uMarked == uState.uMarked &&uCur.iPriority == uState.iPriority;
	}

	virtual	ValueType	MakeState(ValueType uCur, ValueType uState, ESet eMode) const
	{
		switch(eMode)
		{
		case eSet:			
			if(uState.bStates)
			{
				uCur.uStates = uState.uStates;
				uCur.uScheduled = uState.uScheduled;
				uCur.uRequested = uState.uRequested;
				uCur.uMarked = uState.uMarked;
			}
			if(uState.bPriority)
				uCur.iPriority = uState.iPriority;
			return uCur;
		case eAdd:			//return uCur | uState;
			if(uState.bStates)
			{
				uCur.uStates |= uState.uStates;
				NUM_ADD(uCur.uScheduled, uState.uScheduled);
				NUM_ADD(uCur.uRequested, uState.uRequested);
				NUM_ADD(uCur.uMarked, uState.uMarked);
				STAT_UPD(uCur);
			}
			if(uState.bPriority)
			{
				NUM_ADD(uCur.iPriority, uState.iPriority);
			}
			return uCur;
		case eClr:			//return uCur & ~uState;
			if(uState.bStates)
			{
				uCur.uStates &= ~uState.uStates;
				NUM_CLR(uCur.uScheduled, uState.uScheduled);
				NUM_CLR(uCur.uRequested, uState.uRequested);
				NUM_CLR(uCur.uMarked, uState.uMarked);
				STAT_UPD(uCur);
			}
			if(uState.bPriority)
			{
				NUM_CLR(uCur.iPriority, uState.iPriority);
			}
			return uCur;
		default:			
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual	ValueType	MergeState(ValueType uCur, ValueType uState, EMerge eMode) const
	{
		switch(eMode)
		{
		case eUnion:		//return uCur | uState;		// take all bits set in all ranges
			uCur.uStates |= uState.uStates;
			NUM_OR(uCur.uScheduled, uState.uScheduled);
			NUM_OR(uCur.uRequested, uState.uRequested);
			NUM_OR(uCur.uMarked, uState.uMarked);
			STAT_UPD(uCur);
			NUM_OR(uCur.iPriority, uState.iPriority);
			return uCur;
		case eInter:		//return uCur & uState;		// take only those bits set in all ranges
			uCur.uStates &= uState.uStates;
			NUM_AND(uCur.uScheduled, uState.uScheduled);
			NUM_AND(uCur.uRequested, uState.uRequested);
			NUM_AND(uCur.uMarked, uState.uMarked);
			STAT_UPD(uCur);
			NUM_AND(uCur.iPriority, uState.iPriority);
			return uCur;
		default:			
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual QString		State2Str(ValueType uCur) const
	{
		QString Out;
		Out = QString("s%1/r%2/m%3/p%4/").arg(uCur.uScheduled).arg(uCur.uRequested).arg(uCur.uMarked).arg(uCur.iPriority);

		uint32 Int = uCur.uStates;

		for(int i=0; i < 32; i++)
		{
			Out += (Int & 1) ? "1" : "0";
			Int >>= 1;
		}

		return Out;
	}
};

typedef QSharedPointer<CPartMap> CPartMapPtr;
typedef QWeakPointer<CPartMap> CPartMapRef;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CShareMap

class CShareMap: public QObject, public CValueMap<uint16>
{
	Q_OBJECT

public:
	CShareMap(uint64 Size) : CValueMap<uint16>(Size) {}

	virtual QString		GetRanges(ValueType uTest) const;
	virtual bool		SetRanges(const QString& Ranges, ValueType uState);
	virtual uint64		FromBitArray(const QBitArray& Array, uint64 uPartSize, uint64 uBlockSize, ValueType uState, CAvailDiff& AvailDiff);
	//virtual void		FromBitArray(const QBitArray& Array, uint64 uPartSize, ValueType uState);

	virtual QVariantMap	Store();
	virtual bool		Load(const QVariantMap& Map);

	static QByteArray	Bits2Bytes(const QBitArray& Bits, bool bED2kStyle = false);
	static QBitArray	Bytes2Bits(const QByteArray& Bytes, int PartCount, bool bED2kStyle = false);

protected:
};

typedef QSharedPointer<CShareMap> CShareMapPtr;
typedef QWeakPointer<CPartMap> CPartMapRef;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CSyncPartMap

template <class T>
class CSynced: public T
{
public:
	CSynced(uint64 Size) : T(Size) {}

	virtual void		Reset(uint64 Size = -1)																		{QMutexLocker Locker(&m_Mutex);	return T::Reset(Size);								}
	virtual uint64		GetSize() const																				{QMutexLocker Locker(&m_Mutex);	return T::GetSize();									}
    virtual void		SetRange(uint64 uBegin, uint64 uEnd, typename T::ValueType uState, typename T::ESet eMode = T::eSet)					{QMutexLocker Locker(&m_Mutex);	T::SetRange(uBegin, uEnd, uState, eMode);			}
    virtual typename T::ValueType	GetRange(uint64 uBegin, uint64 uEnd, typename T::EMerge eMode = T::eInter, typename T::ValueType uMask = 0) const		{QMutexLocker Locker(&m_Mutex);	return T::GetRange(uBegin, uEnd, eMode, uMask);		}
	virtual bool		GetNextRange(uint64 &uBegin, uint64 &uEnd, typename T::ValueType &uState, typename T::ValueType uMask = 0, typename T::SIterHint** pHint = NULL) const	{QMutexLocker Locker(&m_Mutex);	return T::GetNextRange(uBegin, uEnd, uState, uMask, pHint);	}

	virtual QVariantMap	Store()																						{QMutexLocker Locker(&m_Mutex); return T::Store();}
	virtual bool		Load(const QVariantMap& Map)																{QMutexLocker Locker(&m_Mutex); return T::Load(Map);}

protected:
	mutable QMutex		m_Mutex;
};

typedef CSynced<CPartMap> CSyncPartMap;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CLinkedPartMap

struct SPartMapLink
{
	uint64 uShareBegin;
	uint64 uShareEnd;
	uint64 ID;
	CPartMapRef pMap;
};

class CLinkedPartMap: public CSyncPartMap
{
	Q_OBJECT

public:
	CLinkedPartMap(uint64 Size) : CSyncPartMap(Size) {}
	virtual ~CLinkedPartMap() 
	{
		foreach(SPartMapLink* pLink, m_Links)
			delete pLink;
	}

	struct SIterHintEx: SIterHint
	{
		virtual ~SIterHintEx()
		{
			foreach(SIterHint* pHint, Hint)
				CLinkedPartMap::FreeHint(pHint);
		}

		//QMapEx<void*,SIterHint*> Hint;
		QMap<void*,SIterHint*> Hint;
	};

	virtual bool		EnableLink(uint64 ID, CPartMapPtr& pMap);
	virtual void		SetupLink(uint64 uShareBegin, uint64 uShareEnd, uint64 ID);
	virtual QMap<uint64, SPartMapLink*> GetLinks() const {return m_Links;}
	virtual void		BreakLink(uint64 ID);

	// *RawRange returns only the stat of the actual part map ignoring other linked maps
	virtual ValueType	GetRawRange(uint64 uBegin, uint64 uEnd, EMerge eMode = eInter, ValueType uMask = 0) const	{return CSyncPartMap::GetRange(uBegin, uEnd, eMode, uMask);}
	virtual bool		GetNextRawRange(uint64 &uBegin, uint64 &uEnd, ValueType &uState, ValueType uMask = 0, SIterHint** pHint = NULL) const	{return CSyncPartMap::GetNextRange(uBegin, uEnd, uState, uMask, pHint);}

	virtual QVariantMap	Store();
	virtual bool		Load(const QVariantMap& Map);
	static CSyncPartMap*Restore(const QVariantMap& Map);

	virtual void		NotifyChange(bool bPurge = false);

protected:
	virtual QString		GetType() = 0;

	virtual void		AddLink(SPartMapLink* pLink) 
	{
		delete m_Links.value(pLink->ID);
		m_Links.insert(pLink->ID, pLink);
	}

	QMap<uint64, SPartMapLink*>m_Links;

	mutable QReadWriteLock	m_Locker;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CJoinedPartMap

class CJoinedPartMap: public CLinkedPartMap
{
	Q_OBJECT

public:
	CJoinedPartMap(uint64 Size) : CLinkedPartMap(Size) {}
	virtual ~CJoinedPartMap() 
	{
		foreach(SPartMapLink* pLink, m_Joints)
		{
			if(pLink->ID == 0)
				delete pLink;
		}
	}

	virtual bool		HasLinks() const {QReadLocker Locker(&m_Locker); return !m_Joints.isEmpty();}
	virtual void		BreakLink(uint64 ID);
	virtual QMap<uint64, SPartMapLink*> GetJoints() const {QReadLocker Locker(&m_Locker); return m_Joints;}

	virtual void		SetSharedRange(uint64 uBegin, uint64 uEnd, ValueType uState, ESet eMode = eSet);

	virtual ValueType	GetRange(uint64 uBegin, uint64 uEnd, EMerge eMode = eInter, ValueType uMask = 0) const;
	virtual bool		GetNextRange(uint64 &uBegin, uint64 &uEnd, ValueType &uState, ValueType uMask = 0, SIterHint** pHint = NULL) const;

	virtual QList<SPartMapLink*>SelectLinks(uint64 &uBegin, uint64 &uEnd) const;

	static	bool		CheckType(const QString& Type) {return Type == "Joined";}
protected:
	virtual QString		GetType() {return "Joined";}

	virtual void		AddLink(SPartMapLink* pLink )
	{
		if(pLink->ID)
			CLinkedPartMap::AddLink(pLink);

		if(SPartMapLink* pOldLink = m_Joints.value(ULLONG_MAX - pLink->uShareBegin))
		{
			if(pOldLink->ID == 0)
				delete pOldLink;
		}
		m_Joints.insert(ULLONG_MAX - pLink->uShareBegin, pLink);
	}

	QMap<uint64,SPartMapLink*>	m_Joints;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CSharedPartMap

class CSharedPartMap: public CLinkedPartMap
{
	Q_OBJECT

public:
	CSharedPartMap(uint64 Size) : CLinkedPartMap(Size) {}
	virtual ~CSharedPartMap() {}

	virtual void		SetRange(uint64 uBegin, uint64 uEnd, ValueType uState, ESet eMode = eSet);
	virtual void		SetJoinedRange(uint64 uBegin, uint64 uEnd, ValueType uState, ESet eMode = eSet);

	virtual ValueType	GetRange(uint64 uBegin, uint64 uEnd, EMerge eMode = eInter, ValueType uMask = 0) const;
	virtual bool		GetNextRange(uint64 &uBegin, uint64 &uEnd, ValueType &uState, ValueType uMask = 0, SIterHint** pHint = NULL) const;

	static	bool		CheckType(const QString& Type) {return Type == "Shared";}
protected:
	virtual QString		GetType() {return "Shared";}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CShareMap
/*
class CShareMap: public CValueMap<uint8>
{
public:
	CShareMap(uint64 Size) : CValueMap<uint8>(Size) {}

	virtual void		From(const CPartMap* pParts);
	virtual void		To(CPartMap* pParts, CAvailDiff& AvailDiff) const;

	virtual bool		Read(const CBuffer* Data);
	virtual void		Write(CBuffer* Data, bool bI64 = true) const;
	virtual bool		LoadBin(const QByteArray& Array);
	virtual QByteArray	SaveBin(bool bI64 = true) const;

	enum EStatus
	{
		eNotAvailable	= 0,	// 0000 0000 0000 0000

		eAvailable		= 1,	// 0000 0000 0000 0001	// Available on HDD, or advertized remotly (usually after verification)
		eNeoVerified	= 2,	// 0000 0000 0000 0010	// downlaoded and successfuly Verified with Neo Has
		eEd2kVerified	= 4,	// 0000 0000 0000 0100	// downlaoded and successfuly Verified with ed2k Hash 
		eMuleVerified	= 8,	// 0000 0000 0000 1000	// downlaoded and successfuly Verified with Mule Hash 

		//				= 16,	// 0000 0000 0001 0000
		//				= 32,	// 0000 0000 0010 0000
		//				= 64,	// 0000 0000 0100 0000
		//				= 128,	// 0000 0000 1000 0000
	};
};
*/
