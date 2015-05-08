#include "GlobalHeader.h"
#include "BandwidthCounter.h"

CBandwidthCounter::CBandwidthCounter(QObject* parent)
: QObject(parent)
{
	m_Interval = 0;

	for(int i=0; i < eCount; i++)
	{
		m_iByteRate[i] = 0;
		m_TotalBytes[i] = 0;
	}
	m_TotalTime = 0;
}

void CBandwidthCounter::CountBytes(int iAmount, EType Type)
{
	ASSERT(iAmount >= 0);
	for(int i=0; i <= Type; i++)
		m_LastBytes[i].fetchAndAddOrdered(iAmount);
}

void CBandwidthCounter::ClearRates()
{
	// Note: this may be called not from the network thread
	for(int i=0; i < eCount; i++)
		m_iByteRate[i] = 0;
}

void CBandwidthCounter::Process(int iInterval)
{
	m_Interval += iInterval;
	if(m_Interval < 500)
		return;

	while(m_TotalTime > AVG_INTERVAL && !m_RateStat.isEmpty())
	{
		m_TotalTime -= m_RateStat.first().Interval;
		for(int i=0; i < eCount; i++)
			m_TotalBytes[i] -= m_RateStat.first().Bytes[i];
		m_RateStat.removeFirst();
	}
	m_RateStat.append(SStat(m_Interval, m_LastBytes));

	m_Interval = 0;
	m_TotalTime += m_RateStat.last().Interval;
	int TotalTime = m_TotalTime;
	if(TotalTime < AVG_INTERVAL/2)
		TotalTime = AVG_INTERVAL;

	for(int i=0; i < eCount; i++)
	{
		m_TotalBytes[i] += m_RateStat.last().Bytes[i];
		m_iByteRate[i] = double(m_TotalBytes[i])*1000.0/TotalTime;
		ASSERT(m_iByteRate[i] >= 0);
	}
}

void CBandwidthCounter::XcrLock(int x)
{
	ASSERT(x == 1 || x == -1);
	int cur = m_LockCounter.fetchAndAddOrdered(x) + x; // return is the original value
	ASSERT(cur >= 0);
	if(cur == 0)
	{
		for(int i=0; i < eCount; i++)
			m_iByteRate[i] = 0;
	}
}