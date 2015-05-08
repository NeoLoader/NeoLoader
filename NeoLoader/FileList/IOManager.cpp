#include "GlobalHeader.h"
#include "IOManager.h"
#include "../../Framework/OtherFunctions.h"
#include "../NeoCore.h"

int _uint64_type = qRegisterMetaType<uint64>("uint64");

///////////////////////////////////////////////////////////////////////////////////////////////
//

CAbstractIO::CAbstractIO(const QString& Name)
{
	m_FileName = Name;
	m_FileSize = 0;

	m_LastUse = 0;
}

CAbstractIO::~CAbstractIO() 
{
	foreach(CIOOperation* pOperation, m_Operations)
		delete pOperation;
}

void CAbstractIO::QueueOp(CIOOperation* pOperation)
{
	QMutexLocker Locker(&m_QueueMutex);
	m_Operations.insert(pOperation->GetOffset(), pOperation);
}

void CAbstractIO::Execute()
{
	QMutexLocker Locker(&m_QueueMutex);
	while(!m_Operations.isEmpty())
	{
		QMultiMap<uint64, CIOOperation*>::iterator I = m_Operations.begin();
		CIOOperation* pOperation = I.value();
		m_Operations.erase(I);

		Locker.unlock();

		pOperation->Execute(this);
		pOperation->deleteLater();

		Locker.relock();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
//

CIOManager::CIOManager(QObject* qObject)
 : QThreadEx(qObject)
{
	m_Stop = false;

	m_PendingReadSize = 0;
	m_PendingWriteSize = 0;

	m_PendingAllocation = 0;

	start();
}

CIOManager::~CIOManager()
{
	Stop();

	foreach(CFileAllocator* pAllocator, m_PendingAllocations)
		delete pAllocator;
	m_PendingAllocations.clear();
}

void CIOManager::Stop()
{
	m_Stop = true;
	wait();
}

void CIOManager::run()
{
	int Counter = 0;

	while(!m_Stop)
	{
		// allocation BEIGN
		m_AllocationsMutex.lock();
		if(!m_PendingAllocations.isEmpty())
		{
			QMap<uint64, CFileAllocator*>::iterator I = m_PendingAllocations.find(m_PendingAllocation);
			if(I == m_PendingAllocations.end()) // Note: we ensure we stick to the choice we made
			{
				I = m_PendingAllocations.begin();
				m_PendingAllocation = I.key();
			}

			CFileAllocator* pAllocator = I.value();

			bool bDone = true;
			if(CIOPtr File = m_Files.value(I.key()))
				bDone = pAllocator->Execute(File.data());
			if(bDone)
			{
				delete pAllocator;
				m_PendingAllocations.erase(I);
			}
		}
		m_AllocationsMutex.unlock();
		// allocation END

		if((Counter % 50) == 0) // twice a second
		{
			QMutexLocker Locker(&m_FilesMutex);

			QMutexLocker ActiveLocker(&m_ActiveMutex);

			uint64 uNow = GetCurTick();
			for(QMap<uint64, CIOPtr>::iterator I = m_Files.begin(); I != m_Files.end(); I++)
			{
				CIOPtr& pFile = *I;
				pFile->Process();

				bool bActive = m_ActiveFiles.contains(pFile);
				if(pFile->GetQueueSize() > 0)
				{
					if(!bActive)
						m_ActiveFiles.insert(pFile);
				}
				else if(bActive)
				{
					if(uNow - pFile->GetLastUse() > SEC2MS(5))
						m_ActiveFiles.remove(pFile);
				}
			}
		}
		

		m_ActiveMutex.lock();
		for(QSet<CIOPtr>::iterator I = m_ActiveFiles.begin(); I != m_ActiveFiles.end(); I++)
			(*I)->Execute();
		m_ActiveMutex.unlock();

		if(++Counter >= 100) // every second
			Counter = 0;

		msleep(10);
	}
}

CIOPtr CIOManager::GetFile(uint64 FileID) const
{
	QMutexLocker Locker(&m_FilesMutex);
	return m_Files.value(FileID);
}

bool CIOManager::QueueOp(uint64 FileID, CIOOperation* pOperation)
{
	if(CIOPtr File = GetFile(FileID))
	{
		File->QueueOp(pOperation);
		return true;
	}
	return false;
}

void CIOManager::ReadData(QObject* Reciver, uint64 FileID, uint64 Offset, uint64 Length, void* Aux)
{
	CIOOperation* pOperation = new CReadOperation(this, Offset, Length, Aux);
	connect(pOperation, SIGNAL(DataRead(uint64, uint64, const QByteArray&, bool, void*)), Reciver, SLOT(OnDataRead(uint64, uint64, const QByteArray&, bool, void*)));
	QueueOp(FileID, pOperation);
}

void CIOManager::WriteData(QObject* Reciver, uint64 FileID, uint64 Offset, const QByteArray& Data, void* Aux)
{
	CIOOperation* pOperation = new CWriteOperation(this, Offset, Data, Aux);
	connect(pOperation, SIGNAL(DataWriten(uint64, uint64, bool, void*)), Reciver, SLOT(OnDataWriten(uint64, uint64, bool, void*)));
	QueueOp(FileID, pOperation);
}

void CIOManager::InstallIO(uint64 FileID, const QString& FilePath, const QList<SMultiIO>* pSubFiles)
{
	int Pos = FilePath.lastIndexOf("/");
	ASSERT(Pos != -1);
	CreateDir(FilePath.left(Pos+1));

	QMutexLocker Locker(&m_FilesMutex);
	ASSERT(!m_Files.contains(FileID));
	CIOPtr File = CIOPtr(pSubFiles ? (CAbstractIO*)new CMultiFileIO(FilePath, pSubFiles, this) : (CAbstractIO*)new CFileIO(FilePath));
	m_Files.insert(FileID, File);
}

void CIOManager::ProtectIO(uint64 FileID, bool bSetReadOnly)
{
	if(CIOPtr File = GetFile(FileID))
		return File->SetReadOnly(bSetReadOnly);
}

bool CIOManager::MoveIO(uint64 FileID, const QString& FilePath)
{
	int Pos = FilePath.lastIndexOf("/");
	ASSERT(Pos != -1);
	CreateDir(FilePath.left(Pos+1));

	CIOPtr File = GetFile(FileID);
	return File->Rename(FilePath);
}

CIOPtr CIOManager::TakeFile(uint64 FileID)
{
	m_FilesMutex.lock();
	ASSERT(m_Files.contains(FileID));
	m_AllocationsMutex.lock(); // we dont want allocator to be left firh a file pointer here
	CIOPtr File = m_Files.take(FileID);
	m_AllocationsMutex.unlock();
	m_FilesMutex.unlock();

	m_ActiveMutex.lock();
	m_ActiveFiles.remove(File);
	m_ActiveMutex.unlock();

	return File;
}

void CIOManager::UninstallIO(uint64 FileID)
{
	if(CIOPtr File = TakeFile(FileID)) 
		File->Execute(); // purge all tasks
}

//bool CIOManager::DeleteIO(uint64 FileID)
//{
//	if(CIOPtr File = TakeFile(FileID)) 
//		return File->Remove();
//	return false;
//}

//void CIOManager::CloseIO(uint64 FileID)
//{
//	CIOOperation* pOperation = new CCloseOperation(this);
//	QueueOp(FileID, pOperation);
//}

uint64 CIOManager::GetSize(uint64 FileID)
{
	if(CIOPtr File = GetFile(FileID))
		return File->GetSize();
	return 0;
}

void CIOManager::Allocate(uint64 FileID, uint64 FileSize, QObject* Reciver)
{
	QMutexLocker Locker(&m_AllocationsMutex);
	if(m_PendingAllocations.contains(FileID))
		return;
	CFileAllocator* pAllocator = new CFileAllocator(this, FileSize);
	if(Reciver)
		connect(pAllocator, SIGNAL(Allocation(uint64, bool)), Reciver, SLOT(OnAllocation(uint64, bool)));
	m_PendingAllocations.insert(FileID, pAllocator);
}

bool CIOManager::IsAllocating(uint64 FileID)
{
	QMutexLocker Locker(&m_AllocationsMutex);
	return m_PendingAllocations.contains(FileID);
}

CManagedIO* CIOManager::GetDevice(uint64 FileID)
{
	if(CIOPtr File = GetFile(FileID))
		return new CManagedIO(this, FileID);
	return NULL;
}

bool CIOManager::IsWriteBufferFull(bool bHalf)
{
	return GetPendingWriteSize() >= (theCore->Cfg()->GetInt("Content/CacheLimit") / (bHalf ? 2 : 1));
}

/////////////////////////////////////////////
// Interface

QString CIOManager::fileName(uint64 FileID) const
{
	if(CIOPtr File = GetFile(FileID))
		return File->GetFileName();
	return "";
}

qint64 CIOManager::size(uint64 FileID) const
{
	if(CIOPtr File = GetFile(FileID))
		return File->GetSize();
	return 0;
}

qint64 CIOManager::readData(uint64 FileID, uint64 Offset, char *data, qint64 maxlen)
{
	if(CIOPtr File = GetFile(FileID))
		return File->Read(Offset, data, maxlen);
	return -1;
}

qint64 CIOManager::writeData(uint64 FileID, uint64 Offset, const char *data, qint64 len)
{
	if(CIOPtr File = GetFile(FileID))
		return File->Write(Offset, data, len);
	return -1;
}

void CIOManager::close(uint64 FileID)
{
	if(CIOPtr File = GetFile(FileID))
		File->Close();
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Multi File

QMutex CFileIO::m_GlobalMutex;
QList<CFileIO*> CFileIO::m_GlobalList;

CFileIO::CFileIO(const QString& Name)
 : CAbstractIO(Name)
{
	m_pFile = NULL;
	if(!QFile::exists(m_FileName))
	{
		QMutexLocker Locker(&m_FileMutex);
		Open(QFile::ReadWrite | QFile::Unbuffered);
	}

	m_FileSize = QFileInfo(Name).size();
	m_ReadOnly = false;
}

CFileIO::~CFileIO()
{
	Close();
}

void CFileIO::Process()
{
	if(m_pFile && GetCurTick() - m_LastUse > SEC2MS(5)) // C-ToDo: customize
		Close();
}

bool CFileIO::Rename(const QString & Name)
{
	Close();

	QMutexLocker Locker2(&m_Mutex); 
	if(!QFile::rename(m_FileName, Name))
		return false;
	m_FileName = Name;
	return true;
}

//bool CFileIO::Remove() 
//{
//	Close();
//
//	QMutexLocker Locker2(&m_Mutex); 
//	return QFile::remove(m_FileName);
//}

qint64 CFileIO::Read(qint64 offset, char* data, qint64 maxSize) 
{
	m_LastUse = GetCurTick();
	QMutexLocker Locker(&m_FileMutex);
	if(!m_pFile || !m_pFile->isOpen())
	{
		if(!Open(QFile::ReadOnly | QFile::Unbuffered))
			return -1;
	}
	if(m_pFile->pos() != offset && !m_pFile->seek(offset))
		return -1;
	return m_pFile->read(data, maxSize);
}

qint64 CFileIO::Write(qint64 offset, const char* data, qint64 maxSize) 
{
	if(m_ReadOnly)
		return -1;

	m_LastUse = GetCurTick();
	QMutexLocker Locker(&m_FileMutex);
	if(!m_pFile || !m_pFile->isOpen() || (m_pFile->openMode() & QFile::WriteOnly) == 0)
	{
		if(!Open(QFile::ReadWrite | QFile::Unbuffered))
			return -1;
	}
	if(m_pFile->pos() != offset && !m_pFile->seek(offset))
		return -1;
	qint64 ret = m_pFile->write(data, maxSize);
	m_pFile->flush();

	//QMutexLocker Locker2(&m_Mutex); // m_FileSize is volatile
	if(offset + maxSize > m_FileSize)
		m_FileSize = offset + maxSize;
	return ret;
}

bool CFileIO::Open(QFile::OpenMode Mode)
{
	// Note: enter here only if if filemutex is locked
	if(!m_pFile)
	{
		QMutexLocker Locker2(&m_Mutex); 
		m_pFile = new QFile(m_FileName);

		QMutexLocker LockerX(&m_GlobalMutex);
		ASSERT(!m_GlobalList.contains(this));
		m_GlobalList.append(this);
		while(m_GlobalList.size() > 100)
		{
			CFileIO* pOld = m_GlobalList.first();
			LockerX.unlock();
			pOld->Close();
			LockerX.relock();
		}
	}
	else
		m_pFile->close();

	return m_pFile->open(Mode);
}

void CFileIO::Close()
{
	QMutexLocker Locker(&m_FileMutex);
	if(m_pFile)
	{
		m_pFile->close();
		delete m_pFile;
		m_pFile = NULL;

		QMutexLocker LockerX(&m_GlobalMutex);
		m_GlobalList.removeOne(this);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Multi File

CMultiFileIO::CMultiFileIO(const QString& Name, const QList<CIOManager::SMultiIO>* pSubFiles, CIOManager* Manager)
 : CAbstractIO(Name)
{
	m_Manager = Manager;
	m_SubFiles = *pSubFiles; // Note: this is read only so it can be accessed without a mutex

	m_FileSize = 0;
    for (int i = 0; i < m_SubFiles.size(); i++) 
	{
        const CIOManager::SMultiIO& SubFile = m_SubFiles.at(i);
		m_FileSize += SubFile.uFileSize;
	}
}

bool CMultiFileIO::Rename(const QString & Name)
{
	DeleteDir(m_FileName, true); // delete empty reminding directories
	QMutexLocker Locker(&m_Mutex);
	m_FileName = Name;
	return true;
}

//bool CMultiFileIO::Remove() 
//{
//	DeleteDir(m_FileName, true); // delete empty reminding directories
//	return true;
//}

qint64 CMultiFileIO::Read(qint64 offset, char* data, qint64 maxlen)
{
	m_LastUse = GetCurTick();

	quint64 TotalRead = 0;
    quint64 StartOffset = offset;
    quint64 CurIndex = 0;
	char* Buffer = data;
	quint64 Length = maxlen;
    for (int i = 0; i < m_SubFiles.size() && Length > 0; i++) 
	{
        const CIOManager::SMultiIO& SubFile = m_SubFiles.at(i);
        quint64 CurSize = SubFile.uFileSize;
        if (CurIndex + CurSize > StartOffset) 
		{
			quint64 Offset = StartOffset - CurIndex;
			quint64 ToGo = qMin<quint64>(Length, CurSize - Offset);
			quint64 Read = m_Manager->readData(SubFile.FileID, Offset, Buffer, ToGo);
			if(Read == -1)
				return -1;
			TotalRead += Read;
			if (Read != ToGo) 
				break;
			StartOffset += Read;
			Buffer += Read;
			Length -= Read;
        }
        CurIndex += CurSize;
    }
	//seek(StartOffset); // Warning: calling function does teh seek
    return TotalRead;
}

qint64 CMultiFileIO::Write(qint64 offset, const char* data, qint64 len)
{
	m_LastUse = GetCurTick();

	quint64 TotalWriten = 0;
    quint64 StartOffset = offset;
    quint64 CurIndex = 0;
	const char* Buffer = data;
    quint64 Length = len;
    for (int i = 0; i < m_SubFiles.size() && Length > 0; i++) 
	{
        const CIOManager::SMultiIO& SubFile = m_SubFiles.at(i);
		quint64 CurSize = SubFile.uFileSize;
        if (CurIndex + CurSize > StartOffset) 
		{
			quint64 Offset = StartOffset - CurIndex;
			quint64 ToGo = qMin<quint64>(Length, CurSize - Offset);
			quint64 Writen = m_Manager->writeData(SubFile.FileID, Offset, Buffer, ToGo);
			if(Writen == -1)
				return -1;
			TotalWriten += Writen;
			if (Writen != ToGo) 
				break;
			StartOffset += Writen;
			Buffer += Writen;
			Length -= Writen;
        }
        CurIndex += CurSize;
    }
	// seek(StartOffset); // Warning: calling function does teh seek
    return TotalWriten;
}

void CMultiFileIO::Close()
{
    for (int i = 0; i < m_SubFiles.size(); i++) 
	{
        const CIOManager::SMultiIO& SubFile = m_SubFiles.at(i);
        m_Manager->close(SubFile.FileID);
    }
}
