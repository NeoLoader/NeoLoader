#pragma once

#include "FileHash.h"

class CHashFunction;

struct SHashTreeNode
{
	SHashTreeNode(size_t uSize, SHashTreeNode* left = NULL, SHashTreeNode* right = NULL);
	~SHashTreeNode();

	bool IsLeaf()
	{
		if(!Left || !Right)
		{
			ASSERT(!Left && !Right);
			return true;
		}
		return false;
	}

	SHashTreeNode*				Left;
	SHashTreeNode*				Right;

	byte*						pHash;
};

struct SHashTreeDump
{
	SHashTreeDump(size_t Size, int Length = 100)
	{
		iCount = 0;
		iLength = Length;
		uSize = Size;
		pIDs = (uint64*)malloc(sizeof(uint64) * iLength);
		pHashes = (byte*)malloc(uSize * iLength);
	}
	~SHashTreeDump()
	{
		free(pIDs);
		free(pHashes);
	}

	void Add(uint64 uID, byte* pHash)
	{
		if(iCount >= iLength)
		{
			iLength = iCount + 1;
			iLength += iLength * 25 / 100;
			pIDs = (uint64*)realloc(pIDs, sizeof(uint64) * iLength);
			pHashes = (byte*)realloc(pHashes, uSize * iLength);
		}
		ID(iCount) = uID;
		memcpy(Hash(iCount), pHash, uSize);
		iCount++;
	}
	int Count()				{return iCount;}
	size_t Size()			{return uSize;}
	__inline uint64& ID(int Index)	{return pIDs[Index];}
	__inline byte* Hash(int Index)	{return pHashes + (Index * uSize);}

	int iCount;
	int iLength;
	size_t uSize;
	uint64* pIDs;
	byte* pHashes;
};

class CFileHashTree: public CFileHashEx
{
	Q_OBJECT

public:
	CFileHashTree(EFileHashType eType, uint64 TotalSize, uint64 BlockSize = -1, uint64 PartSize = -1, int DepthLimit = -1); 
	virtual ~CFileHashTree();

	virtual bool				SetRootHash(const QByteArray& RootHash);
	virtual QByteArray			GetRootHash() const;

	virtual uint64				GetBlockSize()	{if(m_BlockSize == -1) return 0; return m_BlockSize;}
	virtual uint64				GetPartSize()	{if(m_PartSize == -1) return 0; return m_PartSize;}

	virtual int					GetDepthLimit()	{return m_DepthLimit;}

	virtual bool				CanHashParts()	{QReadLocker Locker(&m_TreeMutex); return IsFullyResolved(m_PartSize != -1 ? m_PartSize : m_BlockSize);}
	virtual bool				CanHashBlocks()	{QReadLocker Locker(&m_TreeMutex); return IsFullyResolved();}

	//virtual QList<SPart>		GetRanges(uint64 uFrom, uint64 uTo, bool bEnvelope = true);
	virtual bool				IsComplete()	{return IsValid() && CanHashBlocks();}
	virtual bool				IsLoaded()		{QReadLocker Locker(&m_TreeMutex); return m_TreeRoot != NULL;}
	virtual void				Unload();
	virtual bool				SetHashSet(const QList<QByteArray>& HashSet);
	virtual QList<QByteArray>	GetHashSet();

	virtual bool				IsFullyResolved(uint64 uResolution = -1) {return IsFullyResolved(0, -1, uResolution);}
	virtual bool				IsFullyResolved(uint64 uBegin, uint64 uEnd, uint64 uResolution = -1);

	virtual CFileHash*			Clone(bool bFull);

	////////////////////////////////////////////////////////////////////////////////
	// operative part
	virtual bool				Verify(QIODevice* pFile, CPartMap* pPartMap, uint64 uFrom, uint64 uTo, EHashingMode Mode);
	virtual bool 				Calculate(QIODevice* pFile);

	virtual bool				AddLeafs(CFileHashTree* TreeRoot);
	virtual CFileHashTree*		GetLeafs(uint64 uFrom, uint64 uTo); 

	virtual bool				Save(SHashTreeDump& Leafs, int DepthLimit = -1);
	virtual bool				Load(SHashTreeDump& Leafs);
	virtual QByteArray			SaveBin(int DepthLimit = -1);
	virtual bool 				LoadBin(const QByteArray& Array);

protected:
	virtual bool				SetTree(SHashTreeNode* TreeRoot);

	virtual uint64				GetTreeSize();
	enum EBalance
	{
		eLeft,
		eRight,
	};
	virtual uint64				GetBrancheSize(uint64 uBegin, uint64 uEnd, EBalance Balance);
	virtual SHashTreeNode*		CalculateRange(QIODevice* pFile, uint64 uBegin, uint64 uEnd, EBalance Balance, int Depth, CHashFunction& Hash);
	virtual SHashTreeNode*		CalculateLeaf(QIODevice* pFile, uint64 uBegin, uint64 uEnd, CHashFunction& Hash);
	virtual bool				CalculateTree(SHashTreeNode* Branche, CHashFunction& Hash);
	virtual void				CalculateBranche(SHashTreeNode* Branche, CHashFunction& Hash);
	virtual void				TrimTree(SHashTreeNode* Branche, int Depth); 

	virtual	bool				VerifyBranche(QIODevice* pFile, CPartMap* pPartMap, uint64 uFrom, uint64 uTo, EHashingMode Mode, SHashTreeNode* Branche, uint64 uBegin, uint64 uEnd, EBalance Balance, CHashFunction& Hash);

	virtual bool				IsFullyResolved(uint64 uBegin, uint64 uEnd, EBalance Balance, SHashTreeNode* Branche, uint64 uFrom, uint64 uTo, uint64 uResolution, int Depth);

	virtual SHashTreeNode* 		Expose(uint64 uBegin, uint64 uEnd, EBalance Balance, SHashTreeNode* Branche, uint64 uFrom, uint64 uTo);

	virtual void				Merge(SHashTreeNode* SourceBranche, SHashTreeNode*& TargetBranche);
	virtual bool				Extract(SHashTreeNode* SourceBranche, uint64 uBegin, uint64 uEnd, EBalance Balance, SHashTreeNode* TargetBranche, uint64 uFrom, uint64 uTo, uint64 uResolution);

	//virtual QList<SPart>		GetRanges(uint64 uBegin, uint64 uEnd, EBalance Balance, SHashTreeNode* Branche, uint64 uFrom, uint64 uTo, bool bEnvelope);

	virtual bool				Save(SHashTreeDump &Leafs, SHashTreeNode* Branche, uint64 TreePath, int DepthLimit);
	virtual void				Load(byte* Leaf, uint64 TreePath, int ToGo, SHashTreeNode* Branche);

	uint64						m_BlockSize;
	uint64						m_PartSize;
	SHashTreeNode*				m_TreeRoot;
	int							m_DepthLimit;

	mutable QReadWriteLock		m_TreeMutex;
};

UINT Neo2Merkle(UINT N);
UINT Merkle2Neo(UINT M);
