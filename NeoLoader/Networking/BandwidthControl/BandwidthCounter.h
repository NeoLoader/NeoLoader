#pragma once

#include "../../../Framework/ObjectEx.h"

#ifdef _DEBUG
#define AVG_INTERVAL SEC2MS(10)
#else
#define AVG_INTERVAL SEC2MS(30)
#endif

class CBandwidthCounter: public QObject
{
	Q_OBJECT
public:
	CBandwidthCounter(QObject* parent = 0);

	virtual void		Process(int iInterval);
	virtual void		ClearRates();

	enum EType
	{
		eAll = 0, // all traffic including Syn packets
		eAck = 1, // playload and protocol overhead and IP Headers and payload ACKs
		eHeader = 2, // playload and protocol overhead and IP Headers
		eProtocol = 3, // playload and protocol overhead
		ePayload = 4, // payload
		eCount
	};

	void				CountBytes(int iAmount, EType Type);
	int					GetRate(EType Type = eAll) const		{return m_iByteRate[Type];}
	void				XcrLock(int x);

protected:
	// the rate is the number of bytes per second we are sending right now
	volatile int		m_iByteRate[eCount];
	QAtomicInt			m_LockCounter;

	int					m_Interval;
	struct SStat
	{
		SStat(int i, QAtomicInt* b) 
		{
			Interval = i; 
			for(int i=0; i < eCount; i++)
				Bytes[i] = b[i].fetchAndStoreOrdered(0); 
		}
		int Interval;
		int Bytes[eCount];
	};
	QList<SStat>		m_RateStat;
	int					m_TotalBytes[eCount];
	int					m_TotalTime;
	QAtomicInt			m_LastBytes[eCount];
};

//////////////////////////////////////////////////////////////////////////////////////////
//

#include "../../../Framework/Maths.h"

template <class T>
class CExtSDev: public T
{
public:
	CExtSDev(QObject* parent = 0)
	 :T(parent)
	{
		m_iRateDeviation = 0;
		m_TimeCounter = 0;
		m_RateSumm = 0;
		m_RateCount = 0;
	}

	virtual void		Process(int iInterval)
	{
		T::Process(iInterval);

        if(T::m_RateStat.isEmpty())
			return;

		m_TimeCounter += iInterval;
        T::m_RateSumm += T::m_iByteRate;
		m_RateCount++;
		if(m_TimeCounter < SEC2MS(1))
			return;
		m_TimeCounter = 0;

		uint64 RateTmp = m_RateSumm / m_RateCount;
		m_RateCount = 0;
		m_RateSumm = 0;
		//qDebug() << "AvgRate: " << (double)RateTmp/1000.0 << "kb/s";

		while(m_RateDev.size() > 15)
			m_RateDev.pop_front();
        m_RateDev.push_back(T::SDev(RateTmp));
		ASSERT(!m_RateDev.isEmpty());

        typedef QList<typename T::SDev> TSDevList;

		qint64 RateAvg = 0;
        for(typename TSDevList::iterator I = T::m_RateDev.begin(); I != T::m_RateDev.end(); I++)
			RateAvg += (qint64)(*I).Rate;
		RateAvg /= (qint64)m_RateDev.size();

		qint64 RateDev = 0;
        for(typename TSDevList::iterator I = T::m_RateDev.begin(); I != T::m_RateDev.end(); I++)
		{
			qint64 Dev = (qint64)(*I).Rate - RateAvg;
			RateDev += Dev * Dev;
		}
		RateDev /= (qint64)m_RateDev.size();
		m_iRateDeviation = isqrt(RateDev);
		//qDebug() << "RateDev: "<< (double)m_iRateDeviation/1000.0 << "kb/s";
	}

	int					GetDeviation() const		{return m_iRateDeviation;}

protected:
	volatile int		m_iRateDeviation;
	uint64				m_TimeCounter;
	uint64				m_RateSumm;
	uint64				m_RateCount;

	struct SDev
	{
		SDev(int r) {Rate = r;}
		int Rate;
	};
	QList<SDev>		m_RateDev;
};
