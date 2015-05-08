#pragma once

#include "PartMap.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CAvailMap

class CAvailMap: public CValueMap<uint32>
{
public:
	CAvailMap(uint64 Size) : CValueMap<uint32>(Size) {}

protected:

	virtual	ValueType	MakeState(ValueType uCur, ValueType uState, ESet eMode) const
	{
		switch(eMode)
		{
		case eSet:			return uState;
		case eAdd:			//return uCur | uState;
			NUM_ADD(uCur, uState);
			return uCur;
		case eClr:			//return uCur & ~uState;
			NUM_CLR(uCur, uState);
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
			NUM_OR(uCur, uState);
			return uCur;
		case eInter:		//return uCur & uState;		// take only those bits set in all ranges
			NUM_AND(uCur, uState);
			return uCur;
		default:			
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}
};

typedef QSharedPointer<CAvailMap> CAvailMapPtr;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CCacheMap

struct SCacheRange
{
	SCacheRange(uint32 a = 0, uint32 n = 0)
	: uAvail(a), uNeed(n) {}
	uint32 uAvail;
	uint32 uNeed;
};

class CCacheMap: public CRangeMap<SCacheRange>
{
public:
	CCacheMap(uint64 Size)
	: CRangeMap<SCacheRange>(Size) {}

protected:

	virtual bool		StateSet(ValueType Cur) const
	{
		return Cur.uAvail || Cur.uNeed;
	}

	virtual	bool		MatchState(ValueType Cur, ValueType State) const
	{
		return Cur.uAvail == State.uAvail && Cur.uNeed == State.uNeed;
	}

	virtual	ValueType	MakeState(ValueType Cur, ValueType State, ESet eMode) const
	{
		switch(eMode)
		{
		case eSet:			return State;
		case eAdd:			//return uCur | uState;
			NUM_ADD(Cur.uAvail, State.uAvail);
			NUM_ADD(Cur.uNeed, State.uNeed);
			return Cur;
		case eClr:			//return uCur & ~uState;
			NUM_CLR(Cur.uAvail, State.uAvail);
			NUM_CLR(Cur.uNeed, State.uNeed);
			return Cur;
		default:			
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual	ValueType	MergeState(ValueType Cur, ValueType State, EMerge eMode) const
	{
		switch(eMode)
		{
		case eUnion:		//return uCur | uState;		// take all bits set in all ranges
			NUM_OR(Cur.uAvail, State.uAvail);
			NUM_OR(Cur.uNeed, State.uNeed);
			return Cur;
		case eInter:		//return uCur & uState;		// take only those bits set in all ranges
			NUM_AND(Cur.uAvail, State.uAvail);
			NUM_AND(Cur.uNeed, State.uNeed);
			return Cur;
		default:			
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual QString		State2Str(ValueType Cur) const
	{
		return QString("%1/%2").arg(Cur.uAvail).arg(Cur.uNeed);
	}
};

typedef QSharedPointer<CCacheMap> CCacheMapPtr;

////////////////////////////////////////////////////////////////////////////////////////////////////
// CHosterMap

struct SHosterRange
{
	SHosterRange(int Count = 0) : PubCount(Count) {}
	SHosterRange(int Count, QString& Account) : PubCount(0) {MyCounts[Account] = Count;}

	int PubCount;
	QMap<QString, int> MyCounts;

	int Total() 
	{
		int Count = PubCount;
		foreach(int MyCount, MyCounts)
			Count += MyCount;
		return Count;
	}

	bool operator== (const SHosterRange& Val) const throw() {return PubCount == Val.PubCount && MyCounts == Val.MyCounts;}
};

class CHosterMap: public CRangeMap<QMap<QString, SHosterRange> >
{
public:
	CHosterMap(uint64 Size)
	: CRangeMap<QMap<QString, SHosterRange> >(Size) {}

	static QMap<QString, SHosterRange> MkValue(const QString& Hoster, QString Account, int Count = 1)
	{
		CHosterMap::ValueType Val;
		if(Account.isEmpty())
			Val.insert(Hoster, SHosterRange(Count));
		else
			Val.insert(Hoster, SHosterRange(Count, Account));
		return Val;
	}

protected:

	virtual bool		StateSet(ValueType Cur) const
	{
		return !Cur.isEmpty();
	}

	virtual	bool		MatchState(ValueType Cur, ValueType State) const
	{
		return Cur == State;
	}

	virtual	ValueType	MakeState(ValueType Cur, ValueType State, ESet eMode) const
	{
		switch(eMode)
		{
		case eSet:			return State;
		case eAdd:			//return Cur | State;
			for(ValueType::iterator I = State.begin(); I != State.end(); I++)
			{
				ValueType::iterator J = Cur.find(I.key());
				if(J != Cur.end())
				{
					J.value().PubCount += I.value().PubCount;

					for(QMap<QString, int>::iterator K = I.value().MyCounts.begin(); K != I.value().MyCounts.end(); K++)
						J.value().MyCounts[K.key()] += K.value();
				}
				else
					Cur.insert(I.key(), I.value());
			}
			return Cur;
		case eClr:			//return Cur & ~State;
			for(ValueType::iterator I = State.begin(); I != State.end(); I++)
			{
				ValueType::iterator J = Cur.find(I.key());
				if(J != Cur.end())
				{
					if(J.value().PubCount > I.value().PubCount)
						J.value().PubCount -= I.value().PubCount;

					for(QMap<QString, int>::iterator K = I.value().MyCounts.begin(); K != I.value().MyCounts.end(); K++)
					{
						if(J.value().MyCounts[K.key()] > K.value())
							J.value().MyCounts[K.key()] -= K.value();
						else
							J.value().MyCounts.remove(K.key());
					}

					if(J.value().Total() == 0)
						Cur.erase(J);
				}
			}
			return Cur;
		default:
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual	ValueType	MergeState(ValueType Cur, ValueType State, EMerge eMode) const
	{
		switch(eMode)
		{
		case eUnion:		//return Cur | State;		// take all bits set in all ranges
			{
				for(ValueType::iterator I = State.begin(); I != State.end(); I++)
				{
					ValueType::iterator J = Cur.find(I.key());
					if(J != Cur.end())
					{
						if(I.value().PubCount > J.value().PubCount)
							J.value().PubCount = I.value().PubCount;

						for(QMap<QString, int>::iterator K = I.value().MyCounts.begin(); K != I.value().MyCounts.end(); K++)
						{
							if(K.value() > J.value().MyCounts[K.key()])
								J.value().MyCounts[K.key()] = K.value();
						}
					}
					else
						Cur.insert(I.key(), I.value());
				}
				return Cur;
			}
		case eInter:		//return Cur & State;		// take only those bits set in all ranges
			{
				ValueType New;
				for(ValueType::iterator I = State.begin(); I != State.end(); I++)
				{
					ValueType::iterator J = Cur.find(I.key());
					if(J != Cur.end())
					{
						SHosterRange Val = J.value();

						if(I.value().PubCount != -1) // -1 means its the empyt entry of an itteration value
						{
							if(I.value().PubCount < Val.PubCount)
								Val.PubCount = I.value().PubCount;

							for(QMap<QString, int>::iterator K = I.value().MyCounts.begin(); K != I.value().MyCounts.end(); K++)
							{
								if(K.value() < Val.MyCounts[K.key()])
									Val.MyCounts[K.key()] = K.value();
							}
						}

						New.insert(I.key(), Val);
					}
				}
				return New;
			}
		default: 
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual QString		State2Str(ValueType Cur) const
	{
		QString Str;
		foreach(const QString& Hoster, Cur.keys())
		{
			if(!Str.isEmpty())
				Str.append(", ");
			Str.append(QString("%1 (%2/???)").arg(Hoster).arg(Cur[Hoster].PubCount));
		}
		return Str + ".";
	}
};

typedef QSharedPointer<CHosterMap> CHosterMapPtr;


////////////////////////////////////////////////////////////////////////////////////////////////////
// CStrMap

/*class CStrMap: public CRangeMap<QStringList>
{
public:
	CStrMap(uint64 Size)
	: CRangeMap<QStringList>(Size) {}

protected:

	virtual bool		StateSet(ValueType Cur) const
	{
		return !Cur.isEmpty();
	}

	virtual	bool		MatchState(ValueType Cur, ValueType State) const
	{
		return Cur == State;
	}

	virtual	ValueType	MakeState(ValueType Cur, ValueType State, ESet eMode) const
	{
		switch(eMode)
		{
		case eSet:			return State;
		case eAdd:			//return Cur | State;
			return MergeState(Cur, State, eUnion);
		case eClr:			//return Cur & ~State;
			foreach(const QString& Str, State)
				Cur.removeAll(Str);
			return Cur;
		default:
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual	ValueType	MergeState(ValueType Cur, ValueType State, EMerge eMode) const
	{
		switch(eMode)
		{
		case eUnion:		//return Cur | State;		// take all bits set in all ranges
			{
				foreach(const QString& Str, State)
				{
					if(!Cur.contains(Str))
						Cur.append(Str);
				}
				return Cur;
			}
		case eInter:		//return Cur & State;		// take only those bits set in all ranges
			{
				ValueType New;
				foreach(const QString& Str, State)
				{
					if(Cur.contains(Str))
						New.append(Str);
				}
				return New;
			}
		default: 
			ASSERT(0);	// Not Implemented
			return ValueType();
		}
	}

	virtual QString		State2Str(ValueType Cur) const
	{
		return Cur.join(", ") + ".";
	}
};

typedef QSharedPointer<CStrMap> CStrMapPtr;*/
