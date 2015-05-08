#include "GlobalHeader.h"
#include "BandwidthCounter.h"
#include "../../../Framework/Maths.h"

CBandwidthCounter::CBandwidthCounter(CObject* pParent)
: CObject(pParent)
{
	m_Interval = 0;
	m_iByteRate = 0;
	//m_iPayloadRate = 0;
	m_TotalBytes = 0;
	//m_TotalPayload = 0;
	m_TotalTime = 0;
	m_LastBytes = 0;
	//m_LastPayload = 0;
}

void CBandwidthCounter::CountBytes(int iAmount/*, bool bPayload*/)
{
	ASSERT(iAmount >= 0);
	m_LastBytes += iAmount;
	//if(bPayload) 
	//	m_LastPayload += iAmount;
}

void CBandwidthCounter::ClearRates()
{
	m_iByteRate = 0;
}

void CBandwidthCounter::Process(int iInterval)
{
	m_Interval += iInterval;
	if(m_Interval < 100)
		return;

	while(m_TotalTime > AVG_INTERVAL && !m_RateStat.empty())
	{
		m_TotalTime -= m_RateStat.front().Interval;
		m_TotalBytes -= m_RateStat.front().Bytes;
		//m_TotalPayload -= m_RateStat.front().Payload;
		m_RateStat.pop_front();
	}

	m_RateStat.push_back(SStat(m_Interval, m_LastBytes/*, m_LastPayload*/));
	m_Interval = 0;
	m_LastBytes = 0;
	//m_LastPayload = 0;
	m_TotalTime += m_RateStat.back().Interval;
	m_TotalBytes += m_RateStat.back().Bytes;
	//m_TotalPayload += m_RateStat.back().Payload;

	int TotalTime = m_TotalTime;
	if(TotalTime < AVG_INTERVAL/2)
		TotalTime = AVG_INTERVAL;
	m_iByteRate = double(m_TotalBytes)*1000.0/TotalTime;
	//m_iPayloadRate = double(m_TotalPayload)*1000.0/TotalTime;
	ASSERT(m_iByteRate >= 0 /*&& m_iPayloadRate >= 0*/);
}

///////////////////////////////////////////////////////////////////////////////////
//

/*CBandwidthCounterEx::CBandwidthCounterEx(CObject* pParent)
:CBandwidthCounter(pParent)
{
	m_iRateDeviation = 0;
}

void CBandwidthCounterEx::Process(int iInterval)
{
	CBandwidthCounter::Process(iInterval);

	while(m_RateDev.size() >= m_RateStat.size())
		m_RateDev.pop_front();
	m_RateDev.push_back(SDev(m_iByteRate));

	qint64 RateAvg = 0;
	for(list<SDev>::iterator I = m_RateDev.begin(); I != m_RateDev.end(); I++)
		RateAvg += (qint64)(*I).Rate;
	RateAvg /= (qint64)m_RateDev.size();

	qint64 RateDev = 0;
	for(list<SDev>::iterator I = m_RateDev.begin(); I != m_RateDev.end(); I++)
	{
		qint64 Dev = (qint64)(*I).Rate - RateAvg;
		RateDev += Dev * Dev;
	}
	RateDev /= (qint64)m_RateDev.size();
	m_iRateDeviation = isqrt(RateDev);
	//qDebug() << (double)m_iRateDeviation/1000.0 << "kb/s";
}*/