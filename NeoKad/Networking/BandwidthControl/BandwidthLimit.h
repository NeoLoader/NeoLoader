#pragma once

#include "BandwidthCounter.h"

class CBandwidthLimiter;

#define BW_PRIO_HIGHEST 500
#define BW_PRIO_NORMAL	100
#define BW_PRIO_LOWEST	20

class CBandwidthLimit: public CBandwidthCounter
{

public:
	CBandwidthLimit(CObject* pParent = NULL);
	~CBandwidthLimit();

	void				Add(CBandwidthLimiter* pLimiter, UINT Channel);
	void				Remove(CBandwidthLimiter* pLimiter, UINT Channel);
	size_t				Count()						{return m_Limiters.size();}

	// 0 means infinite
	void				SetLimit(int iLimit);
	int					GetLimit() const			{return m_iLimit;}

	void				SetPriority(int iPriority);
	int					GetPriority()				{return m_iPriority;}

	int					QuotaLeft() const;

	virtual void		Process(int iInterval);
	//void				ClearQuota();

	// this is used when connections disconnect with
	// some quota left. It's returned to its bandwidth
	// channels.
	void				ReturnQuota(int iAmount);

	void				UseQuota(int iAmount);

	int					GetDistributeQuota() const	{return m_iDistributeQuota;}

	// used as temporary storage while distributing bandwidth
	int					m_iTmp;

protected:

	// this is the number of bytes to distribute this round
	int					m_iDistributeQuota;

	// this is the amount of bandwidth we have been assigned without using yet.
	int					m_iQuotaLeft;

	// the limit is the number of bytes per second we are allowed to use.
	volatile int		m_iLimit;

	volatile int		m_iPriority;

	map<CBandwidthLimiter*, UINT> m_Limiters;
};
