#pragma once

#include "../../Common/Object.h"

#define AVG_INTERVAL SEC2MS(5)

class CBandwidthCounter: public CObject
{

public:
	CBandwidthCounter(CObject* pParent = NULL);
	~CBandwidthCounter() {}

	virtual void		Process(int iInterval);
	virtual void		ClearRates();

	int					GetRate(/*bool bPayload = false*/) const		{return /*bPayload ? m_iPayloadRate :*/ m_iByteRate;}
	void				CountBytes(int iAmount/*, bool bPayload*/);

protected:
	// the rate is the number of bytes per second we are sending right now
	volatile int		m_iByteRate;
	//volatile int		m_iPayloadRate;

	int					m_Interval;
	struct SStat
	{
		SStat(int i, int b/*, int p*/) {Interval = i; Bytes = b; /*Payload = p;*/}
		int Interval;
		int Bytes;
		//int Payload;
	};
	list<SStat>			m_RateStat;
	int					m_TotalBytes;
	//int					m_TotalPayload;
	int					m_TotalTime;
	int					m_LastBytes;
	//int					m_LastPayload;
};

/*class CBandwidthCounterEx: public CBandwidthCounter
{
public:
	CBandwidthLimitEx(CObject* pParent = NULL);

	virtual void		Process(int iInterval);

	int					GetDeviation() const		{return m_iRateDeviation;}

protected:
	volatile int		m_iRateDeviation;

	struct SDev
	{
		SDev(int r) {Rate = r;}
		int Rate;
	};
	list<SDev>		m_RateDev;
};*/