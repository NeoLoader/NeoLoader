#pragma once

#include "../../../Framework/MT/ThreadEx.h"
#include "../PartMap.h"
#include "FileHash.h"
class QSqlDatabase;
class CFile;
class CHashingJob;

typedef QSharedPointer<CHashingJob> CHashingJobPtr;
//typedef QWeakPointer<CHashingJob> CHashingJobRef;

class CHashingThread: public QThreadEx
{
	Q_OBJECT

public:
	CHashingThread(QObject* qObject = NULL);
	~CHashingThread();

	void						run();
	void						Stop();

	bool						AddHashingJob(const CHashingJobPtr& pHashingJob);
	uint64						GetProgress(CFile* pFile);

	bool						IsHashing(uint64 FileID);

	int							GetCount()		{return m_HashingCount;}

	bool						LoadHash(CFileHash* pHash);
	void						SaveHash(CFileHash* pHash);

protected:
	QMutex						m_Mutex;
	QList<CHashingJobPtr>		m_HashingQueue;
	QSqlDatabase*				m_DataBase;

	volatile int				m_HashingCount;
	volatile uint64				m_HashingID;
	volatile uint64				m_HashingOffset;
	volatile bool				m_LongJob;

	volatile bool				m_Stop;
};