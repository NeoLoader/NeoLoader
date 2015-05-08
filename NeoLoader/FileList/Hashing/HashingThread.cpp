#include "GlobalHeader.h"
#include "HashingThread.h"
#include "HashingJobs.h"
#include "../File.h"
#include "../FileList.h"
#include "../IOManager.h"
#include "../../NeoCore.h"
#include "FileHash.h"
#include "FileHashSet.h"
#include "FileHashTree.h"
#include "FileHashTreeEx.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

CHashingThread::CHashingThread(QObject* qObject)
: QThreadEx(qObject)
{
	bool bCreate = !QFile::exists(theCore->Cfg()->GetSettingsDir() + "/HashData.sq3");

	m_DataBase = new QSqlDatabase();
	*m_DataBase = QSqlDatabase::addDatabase("QSQLITE", "HashData");
	m_DataBase->setDatabaseName(theCore->Cfg()->GetSettingsDir() + "/HashData.sq3");
	m_DataBase->open();

	m_DataBase->exec("pragma synchronous = off");
	m_DataBase->exec("pragma journal_mode = off");
	m_DataBase->exec("pragma locking_mode = exclusive");
	m_DataBase->exec("pragma cache_size = 100000");

	if(bCreate)
	{
        QSqlQuery Query1(*m_DataBase);
		Query1.exec("CREATE TABLE hashsets (hash VARCHAR(255) PRIMARY KEY, hashset BLOB)");
		//QString e1 = Query1.lastError().text();

		QSqlQuery Query2(*m_DataBase);
		Query2.exec("CREATE TABLE hashtrees (hash VARCHAR(255) PRIMARY KEY, hashtree BLOB)");
		//QString e2 = Query2.lastError().text();

		QSqlQuery Query3(*m_DataBase);
		Query3.exec("CREATE TABLE metahashes (hash VARCHAR(255) PRIMARY KEY, roothash VARCHAR(255), metahash VARCHAR(255))");
		//QString e3 = Query2.lastError().text();
	}

	m_HashingID = 0;
	m_HashingOffset = 0;
	m_HashingCount = 0;
	m_LongJob = false;

	m_Stop = false;
	//start();
}

CHashingThread::~CHashingThread()
{
	Stop();

	m_DataBase->close();
	delete m_DataBase;
}

void CHashingThread::Stop()
{
	m_Stop = true;
	wait();
}

void CHashingThread::run()
{
	while(!m_Stop)
	{
		// if the write buffer is getting full, take a longer break and suspend hashing
		if(theCore->m_IOManager->IsWriteBufferFull(true))
		{
			sleep(5);
			continue;
		}

		m_Mutex.lock();
		if(m_HashingQueue.isEmpty())
		{
			m_Mutex.unlock();
			msleep(256);
			continue;
		}

		CHashingJobPtr pHashingJob = m_HashingQueue.takeFirst();

		m_HashingOffset = 0;
		m_LongJob = pHashingJob->IsLongJob();
		m_HashingID = pHashingJob->GetFileID();

		m_Mutex.unlock();
		//if(!pHashingJob)
		//	continue; // this one is gone

		if(CManagedIO* pDevice = theCore->m_IOManager->GetDevice(pHashingJob->GetFileID()))
		{
			pDevice->tell(&m_HashingOffset); // set the position preview pointer
			
			pHashingJob->Execute(pDevice);

			delete pDevice;
		}
		// else // file have been removed

		m_HashingID = 0;
		if(m_LongJob)
			m_HashingCount--;
	}
}

bool CHashingThread::AddHashingJob(const CHashingJobPtr& pHashingJob)
{
	QMutexLocker Locker(&m_Mutex);
	pHashingJob->SetThread(this);
	if(pHashingJob->IsLongJob())
		m_HashingCount++;

	int i=0;
	for(; i < m_HashingQueue.size(); i++)
	{
		if(m_HashingQueue[i]->GetPriority() > pHashingJob->GetPriority()) // smaler number means higher priority
			break;
	}
	m_HashingQueue.insert(i, pHashingJob);
	return true;
}

uint64 CHashingThread::GetProgress(CFile* pFile)
{
	if(m_LongJob && m_HashingID == pFile->GetFileID())
		return m_HashingOffset;
	return -1;
}

bool CHashingThread::IsHashing(uint64 FileID)
{
	if(FileID == m_HashingID && m_LongJob)
		return true;
	QMutexLocker Locker(&m_Mutex);
	foreach(const CHashingJobPtr& pHashingJob, m_HashingQueue)
	{
		if(pHashingJob->GetFileID() == FileID && pHashingJob->IsLongJob())
			return true;
	}
	return false;
}

bool CHashingThread::LoadHash(CFileHash* pHash)
{
	QMutexLocker Locker(&m_Mutex);

	if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pHash))
	{
		QSqlQuery Query1(*m_DataBase);
		Query1.prepare("SELECT hashset FROM hashsets WHERE hash = :hash");
		Query1.bindValue(":hash", pHashSet->GetHash());
		Query1.exec();
#ifdef _DEBUG
		QString e1 = Query1.lastError().text();
#endif

		if(!Query1.next())
			return false; // not found in DB

		return pHashSet->LoadBin(Query1.value(0).toByteArray());
	}
	else if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pHash))
	{
		CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(pHash);
		QByteArray RootHash;
		if(pHashTreeEx)
		{
			QSqlQuery Query1(*m_DataBase);
			Query1.prepare("SELECT roothash, metahash FROM metahashes WHERE hash = :hash");
			Query1.bindValue(":hash", pHashTreeEx->GetHash());
			Query1.exec();
#ifdef _DEBUG
			QString e1 = Query1.lastError().text();
#endif
			if(!Query1.next())
				return false; // not found in DB

			RootHash = Query1.value(0).toByteArray();
			pHashTreeEx->SetMetaHash(Query1.value(1).toByteArray());
		}

		QSqlQuery Query1(*m_DataBase);
		Query1.prepare("SELECT hashtree FROM hashtrees WHERE hash = :hash");
		Query1.bindValue(":hash", pHashTreeEx ? RootHash : pHashTree->GetHash());
		Query1.exec();
#ifdef _DEBUG
		QString e1 = Query1.lastError().text();
#endif

		if(!Query1.next())
			return false; // not found in DB

		return pHashTree->LoadBin(Query1.value(0).toByteArray());
	}
	return true; // nothing to be loaded
}

void CHashingThread::SaveHash(CFileHash* pHash)
{
	QMutexLocker Locker(&m_Mutex);

	if(CFileHashSet* pHashSet = qobject_cast<CFileHashSet*>(pHash))
	{
		QSqlQuery Query1(*m_DataBase);
		Query1.prepare("SELECT hashset FROM hashsets WHERE hash = :hash");
		Query1.bindValue(":hash", pHashSet->GetHash());
		Query1.exec();
#ifdef _DEBUG
		QString e1 = Query1.lastError().text();
#endif

		bool Found = Query1.next();

		QSqlQuery Query2(*m_DataBase);
		if(Found)
			Query2.prepare("UPDATE hashsets SET hashset = :hashset WHERE hash = :hash");
		else
			Query2.prepare("INSERT INTO hashsets (hash, hashset) VALUES (:hash, :hashset)");
		Query2.bindValue(":hash", pHashSet->GetHash());
		Query2.bindValue(":hashset", pHashSet->SaveBin());
		Query2.exec();
#ifdef _DEBUG
		QString e2 = Query2.lastError().text();
#endif
	}
	else if(CFileHashTree* pHashTree = qobject_cast<CFileHashTree*>(pHash))
	{
		CFileHashTreeEx* pHashTreeEx = qobject_cast<CFileHashTreeEx*>(pHash);
		if(pHashTreeEx)
		{
			QSqlQuery Query1(*m_DataBase);
			Query1.prepare("SELECT roothash, metahash FROM metahashes WHERE hash = :hash");
			Query1.bindValue(":hash", pHashTreeEx->GetHash());
			Query1.exec();
#ifdef _DEBUG
			QString e1 = Query1.lastError().text();
#endif

			bool Found = Query1.next();

			QSqlQuery Query2(*m_DataBase);
			if(Found)
				Query2.prepare("UPDATE metahashes SET roothash = :roothash, metahashe = :metahashes WHERE hash = :hash");
			else
				Query2.prepare("INSERT INTO metahashes (hash, roothash, metahash) VALUES (:hash, :roothash, :metahash)");
			Query2.bindValue(":hash", pHashTreeEx->GetHash());
			Query2.bindValue(":roothash", pHashTreeEx->GetRootHash());
			Query2.bindValue(":metahash", pHashTreeEx->GetMetaHash());
			Query2.exec();
#ifdef _DEBUG
			QString e2 = Query2.lastError().text();
#endif
		}

		QSqlQuery Query1(*m_DataBase);
		Query1.prepare("SELECT hashtree FROM hashtrees WHERE hash = :hash");
		Query1.bindValue(":hash", pHashTreeEx ? pHashTreeEx->GetRootHash() : pHashTree->GetHash());
		Query1.exec();
#ifdef _DEBUG
		QString e1 = Query1.lastError().text();
#endif

		bool Found = Query1.next();

		QSqlQuery Query2(*m_DataBase);
		if(Found)
			Query2.prepare("UPDATE hashtrees SET hashtree = :hashtree WHERE hash = :hash");
		else
			Query2.prepare("INSERT INTO hashtrees (hash, hashtree) VALUES (:hash, :hashtree)");
		Query2.bindValue(":hash", pHashTreeEx ? pHashTreeEx->GetRootHash() : pHashTree->GetHash());
		Query2.bindValue(":hashtree", pHashTree->SaveBin());
		Query2.exec();
#ifdef _DEBUG
		QString e2 = Query2.lastError().text();
#endif
	}
}