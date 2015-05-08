#include "GlobalHeader.h"
#include "JobManager.h"
#include "../MT/ThreadEx.h"
#include <QCoreApplication>

#define CMD_WND_SIZE 10 // max count of commands to execute in paralell

//int _CInterfaceJob_pType = qRegisterMetaType<CInterfaceJob*>("CInterfaceJob*");
int _CInterfaceJob_pType = qRegisterMetaType<CInterfaceJob*>("CInterfaceJob*", (CInterfaceJob**)-1);

CJobManager::CJobManager(QObject* parent)
: QObject(parent)
{
	m_UID_Counter = -1;
	
	m_Blocking = 0;
}

void CJobManager::Suspend(bool bSet)
{
	if(bSet)
	{
		foreach(CInterfaceJob* pJob, m_PendingJobs)
			pJob->Finish(false);
		m_PendingJobs.clear();

		m_UID_Counter = -1;
	}
	else if(m_UID_Counter == -1)
	{
		m_UID_Counter = 0;

		ScheduleNextJob();
	}
}

void CJobManager::ScheduleJob(CInterfaceJob* pJob)
{
	bool bForignThread = QThread::currentThread() != this->thread();

	if(bForignThread)
		QMetaObject::invokeMethod(this, "QueueNextJob", Qt::QueuedConnection,  Q_ARG(CInterfaceJob*, pJob));
	else
		QueueNextJob(pJob);

	if(CBlockingJob* pBlockingJob = qobject_cast<CBlockingJob*>(pJob))
	{
		m_Blocking ++;
		int TimeoutCounter = pBlockingJob->GetTimeout();
		while(!pBlockingJob->IsFinished() && --TimeoutCounter != 0)
		{
			QThreadEx::msleep(10);
			if(!bForignThread)
				QCoreApplication::processEvents();
		}
		if(TimeoutCounter == 0)
			UnScheduleJob(pBlockingJob);
		m_Blocking --;
	}
}

void CJobManager::UnScheduleJob(CInterfaceJob* pJob)
{
	bool bForignThread = QThread::currentThread() != this->thread();

	if(bForignThread)
		QMetaObject::invokeMethod(this, "UnQueueNextJob", Qt::BlockingQueuedConnection,  Q_ARG(CInterfaceJob*, pJob));
	else
		UnQueueNextJob(pJob);
}

void CJobManager::UnQueueNextJob(CInterfaceJob* pJob)
{
	ASSERT(QThread::currentThread() == this->thread());

	if(uint64 ID = pJob->GetUID())
		m_PendingJobs.remove(ID);
	else
		m_QueuedJobs.removeOne(pJob);

	pJob->Finish(false);
}

void CJobManager::QueueNextJob(CInterfaceJob* pJob)
{
	ASSERT(QThread::currentThread() == this->thread());

	m_QueuedJobs.append(pJob);

	ScheduleNextJob();
}

void CJobManager::ScheduleNextJob()
{
	if(m_PendingJobs.size() >= CMD_WND_SIZE)
		return;
	if(m_QueuedJobs.isEmpty())
		return;
	if(m_UID_Counter == -1)
		return;

	CInterfaceJob* pJob = m_QueuedJobs.takeFirst();
	pJob->SetUID(++m_UID_Counter);
	m_PendingJobs.insert(m_UID_Counter, pJob);

	// Note: the UID relaying is not part of the IPC socket, it must be dome by the replying server
	//	this way we have the option to implement asynchroniuse replys using notify

	QVariantMap Request = pJob->GetRequest();
	Request["UID"] = m_UID_Counter;
	emit SendRequest(pJob->GetCommand(), Request);
}

void CJobManager::DispatchResponse(const QString& Command, const QVariantMap& vResponse)
{
	QVariantMap Response = vResponse;
	uint64 UID = Response.take("UID").toULongLong();
	if(CInterfaceJob* pJob = m_PendingJobs.take(UID))
	{
		pJob->HandleResponse(Response);
		pJob->Finish(true);
	}

	ScheduleNextJob();
}
