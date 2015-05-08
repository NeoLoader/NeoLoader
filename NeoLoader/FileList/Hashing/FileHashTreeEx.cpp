#include "GlobalHeader.h"
#include "FileHashTreeEx.h"
#include "../../../Framework/Cryptography/HashFunction.h"

void freeNode(SHashTreeNode* ptr);

CFileHashTreeEx::CFileHashTreeEx(EFileHashType eType, uint64 TotalSize, uint64 BlockSize, uint64 PartSize, int DepthLimit)
: CFileHashTree(eType, TotalSize, BlockSize, PartSize, DepthLimit)
{
}

bool CFileHashTreeEx::SetTree(SHashTreeNode* TreeRoot)
{
	QWriteLocker Locker(&m_TreeMutex);
	freeNode(m_TreeRoot);
	m_TreeRoot = TreeRoot;
	if(m_MetaHash.isEmpty())
		return true;

	return Validate();
}

QByteArray CFileHashTreeEx::HashMetaData(const QByteArray& Data)
{
	CHashFunction Hash(GetAlgorithm());
	ASSERT(Hash.IsValid());

	Hash.Add(Data);
	Hash.Finish();

	return Hash.ToByteArray();
}

bool CFileHashTreeEx::SetMetaHash(const QByteArray& MetaHash)
{
	QWriteLocker Locker(&m_TreeMutex);
	m_MetaHash = MetaHash;
	if(!m_TreeRoot)
		return true;

	if(!Validate())
		return false;
	Locker.unlock();
	return true;
}

bool CFileHashTreeEx::Calculate(QIODevice* pFile)
{
	if(!CFileHashTree::Calculate(pFile))
		return false;
	SetHash(NULL); // clear hash let it be set when metadata are set
	return true;
}

bool CFileHashTreeEx::Validate(const QByteArray& MetaHash, const QByteArray& RootHash)
{
	if(!IsValid())
		return false;

	QReadLocker Locker(&m_TreeMutex);
	CHashFunction Hash(GetAlgorithm());
	ASSERT(Hash.IsValid());

	Hash.Add(MetaHash);
	Hash.Add(RootHash);
	Hash.Finish();

	return Compare(Hash.GetKey());
}

bool CFileHashTreeEx::Validate()
{
	CHashFunction Hash(GetAlgorithm());
	ASSERT(Hash.IsValid());

	Hash.Add(m_MetaHash);
	Hash.Add(m_TreeRoot->pHash, m_uSize);
	Hash.Finish();

	if(!IsValid())
		SetHash(Hash.GetKey());
	else if(!Compare(Hash.GetKey()))
	{
		m_MetaHash.clear();
		freeNode(m_TreeRoot);
		m_TreeRoot = NULL;
		return false;
	}
	return true;
}

CFileHash* CFileHashTreeEx::Clone(bool bFull)			
{
	CFileHashTreeEx* pHashTree = new CFileHashTreeEx(m_eType, m_TotalSize, m_BlockSize, m_PartSize, m_DepthLimit);
	pHashTree->SetHash(GetHash());
	if(bFull)
		pHashTree->SetHashSet(GetHashSet());
	QReadLocker Locker(&m_TreeMutex); 
	if(!m_MetaHash.isEmpty())
		pHashTree->SetMetaHash(m_MetaHash);
	return pHashTree;
}