#pragma once

#include "../../../Framework/MT/ThreadEx.h"
#include "../PartMap.h"
#include "FileHash.h"
class CManagedIO;
class CHashingThread;

class CHashingJob: public QThreadEx
{
	Q_OBJECT

public:
	CHashingJob(uint64 FileID, const QList<CFileHashPtr>& List, CPartMapPtr Parts = CPartMapPtr());
	~CHashingJob() {}

	virtual void			Execute(CManagedIO* pDevice) = 0;

	virtual uint64			GetFileID()				{return m_FileID;}
	virtual bool			IsLongJob()				{return true;}
	virtual int 			GetPriority()			{return 0;}

	virtual void			SetThread(CHashingThread* pThread)	{m_pThread = pThread;}

signals:
	void					Finished();

protected:
	uint64					m_FileID;
	CPartMapPtr				m_Parts;
	QList<CFileHashRef>		m_List;

	CHashingThread*			m_pThread;
};

class CVerifyPartsJob: public CHashingJob
{
	Q_OBJECT

public:
	CVerifyPartsJob(uint64 FileID, const QList<CFileHashPtr>& List, CPartMapPtr Parts, EHashingMode Mode = eVerifyParts);
	CVerifyPartsJob(uint64 FileID, CFileHashPtr pHash, CPartMapPtr Parts, uint64 uOffset, uint64 uSize, EHashingMode Mode = eVerifyParts);
	
	virtual void			Execute(CManagedIO* pDevice);

	virtual bool			IsLongJob()				{return m_Mode != eVerifyParts;}
	virtual int 			GetPriority()			{return 1;}

protected:
	EHashingMode			m_Mode;
	uint64					m_uFrom;
	uint64					m_uTo;
};


class CHashFileJob: public CHashingJob
{
	Q_OBJECT

public:
	CHashFileJob(uint64 FileID, const QList<CFileHashPtr>& List);
	
	virtual void			Execute(CManagedIO* pDevice);

	virtual int 			GetPriority()			{return 10;}

protected:
};

class CImportPartsJob: public CHashingJob
{
	Q_OBJECT

public:
	CImportPartsJob(uint64 FileID, const QList<CFileHashPtr>& List, CPartMapPtr Parts, const QString& FilePath, bool bDeleteSource = false);
	
	virtual void			Execute(CManagedIO* pDevice);

	virtual int 			GetPriority()			{return 5;}

protected:
	QString					m_FilePath;
	bool					m_bDeleteSource;
};
