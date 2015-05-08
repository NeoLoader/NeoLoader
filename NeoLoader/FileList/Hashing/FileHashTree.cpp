#include "GlobalHeader.h"
#include "FileHashTree.h"
#include "../../../Framework/Cryptography/HashFunction.h"
//#include <math.h>

SHashTreeNode* allocNode(size_t uSize, SHashTreeNode* left = NULL, SHashTreeNode* right = NULL)
{
	void* mem = hash_malloc(sizeof(SHashTreeNode));
	SHashTreeNode* ptr = new(mem) SHashTreeNode(uSize, left, right);
	return ptr;
}

void freeNode(SHashTreeNode* ptr)
{
	if(ptr)
	{
		ptr->~SHashTreeNode();
		hash_free(ptr);
	}
}

SHashTreeNode::SHashTreeNode(size_t uSize, SHashTreeNode* left, SHashTreeNode* right)
{
	Left = left;
	Right = right;

	pHash = (byte*)hash_malloc(uSize);
	//pHash = new byte[uSize];
}

SHashTreeNode::~SHashTreeNode()
{
	freeNode(Left);
	freeNode(Right);

	hash_free(pHash);
	//delete pHash;
}

CFileHashTree::CFileHashTree(EFileHashType eType, uint64 TotalSize, uint64 BlockSize, uint64 PartSize, int DepthLimit) 
: CFileHashEx(eType, TotalSize), m_TreeMutex(QReadWriteLock::Recursive)
{
	m_BlockSize = -1;
	m_PartSize = -1;
	m_DepthLimit = 64; // infinite // Note: a depth of 64 means 2^64 leaft we wont reach that realisticly

	switch(eType)
	{
		case HashNeo:
		case HashXNeo:
			m_BlockSize = NEO_BLOCKSIZE;
			ASSERT(BlockSize == -1 || BlockSize == NEO_BLOCKSIZE);
			ASSERT(PartSize == -1);
			if(DepthLimit == -1 ) 
				m_DepthLimit = NEO_TREEDEPTH; 
			else	
				m_DepthLimit = DepthLimit; 
			break;
		case HashMule:
			m_BlockSize = ED2K_BLOCKSIZE;
			ASSERT(BlockSize == -1 || BlockSize == ED2K_BLOCKSIZE);
			m_PartSize = ED2K_PARTSIZE;
			ASSERT(PartSize == -1 || PartSize == ED2K_PARTSIZE);
			ASSERT(DepthLimit == -1);
			break;
		case HashTigerTree:
			m_BlockSize = TTH_BLOCKSIZE;
			ASSERT(BlockSize == -1 || BlockSize == TTH_BLOCKSIZE);
			ASSERT(PartSize == -1);
			if(DepthLimit == -1 ) 
				m_DepthLimit = TTH_TREEDEPTH; 
			else	
				m_DepthLimit = DepthLimit; 
			break;
		case HashTorrent:
			ASSERT(PartSize == -1);
			if(BlockSize == -1) 
				m_BlockSize = KB2B(64); 
			else	
				m_BlockSize = BlockSize; 
			ASSERT(DepthLimit == -1);
			break;
		default: 
			ASSERT(0);
	}

	ASSERT(m_BlockSize != -1);
	ASSERT(m_PartSize == -1 || eType == HashMule);

	m_TreeRoot = NULL;
}

CFileHashTree::~CFileHashTree()
{
	Unload();
}

/**
* removes the tree;
*/
void CFileHashTree::Unload()
{
	QWriteLocker Locker(&m_TreeMutex);

	freeNode(m_TreeRoot);
	m_TreeRoot = NULL;
}

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
bool CFileHashTree::Verify(QIODevice* pFile, CPartMap* pPartMap, uint64 uFrom, uint64 uTo, EHashingMode Mode)
{
	ResetStatus(false); // clear corruption list

	QReadLocker Locker(&m_TreeMutex);

	if(!m_TreeRoot)
		return false;

	ASSERT(pPartMap->GetSize() == m_TotalSize);

	CHashFunction Hash(GetAlgorithm());
	ASSERT(Hash.IsValid());

	return VerifyBranche(pFile, pPartMap, uFrom, uTo, Mode, m_TreeRoot, 0, GetTreeSize(), eLeft, Hash);
}

uint64 CFileHashTree::GetTreeSize()
{
	if(GetType() == HashTorrent)
	{	
		int TotalCount = DivUp(m_TotalSize, m_BlockSize);

		// round up to nearest 2 exponent
		int Pos; // Note: this is buggy, if the value already is an power of two thre result is to high, but libtorrent does this thisway
		for (Pos = 0; TotalCount > 0; TotalCount >>= 1, ++Pos);
		TotalCount = 1 << Pos;

		return TotalCount*m_BlockSize;
	}
	return m_TotalSize;
}

/** 
* Calculate: Calculates a hashtree for a complete file.
*
* @param: pFile:	Pointer to a random access IO device representing the file, It must be a exclusivly used instance.
* @return:			true if it was posible to calculate, or false if for some reason the oepration can nto be performed
*/
bool CFileHashTree::Calculate(QIODevice* pFile)
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

	CHashFunction Hash(GetAlgorithm());
	ASSERT(Hash.IsValid());

	SHashTreeNode* TreeRoot = CalculateRange(pFile,0,GetTreeSize(),eLeft,1, Hash);
	if(!TreeRoot || !SetTree(TreeRoot))
		return false;
	return true;
}

bool CFileHashTree::SetTree(SHashTreeNode* TreeRoot)
{
	QWriteLocker Locker(&m_TreeMutex);
	if(m_eType == HashTorrent)
		; // no verification or what ever
	else if(!IsValid())
		SetHash(TreeRoot->pHash);
	else if(!Compare(TreeRoot->pHash))
	{
		freeNode(TreeRoot);
		return false;
	}
	freeNode(m_TreeRoot);
	m_TreeRoot = TreeRoot;
	return true;
}

/** 
* TrimTree: Trimms all brnaches below a gien depth, thos reducing the storage requirements for the tree
*
* @param: Branche:	Branche to trim
* @param: Depth:	Depth to trimm to
*/
void CFileHashTree::TrimTree(SHashTreeNode* Branche, int Depth)
{
	if(Branche->IsLeaf())
		return;

	if(--Depth > 0)
	{
		TrimTree(Branche->Left, Depth);
		TrimTree(Branche->Right, Depth);
	}
	else
	{
		freeNode(Branche->Left);
		Branche->Left = NULL;
		freeNode(Branche->Right);
		Branche->Right = NULL;
	}
}

/** 
* GetBrancheSize: calculates teh size of the left branche for a given range.
*
* @param: uBegin:	Begin of the range
* @param: uEnd:		end of the range
* @return:			branch size, If the range is the range is one block or less uSize == uEnd - uBegin, this means the branche si a leaf
*/
uint64 CFileHashTree::GetBrancheSize(uint64 uBegin, uint64 uEnd, EBalance Balance)
{
	ASSERT(uBegin < uEnd);
	uint64 uSize = uEnd - uBegin;

	uint64 BlockSize = m_BlockSize;
	if(m_PartSize != -1)
	{
		/* Comment copyed form eMule (Fair Use):
		 SHA haset basically exists of 1 Tree for all Parts (9.28MB) + n  Trees
		 for all blocks (180KB) while n is the number of Parts.
		 This means it is NOT a complete hashtree, since the 9.28MB is a given level, in order
		 to be able to create a hashset format similar to the MD4 one.

		 If the number of elements for the next level are odd (for example 21 blocks to spread into 2 hashs)
		 the majority of elements will go into the left branch if the parent node was a left branch
		 and into the right branch if the parent node was a right branch. The first node is always
		 taken as a left branch.

		Example tree:
			FileSize: 19506000 Bytes = 18,6 MB

										X (18,6)                                   MasterHash
									 /     \
								 X (18,55)   \
							/		\	       \
						   X(9,28)  x(9,28)   X (0,05MB)						   PartHashs
					   /      \    /       \        \
				X(4,75)   X(4,57) X(4,57)  X(4,75)   \

								[...............]
		X(180KB)   X(180KB)  [...] X(140KB) | X(180KB) X(180KB [...]			   BlockHashs
											v
								 Border between first and second Part (9.28MB)
		*/
		
		if(uSize > m_PartSize) // the range spans over multiple parts
			BlockSize = m_PartSize;
		else // the range is containd in a singel part
		{
			uint64 uOffset = uBegin - (uBegin % m_PartSize);

			uBegin -= uOffset;
			uEnd -= uOffset;

			ASSERT(uEnd <= m_PartSize);
		}
	}
	ASSERT(BlockSize != -1);

	ASSERT(uBegin % BlockSize == 0);
	int Count = DivUp(uSize, BlockSize);
	ASSERT(Count); // shouldnt actually ever be 0 (unless the file is empty but than we should have abborted already)
	if(Count <= 1) // partial block
		return uSize;

	uint64 LeftCount = (Count >> 1); //Count/2
	ASSERT(LeftCount); // must not be 0!

	if(GetType() == HashTigerTree || GetType() == HashTorrent)
	{
		/* 
		 Note: in a Tiger Hash Tree tree the left side must always be the next greater or equal power of 2
		Example tree:
										   _______________________________ X (10,5KB)___________________________
										  /																		\
							  _________  X (8KB) ___________										  _________  X (2,5KB) _________							
							 /                              \										 /                              \							
					   X(4KB)								 X(4KB)							   X(2KB)								 X(1,5KB)					
					/		 \					            /		 \						/		 \					            /		 \					
				X(2KB)      X(2KB)			          X(2KB)      X(2KB)				X(1KB)      X(1KB)			          X(1KB)      X(0,5KB)				
			   /      \    /      \                  /      \    /      \					   
		 X(1KB)   X(1KB)  X(1KB)  X(1KB)       X(1KB)   X(1KB)  X(1KB)  X(1KB)
		*/

		/* 
		 Note: in a Torrent Hash Tree tree the base must be balanced with padding
				This is achived by modifying uEnd at the begin of the calculation
		Example tree:
										   _______________________________ X (10,5KB)_______________________________
										  /																			\
							  _________  X (8KB) ___________											  _________  X (2,5KB) _________							
							 /                              \											 /                              \							
					   X(4KB)								 X(4KB)								   X(2,5KB)								 X(0KB)					
					/		 \					            /		 \							/		 \					            /	   \					
				X(2KB)      X(2KB)			          X(2KB)      X(2KB)					X(1KB)      X(=0,5KB)		          X(0KB)      X(0KB)				
			   /      \    /      \                  /      \    /      \				   /      \    /       \                 /      \    /      \					   
		 X(1KB)   X(1KB)  X(1KB)  X(1KB)       X(1KB)   X(1KB)  X(1KB)  X(1KB)		 X(1KB)   X(1KB)  X(0,5KB) 0				 0		0    0		0
		*/

		//LeftCount = pow(2,ceil(log((double)LeftCount)/log(2.0)));
		//sample:
		//	befoure	1000000 0100000 1100000 0010000 1010000 0110000 1110000 0001000 1001000 0101000 1101000 0011000 1011000 0111000 1111000 0000100
		//	after	1000000 0100000 0010000 0010000 0001000 0001000 0001000 0001000 0000100 0000100 0000100 0000100 0000100 0000100 0000100 0000100
		const int len = sizeof(LeftCount)<<3;
		int pos = 0;
		while(LeftCount >> (pos+1)) // find highest order bit
			pos ++;
		if(LeftCount << (len - pos)) // increment it by one if any lower order bit is set
			pos ++;
		LeftCount = ((uint64)1 << pos);
	}
	else // Mule or Neo Tree
	{
		/*
		Example tree: 7 leafs; 4 leafs, 3 leafs 2 leafs
							 _________________X(1,8MB)_______________
							/										 \
				   ______X(1MB)_________						   __X(896KB)__
				  /						\						  /			   \
			    X(512KB)			   X(512KB)				  X(512KB)			\
			  /			\			  /		   \			 /		  \			 \
		X(256KB)	X(256KB)	X(256KB)	X(256KB)	X(256KB)	X(256KB)	X(128KB)

		Example tree: 11 leafs; 6 leafs, 5 leafs
										  _________________________X(2,6MB)_________________________
										 /															\
						  _____________X(1,5MB)___________									  _______X(1,1MB)______
						 /			  					  \									 /			  		   \
					 __X(768KB)_						__X(768KB)_						 __X(768KB)_			    \
					/			\					   /		   \					/			\				 \
			   X(512KB)			 \				 X(512KB)			\			   X(512KB)			 \				 X(384KB)
			  /       \			  \				/		\			 \			  /       \			  \				/		\
		X(256KB)   X(256KB)		X(256KB)	X(256KB)	X(256KB)	X(256KB)	X(256KB)   X(256KB)	X(256KB)	X(256KB)	X(128KB)
		*/

		if(Balance != eRight && (Count & 1)) //Count % 2
			LeftCount++;	
	}

	return LeftCount * BlockSize;
}

/** 
* CalculateRange: setup the tree and calculates all leaf hashes, for a given range
*
* @param: pFile:	Pointer to a random access IO device representing the file, It must be a exclusivly used instance.
* @param: uBegin:	Begin of the range
* @param: uEnd:		end of the range
* @Param: Balance:	specifyes how to balance the branche, it san be left or right depanding of if its the left or the right branche.
* @param: Depth:	current depth
* @return:			Tree nodefor the given range towh set up sub noned down to leafs
*/
SHashTreeNode* CFileHashTree::CalculateRange(QIODevice* pFile, uint64 uBegin, uint64 uEnd, EBalance Balance, int Depth, CHashFunction& Hash)
{
	ASSERT(uBegin < uEnd);

	uint64 BrancheSize = GetBrancheSize(uBegin, uEnd, Balance);
	if(BrancheSize == 0)
	{
		ASSERT(0);
		return NULL;
	}

	if(BrancheSize >= uEnd-uBegin) // this is aleady a leef
		return CalculateLeaf(pFile, uBegin, uEnd, Hash);

	// this is a branche
	Depth++;
	SHashTreeNode* Left = CalculateRange(pFile, uBegin, uBegin + BrancheSize, eLeft, Depth, Hash);
	if(!Left)
		return NULL;
	SHashTreeNode* Right = CalculateRange(pFile, uBegin + BrancheSize, uEnd, eRight, Depth, Hash);
	if(!Right)
	{
		freeNode(Left);
		return NULL;
	}

	SHashTreeNode* Branche = allocNode(m_uSize, Left, Right);
	CalculateBranche(Branche, Hash);
	if(Depth >= m_DepthLimit) // if the depth limit is reached dont keep sub branches
	{
		freeNode(Branche->Left);
		Branche->Left = NULL;
		freeNode(Branche->Right);
		Branche->Right = NULL;
	}
	return Branche;
}

/** 
* CalculateLeaf: calculate a leaf hash
*
* @param: pFile:	Pointer to a random access IO device representing the file, It must be a exclusivly used instance.
* @param: uBegin:	Begin of the range
* @param: uEnd:		end of the range
* @return:			Tree nodefor the given range towh set up sub noned down to leafs
*/
SHashTreeNode* CFileHashTree::CalculateLeaf(QIODevice* pFile, uint64 uBegin, uint64 uEnd, CHashFunction& Hash)
{
	if((qint64)uBegin >= pFile->size())
	{
		SHashTreeNode* Branche = allocNode(m_uSize);
		memset(Branche->pHash, 0, m_uSize);
		return Branche; // file is not complete, or filling
	}
	if((qint64)uEnd > pFile->size())
		uEnd = (quint64)pFile->size();

	Hash.Reset();

	if(GetType() == HashTigerTree)
	{
		byte Mark[1];
		Mark[0] = 0x00; //leaf hash mark.
		Hash.Add(Mark,1);
	}

	if(pFile->pos() != uBegin)
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

	SHashTreeNode* Branche = allocNode(m_uSize);
	ASSERT(Hash.GetSize() == m_uSize);
	memcpy(Branche->pHash, Hash.GetKey(), m_uSize);
	return Branche;
}

/** 
* CalculateTree: calculates intermediate branche hashes
*
* @param: Branche:	BrantcheBranche/tree to calculate intermediate hashes and root hash for
* @return:			true if it was posible to calculate, meaning that all leaf hashes ware available, or false if failed
*/
bool CFileHashTree::CalculateTree(SHashTreeNode* Branche, CHashFunction& Hash)
{
	ASSERT(Branche);
	if(!Branche)
		return false;
	if(!Branche->IsLeaf())
	{
		if(!CalculateTree(Branche->Left, Hash))
			return false;
		if(!CalculateTree(Branche->Right, Hash))
			return false;

		CalculateBranche(Branche, Hash);
	}
	ASSERT(Branche->pHash != NULL);
	return true;
}

void CFileHashTree::CalculateBranche(SHashTreeNode* Branche, CHashFunction& Hash)
{
	Hash.Reset();

	if(GetType() == HashTigerTree)
	{
		byte Mark[1];
		Mark[0] = 0x01; //internal hash mark.
		Hash.Add(Mark,1);
	}

	Hash.Add(Branche->Left->pHash, m_uSize);
	Hash.Add(Branche->Right->pHash, m_uSize);
	Hash.Finish();

	ASSERT(Hash.GetSize() == m_uSize);
	memcpy(Branche->pHash, Hash.GetKey(), m_uSize);
}

/** 
* VerifyBranche: Verifyed all leafs in a given branche.
*
* @param: pFile:	Pointer to a random access IO device representing the file, It must be a exclusivly used instance.
* @param: pPartMap:	Part map that contains the state of all file segments
* @param: bAll:		Verification mode
* @param: Branche:	Branche for the range
* @param: uBegin:	Begin of the range
* @param: uEnd:		end of the range
* @Param: Balance:	specifyes how to balance the branche, it san be left or right depanding of if its the left or the right branche.
* @return:			true if it was posible to verify, or false if for some reason the oepration can nto be performed
*/
bool CFileHashTree::VerifyBranche(QIODevice* pFile, CPartMap* pPartMap, uint64 uFrom, uint64 uTo, EHashingMode Mode, SHashTreeNode* Branche, uint64 uBegin, uint64 uEnd, EBalance Balance, CHashFunction& Hash)
{
	if(uBegin > m_TotalSize) // HashTorrent
		return true;

	ASSERT(Branche);
	if(!Branche)
		return false;

	SHashTreeNode* TestBranche = NULL;
	uint64 BrancheSize = GetBrancheSize(uBegin, uEnd, Balance);
	ASSERT(BrancheSize);

	bool bLeaf;
	if((bLeaf = BrancheSize >= uEnd-uBegin) || Branche->IsLeaf()) 
	{
		if(!(uFrom < uEnd && uTo > uBegin))
			return true; // not in selected range

		if(Mode == eVerifyParts)
		{
			if(GetResult(uBegin, uEnd))
				return true; // already verified

			if((pPartMap->GetRange(uBegin, uEnd) & Part::Available) == 0)
				return true; // this part is not complete
		}

		if(bLeaf) // this is aleady a leef
			TestBranche = CalculateLeaf(pFile, uBegin, uEnd, Hash);
		else // this is a pseudo leef, we dont have deper data
			TestBranche = CalculateRange(pFile, uBegin, uEnd, Balance, 1, Hash);
	}
	else
	{
		if(Mode == eVerifyParts)
		{
			CPartMap::SIterator FileIter(uBegin, uEnd);
			bool bFound = false;
			while(pPartMap->IterateRanges(FileIter))
			{
				if(!(uFrom < FileIter.uEnd && uTo > FileIter.uBegin))
					continue; // not in selected range

				// Note: Corrupt means Available but failed hash test we hash it
				if((FileIter.uState & Part::Available) == 0)
					continue; // this range is not complete

				if(!GetResult(FileIter.uBegin, FileIter.uEnd)) // there is at least one bit not set in this range
				{
					bFound = true;
					break;
				}
			}
			if(!bFound)
				return true;
		}

		if(!VerifyBranche(pFile, pPartMap, uFrom, uTo, Mode, Branche->Left, uBegin, uBegin + BrancheSize, eLeft, Hash))
			return false;
		if(!VerifyBranche(pFile, pPartMap, uFrom, uTo, Mode, Branche->Right, uBegin + BrancheSize, uEnd, eRight, Hash))
			return false;
		return true;
	}

	bool bResult = TestBranche && memcmp(Branche->pHash, TestBranche->pHash, m_uSize) == 0;
	freeNode(TestBranche);

	if(uEnd > m_TotalSize) // HashTorrent
		uEnd = m_TotalSize;

	if(Mode != eRecoverParts || bResult) // dont set corruption for recovery runs, ok shoudl have already ben cleared anyways
		SetResult(bResult, uBegin, uEnd);
	return true;
}

/** 
* IsFullyResolved: check if on a givel size resolution the hashtree is compelte
*
* @param: uBegin:	Begin of the range
* @param: uEnd:		end of the range
* @param: uResolution:	desired reolution
*/
bool CFileHashTree::IsFullyResolved(uint64 uBegin, uint64 uEnd, uint64 uResolution)
{
	if(m_TreeRoot)
	{
		if(uResolution == -1)
			uResolution = m_BlockSize;

		uint64 TotalSize = GetTreeSize();
		if(uEnd == -1)
			uEnd = TotalSize;
		return IsFullyResolved(0, TotalSize, eLeft, m_TreeRoot, uBegin, uEnd, uResolution, 1);
	}
	return false;
}

bool CFileHashTree::IsFullyResolved(uint64 uBegin, uint64 uEnd, EBalance Balance, SHashTreeNode* Branche, uint64 uFrom, uint64 uTo, uint64 uResolution, int Depth)
{
	if(uBegin > m_TotalSize) // HashTorrent
		return true;

	uint64 BrancheSize = GetBrancheSize(uBegin, uEnd, Balance);
	ASSERT(BrancheSize);
	if(BrancheSize > uResolution && (uEnd > uFrom && uBegin < uTo)) // this is aleady a leef, or the target resolution has been reached
	{
		Depth++;
		if(Branche->IsLeaf())
			return Depth >= m_DepthLimit;

		if(!IsFullyResolved(uBegin, uBegin + BrancheSize, eLeft, Branche->Left, uFrom, uTo, uResolution, Depth))
			return false;
		if(!IsFullyResolved(uBegin + BrancheSize, uEnd, eRight, Branche->Right, uFrom, uTo, uResolution, Depth))
			return false;
		return true;
	}
	// Leaf or Branche we dont care we have a hash for this level
	return true;
}

bool CFileHashTree::SetHashSet(const QList<QByteArray>& HashSet)
{
	QReadLocker Locker(&m_TreeMutex);

	uint64 PartSize = m_PartSize;
	if(PartSize == -1)
		PartSize = m_BlockSize;
	ASSERT(!CanHashParts());

	if(HashSet.isEmpty() || HashSet.at(0).size() != m_uSize)
	{
		ASSERT(0);
		return false;
	}

	SHashTreeNode* TreeRoot = allocNode(m_uSize);

	uint64 TreeSize = GetTreeSize();

	int Index = 0;
	foreach(const QByteArray& Hash, HashSet)
	{
		uint64 uFrom = Index * PartSize;
		uint64 uTo = uFrom + PartSize;
		SHashTreeNode* Branche = Expose(0,TreeSize,eLeft,TreeRoot,uFrom,uTo);
		memcpy(Branche->pHash, Hash.data(), m_uSize);
		Index++;
	}
	/*if(GetType() == HashTorrent) // if its a torrent we have to add filling hashes
	{
		ASSERT((TreeSize%PartSize) == 0);
		int Count = TreeSize/PartSize;
		while(Index < Count)
		{
			uint64 uFrom = Index * PartSize;
			uint64 uTo = uFrom + PartSize;
			SHashTreeNode* Branche = Expose(0,TreeSize,eLeft,TreeRoot,uFrom,uTo);
			memset(Branche->pHash, 0, m_uSize);
			Index++;
		}
	}*/
	
	CHashFunction Hash(GetAlgorithm());
	ASSERT(Hash.IsValid());

	if(!CalculateTree(TreeRoot, Hash))
	{
		LogLine(LOG_ERROR, tr("Incomplete hash tree"));
		freeNode(TreeRoot);
		return false;
	}
	Locker.unlock();

	return SetTree(TreeRoot);
}

SHashTreeNode* CFileHashTree::Expose(uint64 uBegin, uint64 uEnd, EBalance Balance, SHashTreeNode* Branche, uint64 uFrom, uint64 uTo)
{
	if(uTo > uEnd)
		uTo = uEnd;

	if(uBegin == uFrom && uEnd == uTo)
		return Branche;
	
	uint64 BrancheSize = GetBrancheSize(uBegin, uEnd, Balance);
	ASSERT(BrancheSize);

	if(uTo <= uBegin + BrancheSize)
	{
		if(!Branche->Left)
			Branche->Left = allocNode(m_uSize);
		return Expose(uBegin, uBegin + BrancheSize, eLeft, Branche->Left, uFrom, uTo);
	}
	else
	{
		ASSERT(uFrom >= uBegin + BrancheSize);
		if(!Branche->Right)
			Branche->Right = allocNode(m_uSize);
		return Expose(uBegin + BrancheSize, uEnd, eRight, Branche->Right, uFrom, uTo);
	}
}

bool CFileHashTree::SetRootHash(const QByteArray& RootHash)
{
	QWriteLocker Locker(&m_TreeMutex);
	SHashTreeNode* TreeRoot = allocNode(m_uSize);
	memcpy(TreeRoot->pHash, RootHash.data(), m_uSize);
	if(!SetTree(TreeRoot))
	{
		LogLine(LOG_ERROR, tr("Invalid Hash Three for %1").arg(QString(ToString())));
		return false;
	}
	return true;
}

QByteArray CFileHashTree::GetRootHash() const
{
	QReadLocker Locker(&m_TreeMutex); 
	ASSERT(m_TreeRoot);
	if(!m_TreeRoot)
		return QByteArray();
	return QByteArray((char*)m_TreeRoot->pHash, (int)m_uSize);
}

QList<QByteArray> CFileHashTree::GetHashSet()
{
	QReadLocker Locker(&m_TreeMutex);

	uint64 PartSize = m_PartSize;
	if(PartSize == -1)
		PartSize = m_BlockSize;
	if(!CanHashParts())
	{
		ASSERT(0);
		return QList<QByteArray>();
	}

	QList<QByteArray> HashSet;

	uint64 uFrom = 0;
	uint64 uTo = 0;
	while(uFrom < m_TotalSize)
	{
		uTo += PartSize;
		HashSet.append(QByteArray((char*)Expose(0,m_TotalSize,eLeft,m_TreeRoot,uFrom,uTo)->pHash, (int)m_uSize));
		uFrom = uTo;
	}

	return HashSet;
}

bool CFileHashTree::AddLeafs(CFileHashTree* TreeRoot)
{
	if(TreeRoot->GetType() != GetType())
	{
		LogLine(LOG_ERROR, tr("Incompatible tree types"));
		ASSERT(0);
		return false;
	}

	ASSERT(TreeRoot->GetSize() == GetSize());

	QWriteLocker ForignLocker(&TreeRoot->m_TreeMutex);
	QWriteLocker Locker(&m_TreeMutex);
	if(!TreeRoot->m_TreeRoot)
	{
		ASSERT(0);
		return false;
	}

	if(!m_TreeRoot)
	{
		SHashTreeNode* NewTreeRoot = allocNode(m_uSize);
		memcpy(NewTreeRoot->pHash, TreeRoot->m_TreeRoot->pHash, m_uSize);
		if(!SetTree(NewTreeRoot))
		{
			LogLine(LOG_ERROR, tr("Invalid Hash Three for %1").arg(QString(ToString())));
			return false;
		}
	}
	else if(memcmp(TreeRoot->m_TreeRoot->pHash, m_TreeRoot->pHash, m_uSize) != 0) // Note: at this point TreeRoot->CalculateTree(...) must already have been called to verify the tree
	{
		LogLine(LOG_ERROR, tr("Source tree route hash does not match Target tree root hash"));
		return false;
	}

	// Note: we must own TreeRoot exclusivly so that we can do with it what we want without locking
	Merge(TreeRoot->m_TreeRoot, m_TreeRoot); 

	return true; // Merging must must nececerly succed as booth trees are already validated
}

void CFileHashTree::Merge(SHashTreeNode* SourceBranche, SHashTreeNode*& TargetBranche)
{
	if(!TargetBranche)
		TargetBranche = allocNode(m_uSize);

	if(TargetBranche->IsLeaf()) // if the target branche id a leaf we atempt to expand it
	{
		if(!SourceBranche->IsLeaf()) // if the source branche is not a leaf we can expand the target node
		{
			TargetBranche->Left = SourceBranche->Left;
			SourceBranche->Left = NULL;
			TargetBranche->Right = SourceBranche->Right;
			SourceBranche->Right = NULL;
		}
		// else // booth are leafs, nothing to do
	}
	else if(!SourceBranche->IsLeaf()) // if the source branche is not a leaf we can atempt to merge booth
	{
		Merge(SourceBranche->Left, TargetBranche->Left);
		Merge(SourceBranche->Right, TargetBranche->Right);
	}
}

CFileHashTree* CFileHashTree::GetLeafs(uint64 uFrom, uint64 uTo)
{
	QReadLocker Locker(&m_TreeMutex);
	ASSERT(m_TreeRoot);
	if(!m_TreeRoot)
		return NULL;

	CFileHashTree* pHash = new CFileHashTree(GetType(), GetTotalSize());
	pHash->m_TreeRoot = allocNode(m_uSize);

	ASSERT(m_BlockSize != -1);
	if(!Extract(m_TreeRoot, 0, GetTreeSize(), eLeft, pHash->m_TreeRoot, uFrom, uTo, m_BlockSize))
	{
		delete pHash;
		pHash = NULL;
	}
	return pHash;
}

bool CFileHashTree::Extract(SHashTreeNode* SourceBranche, uint64 uBegin, uint64 uEnd, EBalance Balance, SHashTreeNode* TargetBranche, uint64 uFrom, uint64 uTo, uint64 uResolution)
{
	if(uEnd > uFrom && uBegin < uTo)
	{
		ASSERT(TargetBranche->IsLeaf());

		uint64 BrancheSize = GetBrancheSize(uBegin, uEnd, Balance);
		ASSERT(BrancheSize);
		if(BrancheSize < uEnd-uBegin && BrancheSize >= uResolution && !SourceBranche->IsLeaf()) // this is aleady a leef, or the target resolution has been reached
		{
			TargetBranche->Left = allocNode(m_uSize);
			TargetBranche->Right = allocNode(m_uSize);
			if(!Extract(SourceBranche->Left, uBegin, uBegin + BrancheSize, eLeft, TargetBranche->Left, uFrom, uTo, uResolution))
				return false;
			if(!Extract(SourceBranche->Right, uBegin + BrancheSize, uEnd, eRight, TargetBranche->Right, uFrom, uTo, uResolution))
				return false;
			return true;
		}
	}

	ASSERT(SourceBranche->pHash);
	if(!SourceBranche->pHash)
		return false;

	memcpy(TargetBranche->pHash, SourceBranche->pHash, m_uSize);
	return true;
}

/*QList<CFileHashTree::SPart> CFileHashTree::GetRanges(uint64 uFrom, uint64 uTo, bool bEnvelope)
{
	ASSERT(uFrom <= uTo);
	if(!m_TreeRoot)
		return QList<SPart> ();
	if(uTo == -1 || uTo > m_TotalSize)
		uTo = m_TotalSize;
	return GetRanges(0,m_TotalSize,eLeft,m_TreeRoot,uFrom,uTo, bEnvelope);
}

QList<CFileHashTree::SPart> CFileHashTree::GetRanges(uint64 uBegin, uint64 uEnd, EBalance Balance, SHashTreeNode* Branche, uint64 uFrom, uint64 uTo, bool bEnvelope)
{
	QList<SPart> Ranges;
	if(uEnd > uFrom && uBegin < uTo)
	{
		uint64 BrancheSize = GetBrancheSize(uBegin, uEnd, Balance);
		ASSERT(BrancheSize);
		if(BrancheSize < uEnd-uBegin && !Branche->IsLeaf()) // this is aleady a leef, or have we reached full resolution
		{
			Ranges.append(GetRanges(uBegin, uBegin + BrancheSize, eLeft, Branche->Left, uFrom, uTo, bEnvelope));
			Ranges.append(GetRanges(uBegin + BrancheSize, uEnd, eRight, Branche->Right, uFrom, uTo, bEnvelope));
		}
		else if(bEnvelope || (uBegin >= uFrom && uEnd <= uTo))
		{
			Ranges.append(SPart(uBegin, uEnd));
		}
	}
	return Ranges;
}*/

/* Comment copyed form eMule (Fair Use):
The identifier basically describes the way from the top of the tree to the hash. a set bit (1)
means follow the left branch, a 0 means follow the right. The highest bit which is set is seen as the start-
postion (since the first node is always seend as left).

Example

								x                   0000000000000001
							 /     \		
						 x		    \				0000000000000011
					  /		\	       \
                    x       _X_          x 	        0000000000000110

*/

QByteArray CFileHashTree::SaveBin(int DepthLimit)
{
	QReadLocker Locker(&m_TreeMutex);

	SHashTreeDump Leafs(m_uSize);
	Save(Leafs, DepthLimit);

	CBuffer Buffer;
	
	uint32 LeafCount = Leafs.Count();
	Buffer.WriteValue<uint32>(LeafCount);

	for(int Index = 0; Index < LeafCount; Index++)
	{
		// Note: 32 bit identifyers allow for 0x7FFFFFFF entrys at the lowest level, thats haver going to be exhausted
		//Buffer.WriteValue<uint32>(TreePath);
		Buffer.WriteValue<uint64>(Leafs.ID(Index));
		Buffer.WriteData(Leafs.Hash(Index), m_uSize);
	}
	return Buffer.ToByteArray();
}

bool CFileHashTree::Save(SHashTreeDump& Leafs, int DepthLimit)
{
	if(m_TreeRoot)
		return Save(Leafs, m_TreeRoot, 1, DepthLimit);
	return false;
}

bool CFileHashTree::Save(SHashTreeDump &Leafs, SHashTreeNode* Branche, uint64 TreePath, int DepthLimit)
{
	if(DepthLimit == -1)
		DepthLimit = m_DepthLimit;

	ASSERT(Branche);
	if(!Branche)
		return false;

	if(!Branche->IsLeaf() && DepthLimit > 0)
	{
		uint64 RightPath = TreePath << 1;
		uint64 LeftPath = RightPath | 1;

		if(!Save(Leafs, Branche->Left, LeftPath, DepthLimit-1))
			return false;
		if(!Save(Leafs, Branche->Right, RightPath, DepthLimit-1))
			return false;
		return true;
	}

	ASSERT(Branche->pHash);
	Leafs.Add(TreePath, Branche->pHash);
	return true;
}

bool CFileHashTree::LoadBin(const QByteArray& Array)
{
	ASSERT(m_TreeRoot == NULL);

	CBuffer Buffer(Array, true);

	uint32 LeafCount = Buffer.ReadValue<uint32>();
	SHashTreeDump Leafs(m_uSize, LeafCount);
	for(uint32 i = 0; i < LeafCount; i++)
	{
		//uint64 TreePath = bOld ? Buffer.ReadValue<uint64>() : Buffer.ReadValue<uint32>();
		uint64 TreePath = Buffer.ReadValue<uint64>();
		byte* Hash = Buffer.ReadData(m_uSize);
		Leafs.Add(TreePath, Hash);
	}
	return Load(Leafs);
}

bool CFileHashTree::Load(SHashTreeDump& Leafs)
{
	SHashTreeNode* TreeRoot = allocNode(m_uSize);

	for(int Index = 0; Index < Leafs.Count(); Index++)
	{
		uint64 TreePath = Leafs.ID(Index);

		int i = 0;
		for (; i < 64 && (TreePath & 0x8000000000000000) == 0; i++) // shift the identso that 1 stands first
			TreePath <<= 1;

		Load(Leafs.Hash(Index), (TreePath << 1), ((64-i) - 1), TreeRoot); // Note: level 0 is prealocated automaticaly
	}

	CHashFunction Hash(GetAlgorithm());
	ASSERT(Hash.IsValid());

	if(!CalculateTree(TreeRoot, Hash))
	{
		LogLine(LOG_ERROR, tr("Incomplete hash tree"));
		freeNode(TreeRoot);
		return false;
	}
	if(!SetTree(TreeRoot))
	{
		LogLine(LOG_ERROR, tr("Invalid Hash Three for %1").arg(QString(ToString())));
		return false;
	}
	return true;
}

void CFileHashTree::Load(byte* Leaf, uint64 TreePath, int ToGo, SHashTreeNode* Branche)
{
	if(!ToGo)
		memcpy(Branche->pHash, Leaf, m_uSize);
	else
	{
		if(Branche->IsLeaf())
		{
			Branche->Left = allocNode(m_uSize);
			Branche->Right = allocNode(m_uSize);
		}

		if(TreePath & 0x8000000000000000)
			Load(Leaf, (TreePath << 1), (ToGo - 1), Branche->Left);
		else
			Load(Leaf, (TreePath << 1), (ToGo - 1), Branche->Right);
	}
}

CFileHash* CFileHashTree::Clone(bool bFull)
{
	CFileHashTree* pHashTree = new CFileHashTree(m_eType, m_TotalSize, m_BlockSize, m_PartSize, m_DepthLimit);
	pHashTree->SetHash(GetHash());
	if(bFull)
		pHashTree->SetHashSet(GetHashSet());
	return pHashTree;
}

/* Merkle Torrent notation
#                                      1 (1)
                                       0* = root hash
                                    /     \
                                /            \
                            /                   \
                        /                          \
                    /                                 \
#                 11 (3)							 10 (2)
                  1*                                     2
                 / \                                    / \
               /     \                                /     \
             /         \                            /         \
           /             \                        /             \
         /                 \                    /                 \
#       111 (7)           110 (6)            101 (5)             100 (4)
        3                   4                  5                   6* = uncle
       / \                 / \                / \                 / \
      /   \               /   \              /   \               /   \
     /     \             /     \            /     \             /     \
# 1111(15) 1110 (14) 1101 (13) 1100 (12) 1011 (11) 1010 (10) 1001 (9)  0100 (8)
   7         8         9        10        11        12*       13        14
  / \       / \       / \       / \       / \       / \       / \       / \
15   16   17   18   19   20   21   22   23   24   25   26   27   28   29   30

P0   P1   P2   P3   P4   P5   P6   P7   P8*  P9*  P10  P11  P12   X    X    X
= piece index                            =    =                   = filler hash
                                         p    s
                                         i    i
                                         e    b
                                         c    l
                                         e    i
                                              n
                                              g

# Neo Notation
*/

UINT cbits(UINT val)
{
	ASSERT(val != 0); // ASSERT(val == cbits(cbits(val)));
	const UINT len = sizeof(val) << 3;
	UINT pos = 0;
	while(val >> (pos+1) && pos < len) // find highest order bit
		pos ++;
	UINT hob = (1 << pos);
	UINT tmp = (hob << 1) - val;
	return hob + tmp - 1;
}

UINT Neo2Merkle(UINT N)
{
	return cbits(N) - 1;
}

UINT Merkle2Neo(UINT M)
{
	return cbits(M + 1);
}