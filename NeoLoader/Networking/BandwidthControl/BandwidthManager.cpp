#include "GlobalHeader.h"
#include "BandwidthManager.h"
#include "BandwidthLimit.h"

/*#include "SocketThread.h"
#include "../NeoCore.h"
#define CHECK_THREAD ASSERT(QThread::currentThread() == theCore->m_Network);*/

CBandwidthManager::CBandwidthManager(UINT Channel, QObject* parent)
 :QObject(parent)
{
	m_iQueuedBytes = 0;
	m_Channel = Channel;

	m_uLastTick = GetCurTick();
	m_uTimerID = startTimer(10);
}

CBandwidthManager::~CBandwidthManager()
{
	killTimer(m_uTimerID);
}

void CBandwidthManager::RequestBandwidth(const QList<CBandwidthLimit*>& Limits, CBandwidthLimiter* pLimiter, int iAmount)
{
	ASSERT(iAmount >= 0);

	QMutexLocker Locker(&m_Mutex);

	SEntry* pEntry = Get(pLimiter, true);
	if(pEntry->iRequestSize < iAmount)
	{
		m_iQueuedBytes -= pEntry->iRequestSize;
		pEntry->iRequestSize = iAmount;
		m_iQueuedBytes += iAmount;
	}
	if(!UpdateEntry(pEntry, Limits))
	{
		// the connection is not rate limited by any of its
		// bandwidth channels, or it doesn't belong to any
		// channels. There's no point in adding it to
		// the queue, just satisfy the request immediately
		pLimiter->AssignBandwidth(m_Channel, pEntry->iRequestSize);
		m_iQueuedBytes -= pEntry->iRequestSize;
		pEntry->iRequestSize = 0;
		pEntry->iAssigned = 0;
		// Note: we must still put in on the queue so that bandwidth gets measured
		//pLimiter->BandwidthAssigned(m_Channel);
		//m_Queue.remove(pLimiter);
	}
}

bool CBandwidthManager::UpdateEntry(SEntry* pEntry, const QList<CBandwidthLimit*>& Limits)
{
	int ActiveLimits = 0;
	int iPrioritySumm = 0;
	int iPriorityCount = 0;

	for (int j = 0; j < pEntry->Limits.size(); j++)
	{
		CBandwidthLimit* pLimit = pEntry->Limits[j];
		if(!Limits.contains(pLimit))
		{
			pLimit->XcrLock(-1);
			pEntry->Limits.removeAt(j--);
		}
	}

	foreach(CBandwidthLimit* pLimit, Limits)
	{
		if(pLimit->GetLimit() != 0)
			ActiveLimits++;

		if(int iPriority = pLimit->GetPriority())
		{
			iPrioritySumm += iPriority;
			iPriorityCount++;
		}

		if(!pEntry->Limits.contains(pLimit))
		{
			pEntry->Limits.append(pLimit);
			pLimit->XcrLock(1);
		}
	}

	if(iPriorityCount > 0)
		pEntry->iPriority = iPrioritySumm/iPriorityCount;
	else
		pEntry->iPriority = BW_PRIO_NORMAL; // no priority all use default priority
	return ActiveLimits > 0;
}

void CBandwidthManager::UpdateLimits(const QList<CBandwidthLimit*>& Limits, CBandwidthLimiter* pLimiter)
{
	QMutexLocker Locker(&m_Mutex);

	if(SEntry* pEntry = Get(pLimiter))
		UpdateEntry(pEntry, Limits);
}

void CBandwidthManager::ReturnBandwidth(CBandwidthLimiter* pLimiter)
{
	QMutexLocker Locker(&m_Mutex);

	if(SEntry* pEntry = Get(pLimiter))
	{
		m_iQueuedBytes -= pEntry->iRequestSize - pEntry->iAssigned;

		// return all assigned quota to all the
		// bandwidth channels this peer belongs to
		for (int j = 0; j < pEntry->Limits.size(); j++)
		{
			CBandwidthLimit* pLimit = pEntry->Limits[j];
			pLimit->ReturnQuota(pEntry->iAssigned);
		}
		
		m_Queue.remove(pLimiter);
	}
}

void CBandwidthManager::Process(int iInterval)
{
	QMutexLocker Locker(&m_Mutex);

	if (m_Queue.isEmpty()) 
		return;

	if (iInterval > SEC2MS(3)) 
		iInterval = SEC2MS(3);

	// for each bandwidth channel, call update_quota(dt)
	for(QMap<CBandwidthLimiter*, SEntry>::iterator I = m_Queue.begin(); I != m_Queue.end(); ++I)
	{
		SEntry* pEntry = &I.value();
		for (int j = 0; j < pEntry->Limits.size(); j++)
		{
			CBandwidthLimit* pLimit = pEntry->Limits[j];
			pLimit->m_iTmp = 0;
		}
	}

	for(QMap<CBandwidthLimiter*, SEntry>::iterator I = m_Queue.begin(); I != m_Queue.end(); ++I)
	{
		SEntry* pEntry = &I.value();
		for (int j = 0; j < pEntry->Limits.size(); j++)
		{
			CBandwidthLimit* pLimit = pEntry->Limits[j];
			if (pLimit->m_iTmp == 0) 
				pLimit->Process(iInterval);
			ASSERT(pEntry->iPriority > 0);
			pLimit->m_iTmp += pEntry->iPriority;
		}
	}

	for(QMap<CBandwidthLimiter*, SEntry>::iterator I = m_Queue.begin(); I != m_Queue.end();)
	{
		CBandwidthLimiter* pLimiter = I.key();
		SEntry* pEntry = &I.value();
		int iQuota = AssignBandwidth(pEntry);
		ASSERT(m_iQueuedBytes >= iQuota);
		m_iQueuedBytes -= iQuota;
#ifdef MSS
		if (pEntry->iAssigned >= Min(pEntry->iRequestSize, 1300) || (--pEntry->iTTL <= 0 && pEntry->iAssigned > 0))
#else
		if(pEntry->iAssigned > 0)
#endif
		{
#ifdef MSS
			pEntry->iTTL = 20; // we tick every 10 ms so this limit adds up to 200 ms
#endif
			ASSERT(pEntry->iAssigned <= pEntry->iRequestSize);
			pEntry->iRequestSize -= pEntry->iAssigned;
			ASSERT(pEntry->iRequestSize >= 0);
			pLimiter->AssignBandwidth(m_Channel, pEntry->iAssigned);
			//pLimiter->BandwidthAssigned(m_Channel);
			pEntry->iAssigned = 0;
		}

		if(pEntry->iRequestSize > 0)
			pEntry->iIdle = 0;
		else
		{
			// Note: we have to keep calling Process untill the caunted datarate will become 0 
			//			on controlles that are exclusive to this limiter
			if(pEntry->iIdle <= AVG_INTERVAL + SEC2MS(3))
				pEntry->iIdle += iInterval;
			else
			{
				UpdateEntry(pEntry, QList<CBandwidthLimit*>()); // release limits
				I = m_Queue.erase(I);
				continue;
			}
		}

		++I;
	}
}

CBandwidthManager::SEntry* CBandwidthManager::Get(CBandwidthLimiter* pLimiter, bool bAdd)
{
	QMap<CBandwidthLimiter*, SEntry>::iterator I = m_Queue.find(pLimiter);
	if(I == m_Queue.end())
	{
		if(!bAdd)
			return NULL;
		I = m_Queue.insert(pLimiter, SEntry());
	}
	return &I.value();
}

int CBandwidthManager::AssignBandwidth(SEntry* pEntry)
{
	ASSERT(pEntry->iAssigned <= pEntry->iRequestSize);

	int iQuota = pEntry->iRequestSize - pEntry->iAssigned;
	ASSERT(iQuota >= 0);

	for (int j = 0; j < pEntry->Limits.size(); j++)
	{
		CBandwidthLimit* pLimit = pEntry->Limits[j];
		if (pLimit->GetLimit() == 0)
			continue;
		// unsigned math to prevent a overflow
		iQuota = Min(uint32(pLimit->GetDistributeQuota()) * pEntry->iPriority / pLimit->m_iTmp, iQuota);
	}
	ASSERT(iQuota >= 0);

	ASSERT(INT_MAX - pEntry->iAssigned > iQuota);
	pEntry->iAssigned += iQuota;

	for (int j = 0; j < pEntry->Limits.size(); j++)
	{
		CBandwidthLimit* pLimit = pEntry->Limits[j];
		pLimit->UseQuota(iQuota);
	}

	ASSERT(pEntry->iAssigned <= pEntry->iRequestSize);

	return iQuota;
}