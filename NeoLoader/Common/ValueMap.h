#pragma once


template <typename V>
class CRangeMap
{
public:
	enum ESet
	{
		eSet,	// set the part status to this exact one
		eAdd,	// add status flag to the part
		eClr,	// remove a the status flag
	};

	enum EMerge
	{
		eUnion,
		eInter,
	};

	typedef V					ValueType;
	typedef QMap<uint64,V>		MapType;

	struct SIterHint
	{
		SIterHint() {Revision = 0;}
		virtual ~SIterHint() {}

		uint64 Revision;
		typename MapType::const_iterator Iter;
	};

	template<class T>
	static SIterHint* AllocHint()
	{
		T* ptr = (T*)malloc(sizeof(T));
		new (ptr) T();
		return ptr;
	}
	static void FreeHint(SIterHint* ptr)	
	{
		if(ptr)
		{
			ptr->~SIterHint();
			free(ptr);
		}
	}

	struct SIterator
	{
		SIterator(uint64 From = 0, uint64 To = -1, bool bHint = true)
		{
			uFrom = From;
			uTo = To;

			uBegin = uFrom;
			uEnd = 0;

			uState = ValueType();

			pHint = bHint ? NULL : ((SIterHint*)-1);
		}
		~SIterator() {if(pHint && pHint != ((SIterHint*)-1)) CRangeMap::FreeHint(pHint);}

		uint64		uFrom;
		uint64		uTo;

		uint64		uBegin;
		uint64		uEnd;

		ValueType	uState;

		SIterHint*	pHint;
	};

	CRangeMap(uint64 Size)
	{
		m_Revision = 1;

		ASSERT(Size != 0); // Size can't be 0!
		m_PartMap.insert(Size,V());
	}

	/**
	* @return: the total covered file size, the alst key in the map mus alwys be equal to the total file size.
	*/
	virtual void		Reset(uint64 Size = -1)
	{
		IncrRevision();

		ASSERT(Size != 0); // Size can't be 0!
		ASSERT(!m_PartMap.empty());
		if(Size == -1)
			Size = (--m_PartMap.end()).key();
		m_PartMap.clear();
		m_PartMap.insert(Size,V());
	}

	virtual uint64		GetSize() const
	{
		ASSERT(!m_PartMap.empty());
		return (--m_PartMap.end()).key();
	}

	virtual size_t		GetCount() const
	{
		return m_PartMap.size();
	}

	/**
	* Sets the given range to a particular value
	* @param: uBegin: first index in teh range to set
	* @param: uEnd: first index after the range, those uEnd - uBegin == byte coutn in range
	* @param: uState: state to set/add/clear
	* @param: eMode: set mode, can eider set teh value, or update if by addin/removing one ore more bit flags
	*/
	virtual void		SetRange(uint64 uBegin, uint64 uEnd, ValueType uState, ESet eMode = eSet)
	{
		IncrRevision();

		ASSERT(!m_PartMap.empty());
		if(uEnd == -1)
			uEnd = (--m_PartMap.end()).key();

		ASSERT(uBegin >= 0);
		ASSERT(uBegin < uEnd);

		// Get begin and end of teh range
		typename MapType::iterator End = m_PartMap.lowerBound(uEnd); // get the end of the part
		if(End == m_PartMap.end())
		{
			ASSERT(0);
			return;
		}
		if(End.key() != uEnd) 
			End = m_PartMap.insert(uEnd,End.value()); 
		typename MapType::iterator Begin = m_PartMap.lowerBound(uBegin); // get the begin of the part
		if(Begin == m_PartMap.end())
		{
			ASSERT(0);
			return;
		}
		if(Begin.key() != uBegin)
			Begin = m_PartMap.insert(uBegin,Begin.value()); 

		// update valie
		ASSERT(Begin != End);
		for(typename MapType::iterator Part = End; Part != Begin ;Part--)
			Part.value() = MakeState(Part.value(), uState, eMode); 

		// clear duplicates
		typename MapType::iterator Part = Begin == m_PartMap.begin() ? Begin : --Begin;
		typename MapType::iterator PartEnd = ++End == m_PartMap.end() ? End : ++End;
		typename MapType::iterator PrevPart = Part;
		for(Part++; Part != PartEnd; Part++)
		{
			if(MatchState(PrevPart.value(), Part.value()) || PrevPart.key() == 0) // range anding with 0 is not real
				m_PartMap.erase(PrevPart);
			PrevPart = Part;
		}
	}

	/**
	* Return a given range
	* @param: uBegin: first index in teh range to get
	* @param: uEnd: first index after the get
	* @param: eMode: merge mode
	* @param: uMask: state mask
	* @return: resulting range state
	*/
	virtual ValueType	GetRange(uint64 uBegin, uint64 uEnd, EMerge eMode = eInter, ValueType uMask = V()) const
	{
		ASSERT(!m_PartMap.empty());
		if(uEnd == -1)
			uEnd = (--m_PartMap.end()).key();

		ASSERT(uBegin < uEnd);
		typename MapType::const_iterator Part = m_PartMap.lowerBound(uEnd); // get the end of the part
		if(Part == m_PartMap.end())
		{
			ASSERT(0);
			return V();
		}

		ValueType uState = V();
		bool bFirst = true;
		for(;;)
		{
			if(Part.key() <= uBegin)
				break; // part is befoure our area of interest
		
			V uCurrent = StateSet(uMask) ? MergeState(Part.value(), uMask, eInter) : Part.value();
			if(bFirst)
			{
				bFirst = false;
				uState = uCurrent;
			}
			else
				uState = MergeState(uCurrent, uState, eMode);

			if(Part == m_PartMap.begin())
				break;
			--Part;
		}

		return uState;
	}

	/**
	* GetNextRange gives the first range starting at uBegin or after with the state uState
	* @param: uBegin: first index in teh range
	* @param: uEnd: first index after the range
	* @param: uState: range state
	* @param: uMask: state mask
	* @return: range found or not
	*/
	virtual bool		GetNextRange(uint64 &uBegin, uint64 &uEnd, ValueType &uState, ValueType uMask = V(), SIterHint** pHint = NULL) const
	{
		ASSERT(!m_PartMap.empty());

		if(pHint && !*pHint)
			*pHint = AllocHint<SIterHint>();

		if(uBegin > uEnd) // if we tel a begin we expect to get the range the begin is in
		{
			ASSERT(uBegin < (--m_PartMap.end()).key());
			uEnd = uBegin;
		}
		uEnd++;

		//lower_bound: Returns an iterator to the first element in a QMap that has a key value that is equal to or greater than that of a specified key.
		//upper_bound: Returns an iterator to the first element in a QMap that has a key value that is greater than that of a specified key.
		typename MapType::const_iterator Part;
		if(pHint && (*pHint)->Revision == m_Revision // if the QMap was modifyed, we have to search again
			&& (((*pHint)->Iter != m_PartMap.end()) ? (*pHint)->Iter.key() : (--m_PartMap.end()).key()) + 1 == uEnd) // also if the iterator position was edited
		{
			Part = (*pHint)->Iter;
			Part++;
			//typename MapType::const_iterator Test = m_PartMap.lowerBound(uEnd);
			//ASSERT(Part == Test);
		}
		else
			Part = m_PartMap.lowerBound(uEnd); // get the end of the part

		if(Part == m_PartMap.end())
			return false;

		if(Part == m_PartMap.begin())
			uBegin = 0;
		else
		{
			typename MapType::const_iterator PrevPart = Part;
			PrevPart--;
			uBegin = PrevPart.key();
		}

		if(StateSet(uMask))
		{
			uState = (MergeState(Part.value(), uMask, eInter));
			do
			{
				uEnd = Part.key();
				if(pHint)
					(*pHint)->Iter = Part;

				Part++;
			}
			while(Part != m_PartMap.end() && MatchState(uState, MergeState(Part.value(), uMask, eInter)));
		}
		else
		{
			uState = Part.value();
			uEnd = Part.key();
			if(pHint)
				(*pHint)->Iter = Part;
		}
		if(pHint)
			(*pHint)->Revision = m_Revision;
		return true;
	}

	virtual bool		GetPrevRange(uint64 &uBegin, uint64 &uEnd, ValueType &uState, ValueType uMask = V()) const
	{
		uEnd = --uBegin;
		return GetNextRange(uBegin, uEnd, uState, uMask);
	}

	/**
	* IterateRanges itterates through all ranges
	* @param: Iterator: Range Iterator
	* @param: uMask: state mask
	* @return: range found or not
	*/
	virtual bool		IterateRanges(SIterator& Iterator, ValueType uMask = V()) const
	{
		if(Iterator.uEnd == Iterator.uTo)
			return false; // check if we reached the end

		uint64 uFrom = Max(Iterator.uEnd, Iterator.uFrom);

		if(!GetNextRange(Iterator.uBegin, Iterator.uEnd, Iterator.uState, uMask, Iterator.pHint != ((SIterHint*)-1) ? &Iterator.pHint : NULL))
			return false; // there are no more ranges

		ASSERT(Iterator.uTo > uFrom);
		if(Iterator.uBegin < uFrom)
			Iterator.uBegin = uFrom;
		else if(Iterator.uBegin >= Iterator.uTo)
			return false; // we are past the range we ware interested in
		if(Iterator.uEnd > Iterator.uTo)
			Iterator.uEnd = Iterator.uTo;
		return true;
	}

	/**
	* Copyes all states from the source QMap to this QMap
	* @param: pMap: pointer to teh source QMap
	*/
	virtual void		Assign(const CRangeMap* pMap, ValueType uFilter = 0, ESet eMode = eAdd)
	{
		m_PartMap = pMap->m_PartMap;
		m_Revision++;
	}

	/**
	* Copyes all states from the source QMap to this QMap
	* @param: pMap: pointer to teh source QMap
	*/
	virtual void		Merge(const CRangeMap* pMap, ValueType uFilter = 0, ESet eMode = eAdd)
	{
		uint64 uBegin = 0;
		uint64 uEnd = 0;
		V uState = V();
		SIterHint* pHint = NULL;
		while(pMap->GetNextRange(uBegin,uEnd,uState, V(), &pHint))
		{
			if(StateSet(uFilter))
				uState = MergeState(uState, uFilter, eInter);
			SetRange(uBegin, uEnd, uState, eMode);
		}
		FreeHint(pHint);
	}

	/**
	* returns the total length o all ranges with th eparticular state
	* @param: uTest: state to look for
	*/
	virtual uint64		CountLength(ValueType uTest, bool bInverse = false) const
	{
		uint64 uLength = 0;
		uint64 uBegin = 0;
		uint64 uEnd = 0;
		V uState = V();
		SIterHint* pHint = NULL;
		while(GetNextRange(uBegin,uEnd,uState, V(), &pHint))
		{
			if(StateSet(MergeState(uState, uTest, eInter)) != bInverse)
				uLength += uEnd - uBegin;
		}
		FreeHint(pHint);
		return uLength;
	}

	virtual void		IncrRevision() {m_Revision ++;}
	virtual uint64		GetRevision() {return m_Revision;}

	virtual QString		PrintMap() const
	{
		QString Out;
		uint64 Old = 0;
		for(typename MapType::const_iterator Part = m_PartMap.begin(); Part != m_PartMap.end(); Part++)
		{
			Out += QString("%1 - %2: %3; ").arg(Old).arg(Part.key()).arg(State2Str(Part.value()));
			Old = Part.key();
		}
		return Out;
	}

	virtual size_t		GetRangeCount() {return m_PartMap.size();}

protected:
	virtual bool		StateSet(ValueType uCur) const = 0;
	virtual	bool		MatchState(ValueType uCur, ValueType uState) const = 0;
	virtual	ValueType	MakeState(ValueType uCur, ValueType uState, ESet eMode) const = 0;
	virtual	ValueType	MergeState(ValueType uCur, ValueType uState, EMerge eMode) const = 0;
	virtual QString		State2Str(ValueType uCur) const = 0;

	MapType				m_PartMap;
	uint64				m_Revision;
};

///////////////////////////////////////////////////////////////////////////////////////////////
//

template <typename V> // V may be an integer only
class CValueMap: public CRangeMap<V>
{
public:
	CValueMap(uint64 Size)
	: CRangeMap<V>(Size) {}

protected:
	virtual bool		StateSet(V uCur) const
	{
		return uCur != V();
	}

	virtual	bool		MatchState(V uCur, V uState) const
	{
		return uCur == uState;
	}

	virtual	V	MakeState(V uCur, V uState, typename CRangeMap<V>::ESet eMode) const
	{
		switch(eMode)
		{
		case CRangeMap<V>::eAdd:			return uCur | uState;
		case CRangeMap<V>::eClr:			return uCur & ~uState;
		case CRangeMap<V>::eSet:
		default:			return uState;
		}
	}

	virtual	V	MergeState(V uCur, V uState, typename CRangeMap<V>::EMerge eMode) const
	{
		switch(eMode)
		{
		case CRangeMap<V>::eUnion:		return uCur | uState;		// take all bits set in all ranges
		case CRangeMap<V>::eInter:		return uCur & uState;		// take only those bits set in all ranges
		default:			return V();
		}
	}

	virtual QString		State2Str(V uCur) const
	{
		return QString::number(uCur);
	}
};
