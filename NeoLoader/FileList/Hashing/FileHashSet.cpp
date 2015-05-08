#include "GlobalHeader.h"
#include "FileHashSet.h"
#include "../../../Framework/Cryptography/HashFunction.h"

CFileHashSet::CFileHashSet(EFileHashType eType, uint64 TotalSize, uint64 PartSize)
: CFileHashEx(eType, TotalSize), m_SetMutex(QReadWriteLock::Recursive)
{
	switch(eType)
	{
		case HashEd2k:		
			ASSERT(PartSize == -1 || PartSize == ED2K_PARTSIZE);	
			m_PartSize = ED2K_PARTSIZE;	
			break;
		case HashTorrent:	
			if(PartSize == -1 ) 
				m_PartSize = KB2B(64); 
			else	
				m_PartSize = PartSize; 
			break;
		default: 
			m_PartSize = -1;
			ASSERT(0);
	}
}

CFileHashSet::~CFileHashSet()
{
	Unload();
}

void CFileHashSet::Unload()
{
	QWriteLocker Locker(&m_SetMutex);

	foreach(byte* PartHash, m_HashSet)
		hash_free(PartHash);
	m_HashSet.clear();
}

bool CFileHashSet::SetHashSet(const QList<QByteArray>& HashSet)
{
	ASSERT(!CanHashParts());
	if(HashSet.size() != GetPartCount() || HashSet.isEmpty() || HashSet.at(0).size() != m_uSize)
	{
		ASSERT(0);
		return false;
	}

	m_HashSet.reserve(HashSet.size());
	foreach(const QByteArray& Hash, HashSet)
	{
		byte* pHash = (byte*)hash_malloc(m_uSize);
		memcpy(pHash,Hash.data(),m_uSize);

		QWriteLocker Locker(&m_SetMutex);
		m_HashSet.append(pHash);
	}

	if(!CalculateRoot())
	{
		Unload();
		return false;
	}
	return true;
}

QList<QByteArray> CFileHashSet::GetHashSet()
{
	QReadLocker Locker(&m_SetMutex);

	ASSERT(CanHashParts());

	QList<QByteArray> HashSet;
	foreach(byte* pHash, m_HashSet)
		HashSet.append(QByteArray((char*)pHash, (int)GetSize()));
	return HashSet;
}

/** 
* GetPartSize: returns the size of a particular part, 
*				all parts except the last one have the same fixed size.
*				The last part is smaller and ends with the end of the file.
*
* @param: Index:	The index of the part
* @return:			The size of the part
*/
uint64 CFileHashSet::GetPartSize(int Index)
{
	QReadLocker Locker(&m_SetMutex);
	uint32 PartCount = GetPartCount();
	ASSERT(Index < PartCount);
	if(Index + 1 == PartCount) // last part
	{
		uint64 uSize = m_TotalSize % m_PartSize;
		if(uSize == 0) // in case the file is an exact multiple of the part size
			uSize = m_PartSize;
		return uSize;
	}
	return m_PartSize;
}

/** 
* GetPartCount: returns the amount of parts
*				For ed2khash sets it uses edonkeys original flaws calculation method
*				for files that are an exact multiple of a part size it returns one part to much
*
* @return:			The about of parts
*/
uint32 CFileHashSet::GetPartCount()
{
	switch(m_eType)
	{
		case HashEd2k:	return m_TotalSize / m_PartSize + 1;
		default:		return DivUp(m_TotalSize, m_PartSize);
	}
}

/*QList<CFileHashSet::SPart> CFileHashSet::GetRanges(uint64 uBegin, uint64 uEnd, bool bEnvelope)
{
	ASSERT(uBegin < uEnd);
	if(uEnd == -1)
		uEnd = m_TotalSize;
	uint32 FirstPart = bEnvelope ? uBegin / m_PartSize : DivUp(uBegin, m_PartSize);
	uint32 LastPart = (uEnd == m_TotalSize || bEnvelope) ? DivUp(uEnd, m_PartSize) : uEnd / m_PartSize;
	QList<SPart> Ranges;
	for(;FirstPart < LastPart; FirstPart++)
	{
		uint64 uOffset = FirstPart * GetPartSize();
		Ranges.append(SPart(uOffset, uOffset + GetPartSize(FirstPart)));
	}
	return Ranges;
}*/

////////////////////////////////////////////////////////////////////////////////
// operative part

/** 
* Verify: Verifyed all available but not verifyed segments of the file,
*			Complete parts that fail the verification are cleared.
*
* @param: pFile:	Pointer to a random access IO device representing the file, It must be a exclusivly used instance.
* @param: pPartMap:	Part map that contains the state of all file segments
* @param: bAll:		Verify All parts also those already marked as verifyed
* @return:			true if it was posible to verify, or false if for some reason the oepration can nto be performed
*/

bool CFileHashSet::Verify(QIODevice* pFile, CPartMap* pPartMap, uint64 uFrom, uint64 uTo, EHashingMode Mode)
{
	ResetStatus(false); // clear corruption list

	QReadLocker Locker(&m_SetMutex);

	if(m_HashSet.isEmpty())
		return false;

	ASSERT(!pPartMap || pPartMap->GetSize() == m_TotalSize);

	int PartCount = m_HashSet.count();
	uint64 uPartSize = GetPartSize();

	for(int i = 0; i < PartCount; i ++)
	{
		uint64 uBegin = i * uPartSize;
		uint64 uEnd = uBegin + uPartSize;
		if(uEnd > m_TotalSize)
			uEnd = m_TotalSize;

		if(!(uFrom < uEnd && uTo > uBegin))
			continue;

		if(Mode == eVerifyParts) // we want to hash only new parts
		{
			if(!m_StatusMap.isEmpty() && m_StatusMap.testBit(i)) 
				continue; // already verified

			if((pPartMap->GetRange(uBegin, uEnd) & Part::Available) == 0)
				continue; // this part is not complete
		}

		byte* pHash = CalculatePart(pFile, uBegin, uEnd);
		bool bResult = pHash && memcmp(m_HashSet[i], pHash, m_uSize) == 0;
		hash_free(pHash);

		if(Mode != eRecoverParts || bResult) // dont set corruption for recovery runs, ok shoudl have already ben cleared anyways
			SetResult(bResult, uBegin, uEnd);
	}

	return true;
}

/** 
* Calculate: Calculates a hashset for a complete file.
*
* @param: pFile:	Pointer to a random access IO device representing the file, It must be a exclusivly used instance.
* @return:			true if it was posible to calculate, or false if for some reason the oepration can nto be performed
*/
bool CFileHashSet::Calculate(QIODevice* pFile)
{
	Unload();

	if(!pFile->size())
	{
		ASSERT(0);
		return false;
	}
	ASSERT(!m_TotalSize || m_TotalSize == pFile->size());
	m_TotalSize = pFile->size();
	
	SetHash(NULL);

	uint64 uPartSize = GetPartSize();

	int Count = GetPartCount();
	for(int i = 0; i < Count; i++)
	{
		uint64 uBegin = i * uPartSize;
		uint64 uEnd = uBegin + uPartSize;
		if(uEnd > m_TotalSize)
			uEnd = m_TotalSize;

		byte* pHash = CalculatePart(pFile, uBegin, uEnd);
		if(!pHash)
			return false;

		QWriteLocker Locker(&m_SetMutex);
		m_HashSet.append(pHash);
	}

	return CalculateRoot();
}

/** 
* CalculatePart: Calculates a hashset for a specifyed part
*
* @param: pFile:	Pointer to a random access IO device representing the file.
* @param: uBegin:	Part Start Position
* @param: uEnd:		Part End Position (uEnd - uBegin == uPartSize)
* @return:			Pointer to a byte array containing the part hash
*/
byte* CFileHashSet::CalculatePart(QIODevice* pFile, uint64 uBegin, uint64 uEnd)
{
	//ASSERT(uBegin < uEnd); // ed2k
	if((qint64)uEnd > pFile->size())
		return NULL; // file is not complete

	//ASSERT(m_eType == HashEd2k || m_eType == HashTorrent);
	CHashFunction Hash(GetAlgorithm());
	ASSERT(Hash.IsValid());
	
	pFile->seek(uBegin);
	quint64 uSize = uEnd - uBegin;
	const size_t BuffSize = 16*1024;
	char Buffer[BuffSize];
	for(quint64 uPos = 0; uPos < uSize;)
	{
		quint64 uToGo = BuffSize;
		if(uPos + uToGo > uSize)
			uToGo = uSize - uPos;
		qint64 uRead = pFile->read(Buffer, uToGo);
		if(uRead < 1)
			return NULL;
		Hash.Add((byte*)Buffer, uRead);
		uPos += uRead;
	}

	Hash.Finish();
	byte* pHash = (byte*)hash_malloc(m_uSize);
	ASSERT(Hash.GetSize() == m_uSize);
	memcpy(pHash,Hash.GetKey(),m_uSize);
	return pHash;
}

/** 
* CalculateRoot: Calculates the root hash from a full hashset
* @return:			Success or fail
*
*/
bool CFileHashSet::CalculateRoot()
{
	if(m_HashSet.isEmpty())
		return false;

	if(m_eType == HashTorrent)
		return true;

	if(m_HashSet.count() == 1)
		SetHash(m_HashSet.first());
	else
	{
		QWriteLocker Locker(&m_SetMutex);

		CHashFunction Hash(GetAlgorithm());
		ASSERT(Hash.IsValid());
		foreach(byte* PartHash, m_HashSet)
			Hash.Add(PartHash, m_uSize);
		Hash.Finish();
		ASSERT(Hash.GetSize() == m_uSize);
		if(!IsValid())
			SetHash(Hash.GetKey());
		else if(!Compare(Hash.GetKey()))
			return false;
	}
	return true;
}

QByteArray CFileHashSet::SaveBin()
{
	QReadLocker Locker(&m_SetMutex);

	CBuffer Buffer;

	uint32 PartCount = m_HashSet.size();
	Buffer.WriteValue<uint32>(PartCount);

	foreach(byte* PartHash, m_HashSet)
		Buffer.WriteData(PartHash, m_uSize);

	return Buffer.ToByteArray();
}

bool CFileHashSet::LoadBin(const QByteArray& Array)
{
	ASSERT(m_HashSet.isEmpty());

	CBuffer Buffer(Array, true);

	uint32 PartCount = Buffer.ReadValue<uint32>();
	m_HashSet.reserve(PartCount);
	for(uint32 i=0; i<PartCount; i++)
	{
		if(byte* pData = Buffer.ReadData(m_uSize))
		{
			byte* pHash = (byte*)hash_malloc(m_uSize);
			memcpy(pHash, pData, m_uSize);
			m_HashSet.append(pHash);
		}
		else
			return false;
	}

	if(!CalculateRoot())
	{
		Unload();
		LogLine(LOG_ERROR, tr("Invalid Hash Set for %1").arg(QString(ToString())));
		return false;
	}
	return true;
}

CFileHash* CFileHashSet::Clone(bool bFull)
{
	CFileHashSet* pHashSet = new CFileHashSet(m_eType, m_TotalSize, m_PartSize);
	pHashSet->SetHash(GetHash());
	if(bFull)
		pHashSet->SetHashSet(GetHashSet());
	return pHashSet;
}