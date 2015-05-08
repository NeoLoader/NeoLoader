#include "GlobalHeader.h"
#include "BandwidthLimit.h"
#include "BandwidthLimiter.h"

CBandwidthLimit::CBandwidthLimit(QObject* parent)
: CBandwidthCounter(parent)
{
	m_iQuotaLeft = 0;
	m_iLimit = 0;
	m_iDistributeQuota = 0;
	
	m_iTmp = 0;

	m_iPriority = 0;
}

CBandwidthLimit::~CBandwidthLimit()
{
	m_Mutex.lock();
	for(;;)
	{
		QMap<CBandwidthLimiter*, UINT>::iterator I = m_Limiters.begin();
		if(I == m_Limiters.end())
			break;
		CBandwidthLimiter* pLimiter = I.key();
		if(pLimiter->GetMutex()->tryLock())
		{
			m_Mutex.unlock();
			I.key()->RemoveLimit(this, I.value());
			pLimiter->GetMutex()->unlock();
			m_Mutex.lock();
		}
	}
	m_Mutex.unlock();
}

void CBandwidthLimit::Add(CBandwidthLimiter* pLimiter, UINT Channel)
{
	QMutexLocker Locker(&m_Mutex);

	ASSERT(m_Limiters.find(pLimiter) == m_Limiters.end());
	m_Limiters.insert(pLimiter, Channel);
}

void CBandwidthLimit::Remove(CBandwidthLimiter* pLimiter, UINT Channel)
{
	QMutexLocker Locker(&m_Mutex);

	QMap<CBandwidthLimiter*, UINT>::iterator I = m_Limiters.find(pLimiter);
	if(I != m_Limiters.end())
	{
		ASSERT(I.value() == Channel);
		m_Limiters.erase(I);
	}
	else{
		ASSERT(0);
	}

	if(m_Limiters.isEmpty())
		ClearRates();
}

void CBandwidthLimit::SetLimit(int iLimit)
{
	if(iLimit == -1)
		iLimit = 0;

	ASSERT(iLimit >= 0);
	// if the throttle is more than this, we might overflow
	ASSERT(iLimit < INT_MAX / 31);
	m_iLimit = iLimit;
}

void CBandwidthLimit::SetPriority(int iPriority)
{
	if(iPriority < 0)
		iPriority = 0;
	else if(iPriority > 255)
		iPriority = 255;
	m_iPriority = iPriority;
}

void CBandwidthLimit::Process(int iInterval)
{
	CBandwidthCounter::Process(iInterval);

	if (m_iLimit == 0) 
		return;

	m_iQuotaLeft += (m_iLimit * iInterval + 500) / 1000;
	if (m_iQuotaLeft > m_iLimit * 3) 
		m_iQuotaLeft = m_iLimit * 3;
	m_iDistributeQuota = Max(m_iQuotaLeft, 0);
}

int CBandwidthLimit::QuotaLeft() const
{
	if (m_iLimit == 0) 
		return INT_MAX;
	return Max(m_iQuotaLeft, 0);
}

void CBandwidthLimit::ReturnQuota(int iAmount)
{
	ASSERT(iAmount >= 0);
	if (m_iLimit == 0) 
		return;
	ASSERT(m_iQuotaLeft <= m_iQuotaLeft + iAmount);
	m_iQuotaLeft += iAmount;
}

void CBandwidthLimit::UseQuota(int iAmount)
{
	ASSERT(iAmount >= 0);
	ASSERT(m_iLimit >= 0);
	if (m_iLimit == 0) 
		return;
	m_iQuotaLeft -= iAmount;
}
