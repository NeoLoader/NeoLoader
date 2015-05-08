#include "GlobalHeader.h"
#include "PartMap.h"
#include "Hashing/FileHash.h"
#include "../../Framework/Buffer.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CAvailDiff

CAvailDiff::CAvailDiff() 
{
}

void CAvailDiff::Update(uint64 uBegin, uint64 uEnd, uint32 uNew, CShareMap* pMap, uint32 uMask)
{
	ASSERT(uBegin < uEnd);
	CShareMap::SIterator DiffIter(uBegin, uEnd);
	while(pMap->IterateRanges(DiffIter, uMask))
	{
		uint32 bOld = (DiffIter.uState & uMask);
		if(bOld != uNew)
			Add(DiffIter.uBegin, DiffIter.uEnd, uNew, bOld);
	}
}

void CAvailDiff::Add(uint64 uBegin, uint64 uEnd, uint32 uNew, uint32 uOld) 
{
	m_DiffList.append(SAvailDiff(uBegin, uEnd, uNew, uOld));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CPartMap
//

uint64 GetBitMapIndex(uint64 uOffset, uint64 uPartSize, uint16 uBlockCount, uint64 uBlockSize)
{
	return ((uOffset / uPartSize) * uBlockCount) + (uBlockSize ? ((uOffset % uPartSize) / uBlockSize) : 0);
}

QBitArray CPartMap::ToBitArray(uint64 uPartSize, uint64 uBlockSize, ValueType uTest) const
{
	uint64 Size = GetSize();
	uint16 uBlockCount = uBlockSize ? DivUp(uPartSize, uBlockSize) : 1;
	QBitArray Array;
	Array.resize(uBlockSize ? ((Size / uPartSize) * uBlockCount) + DivUp(Size % uPartSize, uBlockSize) : DivUp(Size, uPartSize));
	uint64 uBegin = 0;
	uint64 uEnd = 0;
	ValueType uState = 0;
	ValueType uLast = 0;
	int Index = 0;
	while(GetNextRange(uBegin, uEnd, uState))
	{
		int Prev = GetBitMapIndex(uBegin, uPartSize, uBlockCount, uBlockSize);
		if(Prev > Index)
			Array.setBit(Index++, StateSet(MergeState(uLast, uTest, eInter)) && StateSet(MergeState(uState, uTest, eInter))); //Array.setBit(Index++, (uLast & uTest) != 0 && (uState & uTest) != 0);
		ASSERT(Prev == Index);
		int Next = GetBitMapIndex(uEnd, uPartSize, uBlockCount, uBlockSize);
		while(Index < Next)
			Array.setBit(Index++, StateSet(MergeState(uState, uTest, eInter))); //Array.setBit(Index++, (uState & uTest) != 0);
		uLast = uState;
	}
	return Array;
}

QVariantMap CPartMap::Store()
{
	ASSERT(!m_PartMap.empty());

	QStringList List;
	for(MapType::iterator Part = m_PartMap.begin(); Part != m_PartMap.end(); Part++)
		List.append(QString("%1:%2").arg(Part.key()).arg(Part.value().uStates));

	QVariantMap Map;
	Map["Size"] = (--m_PartMap.end()).key();
	Map["Ranges"] = List.join("|");
	return Map;
}

bool CPartMap::Load(const QVariantMap& Map)
{
	ASSERT(!m_PartMap.empty());
	uint64 Size = (--m_PartMap.end()).key();

	QStringList List = Map["Ranges"].toString().split("|");
	foreach(const QString& Part, List)
	{
		StrPair KeyVal = Split2(Part,":");
		m_PartMap[KeyVal.first.toULongLong()] = KeyVal.second.toUInt();
	}

	if(m_PartMap.empty() || Size != (--m_PartMap.end()).key())
	{
		m_PartMap.clear();
		m_PartMap.insert(Size,0);
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CShareMap
//

QString CShareMap::GetRanges(ValueType uTest) const
{
	QString Ranges;
	uint64 uBegin = 0;
	uint64 uEnd = 0;
	ValueType uState = 0;
	while(GetNextRange(uBegin, uEnd, uState, uTest)) // mask other states thay are irrelevant
	{
		if(StateSet(MergeState(uState, uTest, eInter))) //if((uState & uTest) != 0)
		{
			if(Ranges.length() > 0)
				Ranges.append(", ");
			Ranges.append(QString("%1-%2").arg(uBegin).arg(uEnd));
		}
	}
	return Ranges;
}

bool CShareMap::SetRanges(const QString& Ranges, ValueType uState)
{
	foreach(const QString& Range, Ranges.split(","))
	{
		StrPair BeginEnd = Split2(Range,"-");
		uint64 uBegin = BeginEnd.first.toULongLong();
		uint64 uEnd = BeginEnd.second.toULongLong();
		if(uBegin > uEnd || uEnd > GetSize())
			return false;
		SetRange(uBegin, uEnd, uState);
	}
	return true;
}

uint64 GetBitMapOffset(int BitIndex, uint64 uPartSize, uint16 uBlockCount, uint64 uBlockSize)
{
	uint64 uPart = BitIndex / uBlockCount;
	uint64 uSubPart = BitIndex % uBlockCount;
	return (uPart * uPartSize) + uSubPart * uBlockSize;
}

uint64 CShareMap::FromBitArray(const QBitArray& Array, uint64 uPartSize, uint64 uBlockSize, ValueType uState, CAvailDiff& AvailDiff)
{
	uint64 uAvailSize = 0;

	uint64 Size = GetSize();
	uint16 uBlockCount = uBlockSize ? DivUp(uPartSize, uBlockSize) : 1;
	uint64 uBegin = 0;
	uint64 uEnd = uPartSize;
	bool bCur = Array.testBit(0);
	for(int i = 1; i < Array.size(); i ++)
	{
		bool bVal = Array.testBit(i);
		if(bCur != bVal)
		{
			if(bCur)
				uAvailSize += uEnd - uBegin;
			AvailDiff.Update(uBegin, uEnd, bCur ? uState : 0, this, uState);
			SetRange(uBegin, uEnd, uState, bCur ? CShareMap::eAdd : CShareMap::eClr);
			bCur = bVal;
			uBegin = GetBitMapOffset(i, uPartSize, uBlockCount, uBlockSize);
		}
		uEnd = GetBitMapOffset(i + 1, uPartSize, uBlockCount, uBlockSize);
		ASSERT(uBegin < uEnd);
	}
	if(uEnd > Size)
		uEnd = Size;
	if(bCur)
		uAvailSize += uEnd - uBegin;
	AvailDiff.Update(uBegin, uEnd, bCur ? uState : 0, this, uState);
	SetRange(uBegin, Size, uState, bCur ? CShareMap::eAdd : CShareMap::eClr);

	return uAvailSize;
}

//void CShareMap::FromBitArray(const QBitArray& Array, uint64 uPartSize, ValueType uState)
//{
//	ASSERT(Array.size() == DivUp(GetSize(),uPartSize));
//
//	uint64 Size = GetSize();
//	uint64 uBegin = 0;
//	uint64 uEnd = uPartSize;
//	bool bCur = Array.testBit(0);
//	for(int i = 1; i < Array.size(); i ++)
//	{
//		bool bVal = Array.testBit(i);
//		if(bCur != bVal)
//		{
//			SetRange(uBegin, uEnd, uState, bCur ? CPartMap::eAdd : CPartMap::eClr);
//			bCur = bVal;
//			uBegin = i * uPartSize;
//		}
//		uEnd = (i+1) * uPartSize;
//	}
//	SetRange(uBegin, Size, uState, bCur ? CPartMap::eAdd : CPartMap::eClr);
//}

QVariantMap CShareMap::Store()
{
	ASSERT(!m_PartMap.empty());

	QStringList List;
	for(MapType::iterator Part = m_PartMap.begin(); Part != m_PartMap.end(); Part++)
		List.append(QString("%1:%2").arg(Part.key()).arg(Part.value()));

	QVariantMap Map;
	Map["Size"] = (--m_PartMap.end()).key();
	Map["Ranges"] = List.join("|");
	return Map;
}

bool CShareMap::Load(const QVariantMap& Map)
{
	ASSERT(!m_PartMap.empty());
	uint64 Size = (--m_PartMap.end()).key();

	QStringList List = Map["Ranges"].toString().split("|");
	foreach(const QString& Part, List)
	{
		StrPair KeyVal = Split2(Part,":");
		m_PartMap[KeyVal.first.toULongLong()] = KeyVal.second.toUInt();
	}

	if(m_PartMap.empty() || Size != (--m_PartMap.end()).key())
	{
		m_PartMap.clear();
		m_PartMap.insert(Size,0);
		return false;
	}
	return true;
}

QByteArray CShareMap::Bits2Bytes(const QBitArray& Bits, bool bED2kStyle)
{
	QByteArray Bytes;
	int Size = DivUp(Bits.size(),8);
	Bytes.fill('\0', Size);
	for(int Index = 0, Byte = 0; Index < Bits.size(); Index += 8, Byte++)
	{
		for(int Bit = 0; Bit < 8; Bit++)
		{
			int BitIndex = Index + Bit;
			if(BitIndex >= Bits.size())
				break;

			if (Bits.testBit(BitIndex)) 
			{
				int Shift = bED2kStyle ? Bit : (7 - Bit);
				Bytes[Byte] = uchar(Bytes.at(Byte)) | (1 << Shift);
			}
		}
	}
	return Bytes;
}

QBitArray CShareMap::Bytes2Bits(const QByteArray& Bytes, int PartCount, bool bED2kStyle)
{
	QBitArray Bits(PartCount);
	for (int Index = 0, Byte = 0; Byte < Bytes.size(); Index += 8, Byte++) 
	{
		for (int Bit = 0; Bit < 8; Bit++) 
		{
			int BitIndex = Index + Bit;
			if (BitIndex >= PartCount) 
				break;

			quint32 Shift = bED2kStyle ? Bit : (7 - Bit);
			if (Bytes.at(Byte) & (1 << Shift)) 
				Bits.setBit(BitIndex);
		}
	}
	return Bits;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CLinkedPartMap
//

CSyncPartMap* CLinkedPartMap::Restore(const QVariantMap& Map)
{
	CSyncPartMap* pMap;
	uint64 Size = Map["Size"].toULongLong();
	if(Size == 0)
		return NULL;
	QString Type = Map["Type"].toString();
	if(Type.isEmpty())
		pMap = new CSyncPartMap(Size);
	else if(CSharedPartMap::CheckType(Type))
		pMap = new CSharedPartMap(Size);
	else if(CJoinedPartMap::CheckType(Type))
		pMap = new CJoinedPartMap(Size);
	else
		return NULL; // unknown map type
		
	if(!pMap->Load(Map))
	{
		delete pMap;
		return NULL;
	}
	return pMap;
}

void CLinkedPartMap::SetupLink(uint64 uShareBegin, uint64 uShareEnd, uint64 ID)
{
	QWriteLocker Locker(&m_Locker);

	SPartMapLink* pLink = new SPartMapLink;
	pLink->ID				= ID;
	pLink->uShareBegin		= uShareBegin;
	pLink->uShareEnd		= uShareEnd;
	AddLink(pLink);
}

bool CLinkedPartMap::EnableLink(uint64 ID, CPartMapPtr& pMap)
{
	QWriteLocker Locker(&m_Locker);

	QMap<uint64, SPartMapLink*>::iterator I = m_Links.find(ID);
	if(SPartMapLink* pLink = m_Links.value(ID))
	{
		pLink->pMap = pMap;
		return true;
	}
	return false;
}

void CLinkedPartMap::BreakLink(uint64 ID)
{
	QWriteLocker Locker(&m_Locker);

	delete m_Links.take(ID);
}

QVariantMap CLinkedPartMap::Store()
{
	QVariantMap Map = CSyncPartMap::Store();

	QVariantList Links;
	foreach(SPartMapLink* pLink, GetLinks())
	{
		QVariantMap vLink;
		vLink["ID"]			= pLink->ID;
		vLink["Begin"]		= pLink->uShareBegin;
		vLink["End"]		= pLink->uShareEnd;
		Links.append(vLink);
	}
	Map["Links"] = Links;
	Map["Type"] = GetType();

	return Map;
}

bool CLinkedPartMap::Load(const QVariantMap& Map)
{
	if(!CSyncPartMap::Load(Map))
		return false;

	QVariantList Links = Map["Links"].toList();
	foreach(const QVariant& vlink, Links)
	{
		QVariantMap vLink = vlink.toMap();
		SPartMapLink* pLink = new SPartMapLink;
		pLink->ID			= vLink["ID"].toULongLong();
		pLink->uShareBegin	= vLink["Begin"].toULongLong();
		pLink->uShareEnd	= vLink["End"].toULongLong();
		AddLink(pLink);
	}

	return true;
}

void CLinkedPartMap::NotifyChange(bool bPurge)
{
	emit Change(bPurge);

	foreach(SPartMapLink* pLink, GetLinks())
	{
		if(CPartMapPtr pMap = pLink->pMap.toStrongRef())
			emit pMap->Change(bPurge);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CJoinedPartMap
//

void CJoinedPartMap::BreakLink(uint64 ID)
{
	QWriteLocker Locker(&m_Locker);

	if(SPartMapLink* pLink = m_Links.take(ID))
	{
		pLink->ID = 0;
		pLink->pMap.clear();
	}
	// no delete keep link in joint list
}

void CJoinedPartMap::SetSharedRange(uint64 uBegin, uint64 uEnd, ValueType uState, ESet eMode)
{
	if(uEnd == -1)
		uEnd = GetSize();

	QReadLocker Locker(&m_Locker);
	QMap<uint64,SPartMapLink*>::const_iterator I = m_Joints.lowerBound(ULLONG_MAX - uBegin);
	if(I == m_Joints.end()){ASSERT(0); return;}
	for(;;)
	{
		SPartMapLink* pLink = *I;
		if(!(pLink->uShareBegin <= uBegin && uBegin < pLink->uShareEnd)) {ASSERT(0); break;}

		uint64 uSectionEnd = Min(pLink->uShareEnd, uEnd);
		uint64 uLinkBegin = uBegin - pLink->uShareBegin;
		uint64 uLinkEnd = uSectionEnd - pLink->uShareBegin;
		CPartMapPtr pMap = pLink->pMap.toStrongRef();
		if(CSharedPartMap* pSharedMap = qobject_cast<CSharedPartMap*>(pMap.data()))
			pSharedMap->SetRange(uLinkBegin, uLinkEnd, uState, eMode);
		uBegin = uSectionEnd;

		ASSERT(uBegin <= uEnd);
		if(uBegin == uEnd)
			break; 

		if(I == m_Joints.begin()){ASSERT(0);	break;}
		I--;
	}
}

CSharedPartMap::ValueType CJoinedPartMap::GetRange(uint64 uBegin, uint64 uEnd, EMerge eMode, ValueType uMask) const
{
	if(uEnd == -1)
		uEnd = GetSize();

	ValueType uLocal = CLinkedPartMap::GetRange(uBegin, uEnd, eMode, uMask);

	QReadLocker Locker(&m_Locker);
	if(m_Joints.isEmpty())
		return 0;
	QMap<uint64,SPartMapLink*>::const_iterator I = m_Joints.lowerBound(ULLONG_MAX - uBegin);
	if(I == m_Joints.end())
	{
		ASSERT(0); 
		return 0;
	}

	ValueType uState = 0;
	bool bFirst = true;
	for(;;)
	{
		SPartMapLink* pLink = *I;
		if(!(pLink->uShareBegin <= uBegin && uBegin < pLink->uShareEnd)) {ASSERT(0); break;}

		uint64 uSectionEnd = Min(pLink->uShareEnd, uEnd);

		uint64 uLinkBegin = uBegin - pLink->uShareBegin;
		uint64 uLinkEnd = uSectionEnd - pLink->uShareBegin;
		ValueType uCurrent = 0;
		CPartMapPtr pMap = pLink->pMap.toStrongRef();
		if(CSharedPartMap* pSharedMap = qobject_cast<CSharedPartMap*>(pMap.data()))
			uCurrent = pSharedMap->GetRange(uLinkBegin, uLinkEnd, eMode, uMask);
		
		if(bFirst)
		{
			bFirst = false;
			uState = uCurrent;
		}
		else
			uState = MergeState(uCurrent, uState, eMode);

		uBegin = uSectionEnd;

		ASSERT(uBegin <= uEnd);
		if(uBegin == uEnd)
			break; 

		if(I == m_Joints.begin()){ASSERT(0);	break;}
		I--;
	}

	return MergeState(uState, uLocal, eUnion); //return uState | uLocal;
}

bool CJoinedPartMap::GetNextRange(uint64 &uBegin, uint64 &uEnd, ValueType &uState, ValueType uMask, SIterHint** pHint) const
{
	if(pHint && !*pHint)
		*pHint = AllocHint<SIterHintEx>();

	if(uBegin > uEnd) // if we tel a begin we expect to get the range the begin is in
		uEnd = uBegin;

	QReadLocker Locker(&m_Locker);
	QMap<uint64,SPartMapLink*>::const_iterator I = m_Joints.lowerBound(ULLONG_MAX - uEnd);
	if(I == m_Joints.end())
		return false;

	uint64 uLocalBegin = 0;
	uint64 uLocalEnd = 0;
	ValueType uLocalState = 0;
	uint64 uSectionEnd = uEnd;
	for(;;)
	{
		SPartMapLink* pLink = *I;

		if(uSectionEnd >= pLink->uShareEnd)
		{
			//uSectionEnd--;
			if(I == m_Joints.begin())
			{
				if(uLocalEnd == 0)
					return false; // end
				break;
			}
			I--;
			continue;
		}

		uint64 uRemoteBegin = 0;
		uint64 uRemoteEnd = 0;
		ValueType uRemoteState = 0;
		CPartMapPtr pMap = pLink->pMap.toStrongRef();
		if(CSharedPartMap* pSharedMap = qobject_cast<CSharedPartMap*>(pMap.data()))
		{
			ASSERT(uSectionEnd >= pLink->uShareBegin);
			uRemoteEnd = uSectionEnd - pLink->uShareBegin;
			//if(!pSharedMap->GetNextRange(uRemoteBegin, uRemoteEnd, uRemoteState, uMask, pHint ? &((SIterHintEx*)*pHint)->Hint[pSharedMap] : NULL))
			if(!pSharedMap->GetNextRange(uRemoteBegin, uRemoteEnd, uRemoteState, 0, pHint ? &((SIterHintEx*)*pHint)->Hint[pSharedMap] : NULL)) // note: masking is done in this function
			{
				ASSERT(0);
				return false;
			}
			uRemoteEnd += pLink->uShareBegin;
			uRemoteBegin += pLink->uShareBegin;
		}
		else
		{
			uRemoteBegin = pLink->uShareBegin;
			uRemoteEnd = pLink->uShareEnd;
			uRemoteState = pLink->ID == 0 ? Part::Disabled : 0; // handle missing ranges as eDisabled (no upload no downlaod)
		}

		if(uLocalEnd == 0) // thats the first run
		{
			uLocalBegin = uRemoteBegin;
			uLocalState = uRemoteState;
		}
		else if(StateSet(uMask) ? !MatchState(MergeState(uLocalState, uMask, eInter), MergeState(uRemoteState, uMask, eInter)) : !MatchState(uLocalState, uRemoteState)) // else if(uMask ? (uLocalState & uMask) != (uRemoteState & uMask) : uLocalState != uRemoteState)
			break;
		uLocalEnd = uRemoteEnd;

		uSectionEnd = uRemoteEnd + 1;
	}

	uBegin = uLocalBegin;
	uEnd = uLocalEnd;
	uState = uLocalState;
	return true;
}

QList<SPartMapLink*> CJoinedPartMap::SelectLinks(uint64 &uBegin, uint64 &uEnd) const
{
	if(uEnd == -1)
		uEnd = GetSize();

	QList<SPartMapLink*> Links;
	QReadLocker Locker(&m_Locker);
	QMap<uint64,SPartMapLink*>::const_iterator I = m_Joints.lowerBound(ULLONG_MAX - uBegin);
	if(I == m_Joints.end()){ASSERT(0); return Links;}
	for(;;)
	{
		SPartMapLink* pLink = *I;
		if(!(pLink->uShareBegin <= uBegin && uBegin < pLink->uShareEnd)) {ASSERT(0); break;}

		uint64 uSectionEnd = Min(pLink->uShareEnd, uEnd);
		Links.append(pLink);
		uBegin = uSectionEnd;

		ASSERT(uBegin <= uEnd);
		if(uBegin == uEnd)
			break; 

		if(I == m_Joints.begin()){ASSERT(0);	break;}
		I--;
	}
	return Links;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CSharedPartMap
//

void CSharedPartMap::SetRange(uint64 uBegin, uint64 uEnd, ValueType uState, ESet eMode)
{
	CLinkedPartMap::SetRange(uBegin, uEnd, uState, eMode);

	if(uEnd == -1)
		uEnd = GetSize();

	foreach(SPartMapLink* pLink, m_Links)
	{
		CPartMapPtr pMap = pLink->pMap.toStrongRef();
		if(CJoinedPartMap* pJoinedMap = qobject_cast<CJoinedPartMap*>(pMap.data()))
			pJoinedMap->IncrRevision();
	}
}

void CSharedPartMap::SetJoinedRange(uint64 uBegin, uint64 uEnd, ValueType uState, ESet eMode)
{
	if(uEnd == -1)
		uEnd = GetSize();

	foreach(SPartMapLink* pLink, m_Links)
	{
		CPartMapPtr pMap = pLink->pMap.toStrongRef();
		if(CJoinedPartMap* pJoinedMap = qobject_cast<CJoinedPartMap*>(pMap.data()))
			pJoinedMap->SetRange(uBegin + pLink->uShareBegin, uEnd + pLink->uShareBegin, uState, eMode);
	}
}

CSharedPartMap::ValueType CSharedPartMap::GetRange(uint64 uBegin, uint64 uEnd, EMerge eMode, ValueType uMask) const
{
	if(uEnd == -1)
		uEnd = GetSize();

	ValueType uState = CSyncPartMap::GetRange(uBegin, uEnd, eMode, uMask);
	foreach(SPartMapLink* pLink, m_Links)
	{
		CPartMapPtr pMap = pLink->pMap.toStrongRef();
		if(CJoinedPartMap* pJoinedMap = qobject_cast<CJoinedPartMap*>(pMap.data()))
			uState = MergeState(uState, pJoinedMap->GetRawRange(uBegin + pLink->uShareBegin, uEnd + pLink->uShareBegin, eMode, uMask), eUnion); //uState |= pJoinedMap->GetSharedRange(uBegin + pLink->uShareBegin, uEnd + pLink->uShareBegin, eMode, uMask);
	}
	return uState;
}

bool CSharedPartMap::GetNextRange(uint64 &uBegin, uint64 &uEnd, ValueType &uState, ValueType uMask, SIterHint** pHint) const
{
	if(pHint && !*pHint)
		*pHint = AllocHint<SIterHintEx>();

	if(uBegin > uEnd) // if we tel a begin we expect to get the range the begin is in
		uEnd = uBegin;

	// Note: for the range itteration only uEnd is actualyl relevant as input aprameter
	uint64 uLocalBegin = 0;
	uint64 uLocalEnd = uEnd;
	ValueType uLocalState = 0;
	if(!CSyncPartMap::GetNextRange(uLocalBegin, uLocalEnd, uLocalState, uMask))
		return false; // no more ranges

	foreach(SPartMapLink* pLink, m_Links)
	{
		CPartMapPtr pMap = pLink->pMap.toStrongRef();
		if(CJoinedPartMap* pJoinedMap = qobject_cast<CJoinedPartMap*>(pMap.data()))
		{
			uint64 uRemoteBegin = 0;
			uint64 uRemoteEnd = pLink->uShareBegin + uEnd;
			ValueType uRemoteState = 0;
			if(!pJoinedMap->GetNextRawRange(uRemoteBegin, uRemoteEnd, uRemoteState, uMask, pHint ? &((SIterHintEx*)*pHint)->Hint[pJoinedMap] : NULL))
			{
				ASSERT(0);
				continue;
			}
			ASSERT(uRemoteEnd >= pLink->uShareBegin);
			uRemoteEnd -= pLink->uShareBegin;
			if(uRemoteBegin >= pLink->uShareBegin)
				uRemoteBegin -= pLink->uShareBegin;
			else
				uRemoteBegin = 0;

			if(uLocalBegin < uRemoteBegin)
				uLocalBegin = uRemoteBegin;
			if(uLocalEnd > uRemoteEnd)
				uLocalEnd = uRemoteEnd;
			uLocalState = MergeState(uLocalState, uRemoteState, eUnion); //uLocalState |= uRemoteState;
		}
	}

	uBegin = uLocalBegin;
	uEnd = uLocalEnd;
	uState = uLocalState;
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CShareMap
//
/*
void CShareMap::From(const CPartMap* pParts)
{
	CPartMap::SIterator SrcIter;
	while(pParts->IterateRanges(SrcIter))
	{
		CShareMap::ValueType uNewState = 0;
		if((SrcIter.uState & Part::Available) != 0)
			uNewState |= eAvailable;
		if((SrcIter.uState & Part::NeoVerified) != 0)
			uNewState |= eNeoVerified;
		if((SrcIter.uState & Part::Ed2kVerified) != 0)
			uNewState |= eEd2kVerified;
		if((SrcIter.uState & Part::MuleVerified) != 0)
			uNewState |= eMuleVerified;
		SetRange(SrcIter.uBegin, SrcIter.uEnd, uNewState);
	}
}

void CShareMap::To(CPartMap* pParts, CAvailDiff& AvailDiff) const
{
	CShareMap::SIterator SrcIter;
	while(IterateRanges(SrcIter))
	{
		bool bAvailable = ((SrcIter.uState & CShareMap::eAvailable) != 0);
		bool bVerified = ((SrcIter.uState & CShareMap::eNeoVerified) != 0) || ((SrcIter.uState & CShareMap::eEd2kVerified) != 0) || ((SrcIter.uState & CShareMap::eMuleVerified) != 0);

		AvailDiff.Update(SrcIter.uBegin, SrcIter.uEnd, bAvailable ? Part::Available : 0, pParts, Part::Available);
		if(bAvailable)
			pParts->SetRange(SrcIter.uBegin, SrcIter.uEnd, Part::Available | (bVerified ? Part::Verified : 0), CPartMap::eAdd);
		if(!bAvailable || !bVerified)
			pParts->SetRange(SrcIter.uBegin, SrcIter.uEnd, (bAvailable ? 0 : Part::Available) | (bVerified ? 0 : Part::Verified), CPartMap::eClr);
	}
}

bool CShareMap::Read(const CBuffer* Data)
{
	uint64 Size = (--m_PartMap.end()).key();
	uint32 Ranges = Data->ReadValue<uint32>();
	uint8 SizeFlag = Data->ReadValue<uint8>();
	uint8 Bytes = SizeFlag & 0x7F;
	bool bI64 = SizeFlag & 0x80;
	for(;Ranges;Ranges--)
	{
		m_PartMap[bI64 ? Data->ReadValue<uint64>() : Data->ReadValue<uint32>()] = Data->ReadValue<uint8>();
		if(Bytes > 1) // are more bits atached we dont know yet
			Data->ReadData(Bytes - 1); // skip them
	}
	return !(m_PartMap.empty() || Size != (--m_PartMap.end()).key());
}

bool CShareMap::LoadBin(const QByteArray& Array)
{
	CBuffer Buffer(Array, true);
	return Read(&Buffer);
}

void CShareMap::Write(CBuffer* Data, bool bI64) const
{
	Data->WriteValue<uint32>((uint32)m_PartMap.size());
	uint8 SizeFlag = sizeof(ValueType) & 0x7F;
	if(bI64)
		SizeFlag |= 0x80;
	Data->WriteValue<uint8>(SizeFlag);
	for(QMap<uint64,ValueType>::const_iterator Part = m_PartMap.begin(); Part != m_PartMap.end(); Part++)
	{
		if(bI64)
			Data->WriteValue<uint64>(Part.key());
		else
			Data->WriteValue<uint32>(Part.key());
		Data->WriteValue<uint8>(Part.value());
	}
}

QByteArray CShareMap::SaveBin(bool bI64) const
{
	CBuffer Buffer;
	Write(&Buffer, bI64);
	return Buffer.ToByteArray();
}
*/