#include "GlobalHeader.h"
#include "BandwidthManager.h"
#include "BandwidthLimit.h"

CBandwidthManager::CBandwidthManager(UINT Channel, CObject* pParent)
 :CObject(pParent)
{
	m_iQueuedBytes = 0;
	m_Channel = Channel;

	m_uLastTick = GetCurTick();
}

CBandwidthManager::~CBandwidthManager()
{
}

void CBandwidthManager::Process()
{
	uint64 uCurTick = GetCurTick();
	uint64 uInterval = uCurTick - m_uLastTick;
	m_uLastTick = uCurTick;

	ASSERT(uInterval < INT_MAX);
	Process((int)uInterval);
}

void CBandwidthManager::RequestBandwidth(const vector<CBandwidthLimit*>& Limits, CBandwidthLimiter* pLimiter, int iAmount)
{
	ASSERT(iAmount >= 0);

	SEntry* pEntry = &m_Queue[pLimiter];
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
		// Note: we must still put in on the queue so that bandwidth gets measured
		//pLimiter->BandwidthAssigned(m_Channel);
		//m_Queue.remove(pLimiter);
	}
}

bool CBandwidthManager::UpdateEntry(SEntry* pEntry, const vector<CBandwidthLimit*>& Limits)
{
	int ActiveLimits = 0;
	int iPrioritySumm = 0;
	int iPriorityCount = 0;
	//pEntry->Limits.clear();
	for(size_t i=0; i < Limits.size(); i++)
	{
		CBandwidthLimit* pLimit = Limits.at(i);

		if(pLimit->GetLimit() != 0)
			ActiveLimits++;

		if(int iPriority = pLimit->GetPriority())
		{
			iPrioritySumm += iPriority;
			iPriorityCount++;
		}
		//pEntry->Limits.append(pLimit);
	}
	pEntry->Limits = Limits;

	if(iPriorityCount > 0)
		pEntry->iPriority = iPrioritySumm/iPriorityCount;
	else
		pEntry->iPriority = BW_PRIO_NORMAL; // no priority all use default priority

	return ActiveLimits > 0; //!pEntry->Limits.isEmpty()
}

void CBandwidthManager::UpdateLimits(const vector<CBandwidthLimit*>& Limits, CBandwidthLimiter* pLimiter)
{
	if(m_Queue.find(pLimiter) == m_Queue.end())
		return;

	UpdateEntry(&m_Queue[pLimiter], Limits);
}

void CBandwidthManager::ReturnBandwidth(CBandwidthLimiter* pLimiter)
{
	map<CBandwidthLimiter*, SEntry>::iterator L = m_Queue.find(pLimiter);
	if(L == m_Queue.end())
		return;

	SEntry* pEntry = &L->second;

	m_iQueuedBytes -= pEntry->iRequestSize - pEntry->iAssigned;

	// return all assigned quota to all the
	// bandwidth channels this peer belongs to
	for(size_t i=0; i < pEntry->Limits.size(); i++)
		pEntry->Limits.at(i)->ReturnQuota(pEntry->iAssigned);

	m_Queue.erase(L);
}

void CBandwidthManager::Process(int iInterval)
{
	if (m_Queue.empty()) 
		return;

	if (iInterval > SEC2MS(3)) 
		iInterval = SEC2MS(3);

	// for each bandwidth channel, call update_quota(dt)
	for(map<CBandwidthLimiter*, SEntry>::iterator I = m_Queue.begin(); I != m_Queue.end(); ++I)
	{
		SEntry* pEntry = &I->second;
		for(size_t i=0; i < pEntry->Limits.size(); i++)
			pEntry->Limits.at(i)->m_iTmp = 0;
	}

	for(map<CBandwidthLimiter*, SEntry>::iterator I = m_Queue.begin(); I != m_Queue.end(); ++I)
	{
		SEntry* pEntry = &I->second;
		for(size_t i=0; i < pEntry->Limits.size(); i++)
		{
			CBandwidthLimit* pLimit = pEntry->Limits.at(i);
			if (pLimit->m_iTmp == 0) 
				pLimit->Process(iInterval);
			ASSERT(pEntry->iPriority > 0);
			pLimit->m_iTmp += pEntry->iPriority;
		}
	}

	for(map<CBandwidthLimiter*, SEntry>::iterator I = m_Queue.begin(); I != m_Queue.end();)
	{
		CBandwidthLimiter* pLimiter = I->first;
		SEntry* pEntry = &I->second;
		int iQuota = AssignBandwidth(pEntry);
		ASSERT(m_iQueuedBytes >= iQuota);
		m_iQueuedBytes -= iQuota;
#ifdef MSS
		if (pEntry->iAssigned >= Min(pEntry->iRequestSize, MSS) || (--pEntry->iTTL <= 0 && pEntry->iAssigned > 0))
#else
		if(pEntry->iAssigned > 0)
#endif
		{
#ifdef MSS
			pEntry->iTTL = 20;
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
			// Note: we have to keep calling UpdateQuota untill the caunted datarate will become 0 
			//			on controlles that are exclusive to this limiter
			if(pEntry->iIdle <= AVG_INTERVAL + SEC2MS(3))
				pEntry->iIdle += iInterval;
			else
			{
				UpdateEntry(pEntry, vector<CBandwidthLimit*>()); // release limits
				I = m_Queue.erase(I);
				continue;
			}
		}

		++I;
	}
}

int CBandwidthManager::AssignBandwidth(SEntry* pEntry)
{
	ASSERT(pEntry->iAssigned <= pEntry->iRequestSize);

	int iQuota = pEntry->iRequestSize - pEntry->iAssigned;
	ASSERT(iQuota >= 0);

	for(size_t i=0; i < pEntry->Limits.size(); i++)
	{
		CBandwidthLimit* pLimit = pEntry->Limits.at(i);
		if (pLimit->GetLimit() == 0)
			continue;
		// unsigned math to prevent a overflow
		iQuota = Min(uint32(pLimit->GetDistributeQuota()) * pEntry->iPriority / pLimit->m_iTmp, iQuota);
	}
	ASSERT(iQuota >= 0);

	ASSERT(INT_MAX - pEntry->iAssigned > iQuota);
	pEntry->iAssigned += iQuota;

	for(size_t i=0; i < pEntry->Limits.size(); i++)
		pEntry->Limits.at(i)->UseQuota(iQuota);

	ASSERT(pEntry->iAssigned <= pEntry->iRequestSize);

	return iQuota;
}