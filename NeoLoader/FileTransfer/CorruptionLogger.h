#pragma once

#include "../FileList/Hashing/FileHash.h"
#include "../FileList/File.h"
#include "../../Framework/ObjectEx.h"
#include "../../Framework/Address.h"

#ifndef __APPLE__
#include <array>
#else
#include <vector>
class array_8: public std::vector<unsigned char>
{
public:
	array_8() {resize(8);}
};
#endif

struct SRecord
{
	enum EStatus
	{
		eNone = 0,	// this is not a valid record, its NULL
		eUnknown,	// the status is not known yet
		eVerified,	// the verification was successfull
		eCorrupted,	// the verification failed
		eAligned	// Note: eAligned is not valid as a value in the map but as a state for the iterator, it forces ignoring of the IDs.
	};

	SRecord(EStatus Status = eNone) : eStatus(Status) {}
	SRecord(const QByteArray& ID) : eStatus(eUnknown) {this->ID = ID;}

	EStatus eStatus;
	QByteArray ID;
};

class CRecordMap: public CRangeMap<SRecord>
{
public:
	CRecordMap(uint64 Size)
	: CRangeMap<SRecord>(Size) {}

protected:
	virtual bool		StateSet(ValueType Cur) const
	{
		return Cur.eStatus != SRecord::eNone;
	}

	virtual	bool		MatchState(ValueType Cur, ValueType State) const
	{
		return Cur.eStatus == State.eStatus && Cur.ID == State.ID;
	}

	virtual	ValueType	MakeState(ValueType Cur, ValueType State, ESet eMode) const
	{
		switch(eMode)
		{
		case eSet:			return State;
		//case eAdd:			//return Cur | State;
		//case eClr:			//return Cur & ~State;
		default:
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual	ValueType	MergeState(ValueType Cur, ValueType State, EMerge eMode) const
	{
		switch(eMode)
		{
		//case eUnion:		//return Cur | State;		// take all bits set in all ranges
		case eInter:		//return Cur & State;		// take only those bits set in all ranges
			{
				ASSERT(State.eStatus == SRecord::eAligned);
				ValueType New;
				New.eStatus = Cur.eStatus;
				return New;
			}
		default: 
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual QString		State2Str(ValueType Cur) const
	{
		return Cur.ID + ((Cur.eStatus == SRecord::eCorrupted) ? "-" : "+");
	}
};

class CCorruptionLogger: public QObjectEx
{
	Q_OBJECT

public:
	CCorruptionLogger(CFileHashPtr pHash, uint64 uFileSize, QObject* qObject = NULL);

	//CFile*				GetFile() const			{CFile* pFile = qobject_cast<CFile*>(parent()->parent()); ASSERT(pFile); return pFile;}

	void				Record(uint64 uBegin, uint64 uEnd, const QPair<const byte*, size_t>& ID);
	void				Evaluate();

	bool				IsDropped(const QPair<const byte*, size_t>& ID) const;

protected:
	CFileHashPtr		m_pHash;

	CRecordMap			m_Records;

	struct SSuspect
	{
		SSuspect() 
		: uVerified(0)
		, uCorrupted(0)
		, uClearTime(0) 
		, uDropped(false)
		{}

		uint64 uVerified;
		uint64 uCorrupted;
		uint64 uClearTime;
		bool uDropped;
	};

#ifdef __APPLE__
	typedef array_8 TID;
#else
	typedef std::array<unsigned char, 8> TID;
#endif

	QMap<TID, SSuspect>	m_Suspects;
};