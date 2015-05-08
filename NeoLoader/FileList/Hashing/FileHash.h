#pragma once

class CFileHashTree;
class CFileHashSet;

#include "../PartMap.h"
#include "../../../Framework/ObjectEx.h"

enum EFileHashType 
{
	HashNone = 0, 

	// Hash methods used for file hashing and coruption detection
	HashNeo,
	HashXNeo,
	HashTorrent,		// There actually is no such thing, torrent hashes are always for a groupe of files + metadata
	HashEd2k,
	HashArchive,

	HashMule,			// This hash can not exist alone
	HashEdPr,			// This is a placeholder for eDonkeyHybrid reviced protocol crumb hash set
	HashTigerTree,		// This one can be supported but is extremly unportformant to calculate

	// other hash methods
	HashMD5,
	HashSHA1,
	HashSHA2,

	HashSingle,
	HashMulti,

	HashUnknown
};

enum EHashingMode
{
	eVerifyParts,
	eVerifyFile,
	eRecoverParts
};

#define HASH_NEO		"neo"	// it reads Neo Hash
#define HASH_XNEO		"neox"	// it reads Neo Cross Hash
#define HASH_NEO_EX		"tree:sha2/256k"
#define HASH_XNEO_EX	"x-tree:sha2/256k"
#define HASH_ARCH		"arch"
#define HASH_ED2K		"ed2k"
#define HASH_MULE		"aich"
#define HASH_BT			"btih"
#define HASH_MD5		"md5"
#define HASH_TTH		"tigertree"
#define HASH_TTH_EX		"tree:tiger/1024"
#define HASH_SHA1		"sha1"
#define HASH_SHA2		"sha2"

#define	ED2K_PARTSIZE	((uint64)9728000)
#define ED2K_CRUMBSIZE	((uint64)486400)
#define ED2K_BLOCKSIZE	((uint64)184320)

#define NEO_BLOCKSIZE	KB2B(256)
#define NEO_TREEDEPTH	12

#define TTH_BLOCKSIZE	KB2B(1)
#define TTH_TREEDEPTH	8

__inline uint64 DivUp(uint64 Size, uint64 Value)
{
	//int Count = Size / Value;
	//if(Size % Value)
	//	Count++;
	//return Count;
	return (Size + Value - 1)/Value;
}

void* hash_malloc(size_t size);
void hash_free(void* ptr);

class CFileHash: public QObjectEx
{
	Q_OBJECT

public:
	CFileHash(EFileHashType eType);
	virtual ~CFileHash();

	static	CFileHash*			FromString(QByteArray String, EFileHashType eType = HashNone, uint64 uFileSize = 0);
	static	QByteArray			DecodeHash(EFileHashType eType, QByteArray String);
	static	QByteArray			EncodeHash(EFileHashType eType, QByteArray Hash, int Base = 0);
	static	CFileHash*			New(EFileHashType eType = HashNone, uint64 uFileSize = 0);
	static	CFileHash*			FromArray(QByteArray Hash, EFileHashType eType = HashNone, uint64 uFileSize = 0);
	virtual QByteArray			ToString(int Base = 0);
	static	size_t				GetSize(EFileHashType eType);
	virtual	UINT				GetAlgorithm();
	static	int					GetBase(EFileHashType eType);
	static	EFileHashType		Str2HashType(QString Type);
	static	QString				HashType2Str(EFileHashType eType, bool bExt = false);



	virtual EFileHashType		GetTypeClass() const;

	inline size_t				GetSize() const			{return m_uSize;}
	inline EFileHashType		GetType() const			{return m_eType;}


	/*struct SPart
	{
		SPart(uint64 Begin = 0, uint64 End = 0)
		: uBegin(Begin), uEnd(End) {}
		uint64 uBegin;
		uint64 uEnd;
	};*/

	virtual bool				SetHash(const QByteArray& Hash);
	virtual QByteArray			GetHash() const			{QReadLocker Locker(&m_HashMutex); return m_Hash;}
	virtual bool				IsValid();
	virtual bool				IsComplete();

	virtual bool				Compare(const CFileHash* pHash) const;
	virtual bool				Compare(const byte* pBuffer) const;

	////////////////////////////////////////////////////////////////////////////////
	// operative part
	virtual bool 				Calculate(QIODevice* pFile);

protected:
	friend class CHashingThread;

	virtual void				SetHash(const byte* pBuffer);
	static	bool				IsValid(const byte* pHash, size_t uSize);

	// Note: we are allowed to access size and type without a mutex as it can only change during instantiation
	EFileHashType				m_eType;
	size_t						m_uSize; 
	QByteArray					m_Hash;

	mutable QReadWriteLock		m_HashMutex;
};

typedef QSharedPointer<CFileHash> CFileHashPtr;
typedef QWeakPointer<CFileHash> CFileHashRef;


//////////////////////////////
// CFileHashEx

typedef QPair<uint64, uint64> TPair64;

class CFileHashEx: public CFileHash
{
	Q_OBJECT

public:
	CFileHashEx(EFileHashType eType, uint64 TotalSize) : CFileHash(eType) {m_TotalSize = TotalSize;}

	virtual uint64				GetTotalSize()	{return m_TotalSize;}

	virtual bool				SetHashSet(const QList<QByteArray>& HashSet) = 0;
	virtual QList<QByteArray>	GetHashSet() = 0;

	// Note: We should eider have a part size or a block size, except for AICH
	virtual uint64				GetBlockSize()			{return 0;}
	virtual uint64				GetPartSize()			{return 0;}

	virtual bool				CanHashParts()			{return false;}
	virtual bool				CanHashBlocks()			{return false;}

	virtual uint32				GetStatusCount();

	virtual QBitArray			GetStatusMap()			{QReadLocker Locker(&m_StatusMutex); if(m_StatusMap.isEmpty()) m_StatusMap.resize(GetStatusCount()); return m_StatusMap;}
	virtual QList<TPair64> GetCorruptionSet()		{QReadLocker Locker(&m_StatusMutex); return m_CorruptionSet;} 

	virtual void				ResetStatus(bool bFull = true)	{QWriteLocker Locker(&m_StatusMutex); if(bFull) m_StatusMap.clear(); m_CorruptionSet.clear();}
	virtual void				SetStatus(const QBitArray& StatusMap, const QList<TPair64>& CorruptionSet = QList<TPair64>())	
														{QWriteLocker Locker(&m_StatusMutex); m_StatusMap = StatusMap; m_CorruptionSet = CorruptionSet;}

	virtual bool				IsLoaded() = 0;
	virtual void				Unload() = 0;

	virtual CFileHash*			Clone(bool bFull) = 0;

	////////////////////////////////////////////////////////////////////////////////
	// operative part
	virtual bool				Verify(QIODevice* pFile, CPartMap* pPartMap, uint64 uFrom, uint64 uTo, EHashingMode Mode) = 0;
	virtual void				SetResult(bool bResult, uint64 uBegin, uint64 uEnd);
	virtual void				ClearResult(uint64 uBegin, uint64 uEnd);
	virtual bool				GetResult(uint64 uBegin, uint64 uEnd); // returns only positive results

	virtual QPair<uint32, uint32> IndexRange(uint64 uBegin, uint64 uEnd, bool bEnvelope = true);
	virtual uint64				IndexOffset(uint32 Index);

protected:
	uint64						m_TotalSize;

	mutable QReadWriteLock		m_StatusMutex;
	QBitArray					m_StatusMap;
	// Note: we work under the assumption that there will not be much corruption
	QList<TPair64>				m_CorruptionSet;
};

__inline bool testBits(const QBitArray& BitArray, QPair<uint32, uint32> Range)
{
	if(BitArray.isEmpty())
		return false;
	for(uint32 Index = Range.first; Index < Range.second; Index++)
	{
		if(!BitArray.testBit(Index))
			return false;
	}
	return true;
}

__inline bool testRange(const QList<TPair64>& Set, const TPair64& Val)
{
	foreach(const TPair64 &Cur, Set)
	{
		if(Cur.first < Val.second && Cur.second > Val.first)
			return true; // we have at least a partial match
	}
	return false;
}