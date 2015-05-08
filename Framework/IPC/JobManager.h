#pragma once

#include "../../Framework/ObjectEx.h"

class NEOHELPER_EXPORT CInterfaceJob: public QObject
{
	Q_OBJECT
public:
	CInterfaceJob()	{m_UID = 0; m_StartTime = 0;}

	void					SetUID(uint64 UID)	{m_UID = UID; m_StartTime = GetCurTick();}
	uint64					GetUID()			{return m_UID;}
	virtual QString			GetCommand() = 0;
	virtual QVariantMap		GetRequest()		{return m_Request;}
	virtual void			HandleResponse(const QVariantMap& Response) = 0;
	virtual uint64			GetDuration()		{return m_StartTime ? GetCurTick() - m_StartTime : 0;}
	virtual void			Finish(bool bOK)	{delete this;}

protected:
	uint64					m_UID;
	QVariantMap				m_Request;
	uint64					m_StartTime;
};

class NEOHELPER_EXPORT CBlockingJob: public CInterfaceJob
{
	Q_OBJECT
public:
	CBlockingJob()
	{
		m_Finished = false;
	}

	virtual QVariantMap		GetRequest()		{QMutexLocker Locker(&m_Mutex); return m_Request;}
	virtual void			HandleResponse(const QVariantMap& Response)	{QMutexLocker Locker(&m_Mutex); m_Response = Response;}
	virtual void			Finish(bool bOK)	{m_Finished = true;}
	virtual bool			IsFinished()		{return m_Finished;}
	virtual int				GetTimeout()		{return 0;}

protected:
	volatile bool			m_Finished;
	QMutex					m_Mutex;
	QVariantMap				m_Response;
};

class NEOHELPER_EXPORT CJobManager: public QObject
{
	Q_OBJECT
public:
	CJobManager(QObject* parent = 0);

	void				Suspend(bool bSet);
	bool				IsSuspended()			{return m_UID_Counter == -1;}

	void				ScheduleJob(CInterfaceJob* pJob);
	void				UnScheduleJob(CInterfaceJob* pJob);
	bool				IsBlocking() {return m_Blocking > 0;}

public slots:
	void				DispatchResponse(const QString& Command, const QVariantMap& Response);

signals:
	void				SendRequest(const QString& Command, const QVariantMap& Request);

private slots:
	void				QueueNextJob(CInterfaceJob* pJob);
	void				UnQueueNextJob(CInterfaceJob* pJob);

protected:
	void				ScheduleNextJob();

	uint64				m_UID_Counter;
	QList<CInterfaceJob*> m_QueuedJobs;
	QMap<uint64, CInterfaceJob*> m_PendingJobs;
	int					m_Blocking;
};
