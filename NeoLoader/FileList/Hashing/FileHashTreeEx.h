#pragma once

#include "FileHashTree.h"

class CFileHashTreeEx: public CFileHashTree
{
	Q_OBJECT

public:
	CFileHashTreeEx(EFileHashType eType, uint64 TotalSize, uint64 BlockSize = -1, uint64 PartSize = -1, int DepthLimit = -1);

	virtual bool				IsComplete()	{return CFileHashTree::IsComplete() && HasMetaHash();}
	virtual bool				HasMetaHash()	{QReadLocker Locker(&m_TreeMutex); return !m_MetaHash.isEmpty();}

	virtual QByteArray			HashMetaData(const QByteArray& Data);
	virtual bool				SetMetaHash(const QByteArray& MetaHash);
	virtual QByteArray			GetMetaHash() const			{QReadLocker Locker(&m_TreeMutex); return m_MetaHash;}

	virtual bool 				Calculate(QIODevice* pFile);

	virtual bool				Validate(const QByteArray& MetaHash, const QByteArray& RootHash);

	virtual CFileHash*			Clone(bool bFull);

protected:
	virtual bool				Validate();
	virtual bool				SetTree(SHashTreeNode* TreeRoot);

	QByteArray					m_MetaHash;
};