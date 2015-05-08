#pragma once
//#include "GlobalHeader.h"

#include "../../Framework/MT/ThreadEx.h"
#include "../../Framework/MT/ThreadLock.h"

class CManagedIO;
class CIOOperation;
class CFileAllocator;

class CAbstractIO
{
public:
	CAbstractIO(const QString& Name);
	virtual ~CAbstractIO();

	virtual void		QueueOp(CIOOperation* pOperation);
	virtual void		Process()							{}

	virtual void		Execute();

	virtual int			GetQueueSize()						{QMutexLocker Locker(&m_QueueMutex); return m_Operations.size();}

	virtual QString		GetFileName() const					{QMutexLocker Locker(&m_Mutex); return m_FileName;}
	virtual bool 		Rename(const QString & Name) = 0;
	//virtual bool 		Remove() = 0;

	virtual qint64		GetSize() const						{return m_FileSize;}
	virtual void		SetReadOnly(bool bReadOnly)			{m_ReadOnly = bReadOnly;}

	virtual qint64 		Read(qint64 offset, char* data, qint64 maxSize) = 0;
	virtual qint64 		Write(qint64 offset, const char* data, qint64 maxSize) = 0;

	virtual void		Close() = 0;

	virtual bool		IsMulti() const = 0;

	virtual uint64		GetLastUse()						{return m_LastUse;}

protected:
	mutable QMutex		m_Mutex;
	QString				m_FileName;
	volatile uint64		m_FileSize;
	volatile bool		m_ReadOnly;

	QMutex				m_QueueMutex;
	QMultiMap<uint64, CIOOperation*> m_Operations;

	volatile uint64		m_LastUse;
};


///////////////////////////////////////////////////////////////////////////////////////////////
//

typedef QSharedPointer<CAbstractIO> CIOPtr;

class CIOManager: public QThreadEx
{
	Q_OBJECT

public:
	CIOManager(QObject* qObject = NULL);
	~CIOManager();

	void					run();
	void					Stop();

	void					ReadData(QObject* Reciver, uint64 FileID, uint64 Offset, uint64 Length, void* Aux = NULL);
	void					WriteData(QObject* Reciver, uint64 FileID, uint64 Offset, const QByteArray& Data, void* Aux = NULL);

	struct SMultiIO
	{
		SMultiIO(uint64 uSize, uint64 ID)
		{
			uFileSize = uSize;
			FileID = ID;
		}
		uint64 uFileSize;
		uint64 FileID;
	};

	void					InstallIO(uint64 FileID, const QString& FilePath, const QList<SMultiIO>* pSubFiles = NULL);
	void					ProtectIO(uint64 FileID, bool bSetReadOnly);
	bool					MoveIO(uint64 FileID, const QString& FilePath);
	void					UninstallIO(uint64 FileID);
	//bool					DeleteIO(uint64 FileID);
	bool					HasIO(uint64 FileID)			{QMutexLocker Locker(&m_FilesMutex); return m_Files.contains(FileID);}
	//void					CloseIO(uint64 FileID);
	uint64					GetSize(uint64 FileID);
	void					Allocate(uint64 FileID, uint64 FileSize, QObject* Reciver = NULL);
	bool					IsAllocating(uint64 FileID);

	bool					IsWriteBufferFull(bool bHalf = false);

	int						GetPendingReadSize()			{return m_PendingReadSize.load();}
	int						GetPendingWriteSize()			{return m_PendingWriteSize.load();}

	CManagedIO*				GetDevice(uint64 FileID);

	int						GetAllocationCount()			{QMutexLocker Locker(&m_FilesMutex); return m_PendingAllocations.size();}

protected:
	friend class CManagedIO;
	friend class CMultiFileIO;
	friend class CReadOperation;
	friend class CWriteOperation;
	friend class CCloseOperation;

	CIOPtr					GetFile(uint64 FileID) const;

	bool					QueueOp(uint64 FileID, CIOOperation* pOperation);

	QString					fileName(uint64 FileID) const;

	CIOPtr					TakeFile(uint64 FileID);

	qint64					size(uint64 FileID) const;
	qint64					readData(uint64 FileID, uint64 Offset, char *data, qint64 maxlen);
	qint64					writeData(uint64 FileID, uint64 Offset, const char *data, qint64 len);
	void					close(uint64 FileID);

	bool					m_Stop;

	mutable QMutex			m_FilesMutex;
	QMap<uint64, CIOPtr>	m_Files;
	mutable QMutex			m_ActiveMutex;
	QSet<CIOPtr>			m_ActiveFiles;

	mutable QMutex			m_AllocationsMutex;
	QMap<uint64, CFileAllocator*>	m_PendingAllocations;
	uint64					m_PendingAllocation;

	QAtomicInt				m_PendingReadSize;
	QAtomicInt				m_PendingWriteSize;
};

///////////////////////////////////////////////////////////////////////////////////////////////
//

class CFileAllocator: public QObject
{
	Q_OBJECT

public:
	CFileAllocator(CIOManager* Manager, uint64 uFileSize) {m_Manager = Manager; m_uFileSize = uFileSize;}

	virtual bool		Execute(CAbstractIO* File)
	{
		static char Tmp[MB2B(2)] = {1};
		if(Tmp[0]) memset(Tmp, 0, sizeof(Tmp));

		qint64 uLen = m_uFileSize;
		qint64 uSize = File->GetSize();
		ASSERT(uSize <= uLen);
		//while(uSize < uLen)
		if(uSize < uLen)
		{
			qint64 uTmp = uLen - uSize;
			if(uTmp > MB2B(2))
				uTmp = MB2B(2);
			int Writen = File->Write(uSize, Tmp, uTmp);
			if(Writen > 0)
				uSize += Writen;
			//else
			//	break;
			File->Close(); // force hard buffer flushing
		}
		
		bool Finished = uSize >= uLen;
		emit Allocation(uSize, Finished);
		return Finished;
	}

signals:
	void				Allocation(uint64 Progress, bool Finished);

protected:
	CIOManager* m_Manager;
	uint64		m_uFileSize;
}; 

///////////////////////////////////////////////////////////////////////////////////////////////
//

class CIOOperation: public QObject
{
	Q_OBJECT

public:
	CIOOperation(CIOManager* Manager) {m_Manager = Manager;}

	virtual void		Execute(CAbstractIO* File) = 0;
	virtual uint64		GetOffset() {return -1;}

signals:
	void				DataRead(uint64 Offset, uint64 Length, const QByteArray& Data, bool bOk, void* Aux);
	void				DataWriten(uint64 Offset, uint64 Length, bool bOk, void* Aux);

protected:
	CIOManager* m_Manager;
};

/*class CCloseOperation: public CIOOperation
{
public:
	CCloseOperation(CIOManager* Manager)
		: CIOOperation(Manager)
	{}

	virtual void		Execute(CAbstractIO* File)	{File->Close();}
};*/

class CReadOperation: public CIOOperation
{
public:
	CReadOperation(CIOManager* Manager, uint64 Offset, uint64 Length, void* Aux)
		: CIOOperation(Manager)
	{
		m_Offset = Offset;
		m_Length = Length;
		m_Aux = Aux;

		m_Manager->m_PendingReadSize.fetchAndAddOrdered(m_Length);
	}
	~CReadOperation()
	{
		m_Manager->m_PendingReadSize.fetchAndAddOrdered(-((int)m_Length));
	}

	virtual void		Execute(CAbstractIO* File)
	{
		QByteArray Data;
		Data.resize(m_Length);
		uint64 Length = File->Read(m_Offset, Data.data(), Data.size());
		bool bOk = Data.size() == Length;
		if(Length == -1)
			Length = 0;
		Data.truncate(Length);
		emit DataRead(m_Offset, m_Length, Data, bOk, m_Aux);
	}
	virtual uint64		GetOffset() {return m_Offset;}

protected:
	uint64		m_Offset;
	uint64		m_Length;
	void*		m_Aux;
};

class CWriteOperation: public CIOOperation
{
public:
	CWriteOperation(CIOManager* Manager, uint64 Offset, const QByteArray& Data, void* Aux)
		: CIOOperation(Manager)
	{
		m_Offset = Offset;
		m_Data = Data;
		m_Aux = Aux;

		m_Manager->m_PendingWriteSize.fetchAndAddOrdered(m_Data.size());
	}
	~CWriteOperation()
	{
		m_Manager->m_PendingWriteSize.fetchAndAddOrdered(-((int)m_Data.size()));
	}

	virtual void		Execute(CAbstractIO* File)
	{
		uint64 Length = File->Write(m_Offset, m_Data.data(), m_Data.size());
		bool bOk = m_Data.size() == Length;
		emit DataWriten(m_Offset, m_Data.size(), bOk, m_Aux);
	}
	virtual uint64		GetOffset() {return m_Offset;}

protected:
	uint64		m_Offset;
	QByteArray	m_Data;
	void*		m_Aux;
};

///////////////////////////////////////////////////////////////////////////////////////////////
//

class CManagedIO: public QIODevice
{
	Q_OBJECT

public:
	CManagedIO(CIOManager* Manager, uint64 FileID)
	{
		m_Manager = Manager;
		m_FileID = FileID;
		//m_Offset = 0;
		m_Offset = NULL;
	}

	virtual QString		fileName() const						{return m_Manager->fileName(m_FileID);}

	//virtual bool		open(OpenMode flags)					{return QIODevice::open(flags)}
	virtual qint64		size() const							{return m_Manager->size(m_FileID);}
	virtual bool		isSequential() const					{return false;}
	//virtual qint64		pos() const								{return m_Offset;}
	//virtual bool		seek(qint64 offset)						{m_Offset = offset; return true;}
	virtual bool		atEnd() const							{return pos() == size();}
	//virtual void		close()									{close();}

	virtual void		tell(volatile uint64* offset)			{m_Offset = offset;}

protected:

	virtual qint64		readData(char *data, qint64 maxlen)		
	{
		uint64 ret = m_Manager->readData(m_FileID, pos(), data, maxlen);
		if(m_Offset)
			*m_Offset = pos();
		return ret;
	}
	virtual qint64		writeData(const char *data, qint64 len)	
	{
		uint64 ret = m_Manager->writeData(m_FileID, pos(), data, len);
		if(m_Offset)
			*m_Offset = pos();
		return ret;
	}
	
private:
	CIOManager* m_Manager;
	uint64		m_FileID;
	//uint64		m_Offset;
	volatile uint64*	m_Offset;
};

///////////////////////////////////////////////////////////////////////////////////////////////
//

class CIOProxy: public QIODevice
{
	Q_OBJECT

public:
	CIOProxy(QIODevice* pDevice) //: m_Mutex(QMutex::Recursive)
	{
		m_pDevice = pDevice;
	}

	QMutex*				GetMutex()								{return &m_Mutex;}
	void				Detache()								{m_pDevice = NULL;}
	QIODevice*			GetDevice()								{return m_pDevice;}

	virtual qint64		size() const							{QMutexLocker Locker(&m_Mutex); return m_pDevice ? m_pDevice->size() : 0;}
	virtual bool		isSequential() const					{return false;}
	virtual bool		atEnd() const							{return pos() == size();}

protected:
	virtual qint64		readData(char *data, qint64 maxlen)
	{
		QMutexLocker Locker(&m_Mutex);

		if(!m_pDevice || !m_pDevice->seek(pos()))
			return -1;
		return m_pDevice->read(data, maxlen);
	}

	virtual qint64		writeData(const char *data, qint64 len)
	{
		QMutexLocker Locker(&m_Mutex);

		if(!m_pDevice || !m_pDevice->seek(pos()))
			return -1;
		return m_pDevice->write(data, len);
	}
	
private:
	mutable QMutex		m_Mutex;
	QIODevice*			m_pDevice;
};

///////////////////////////////////////////////////////////////////////////////////////////////
//

class CFileIO: public CAbstractIO
{
public:
	CFileIO(const QString& Name);
	virtual ~CFileIO();

	virtual void		Process();

	virtual bool 		Rename(const QString & Name);
	//virtual bool 		Remove();

	virtual qint64 		Read(qint64 offset, char* data, qint64 maxSize);
	virtual qint64 		Write(qint64 offset, const char* data, qint64 maxSize);

	virtual void		Close();

	virtual bool		IsMulti() const		{return false;}

protected:
	virtual bool		Open(QFile::OpenMode Mode);

	QMutex				m_FileMutex;
	QFile*				m_pFile;

	static QMutex		m_GlobalMutex;
	static QList<CFileIO*>m_GlobalList;
};

///////////////////////////////////////////////////////////////////////////////////////////////
//

class CMultiFileIO: public CAbstractIO
{
public:
	CMultiFileIO(const QString& Name, const QList<CIOManager::SMultiIO>* pSubFiles, CIOManager* Manager);
	virtual ~CMultiFileIO() {}

	virtual bool 		Rename(const QString & Name);
	//virtual bool 		Remove();

	virtual qint64 		Read(qint64 offset, char* data, qint64 maxSize);
	virtual qint64 		Write(qint64 offset, const char* data, qint64 maxSize);

	virtual void		Close();

	virtual bool		IsMulti() const							{return true;}

	virtual const QList<CIOManager::SMultiIO>& GetSubFiles()	{return m_SubFiles;}

protected:
	QList<CIOManager::SMultiIO>	m_SubFiles;

	CIOManager*			m_Manager;
};
