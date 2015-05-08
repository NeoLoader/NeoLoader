#pragma once

#include "../../Common/Object.h"

#include "BandwidthLimiter.h"
class CBandwidthLimit;

class CBandwidthManager: public CObject
{

public:

	CBandwidthManager(UINT Channel, CObject* pParent = NULL);
	~CBandwidthManager();

	void				Process();

	bool				IsQueued(CBandwidthLimiter* pLimiter)		{return m_Queue.find(pLimiter) != m_Queue.end();}
	size_t				GetQueueSize() const						{return m_Queue.size();}
	int					GetQueuedBytes() const						{return m_iQueuedBytes;}
	
	// non prioritized means that, if there's a line for bandwidth,
	// others will cut in front of the non-prioritized peers.
	// this is used by web seeds
	void				RequestBandwidth(const vector<CBandwidthLimit*>& Limits, CBandwidthLimiter* pLimiter, int iAmount);

	void				UpdateLimits(const vector<CBandwidthLimit*>& Limits, CBandwidthLimiter* pLimiter);

	void				ReturnBandwidth(CBandwidthLimiter* pLimiter);

	void				Process(int iInterval);

protected:
	uint64				m_uLastTick;
	
	struct SEntry
	{
		SEntry()
		{
			iPriority = 1;
			iAssigned = 0;
			iRequestSize = 0;
#ifdef MSS
			iTTL = 20;
#endif
			iIdle = 0;
		}

		// 1 is normal prio
		int iPriority;

		// the number of bytes assigned to this request so far
		int iAssigned;

		// once assigned reaches this, we dispatch the request function
		int iRequestSize;

		// the max number of rounds for this request to survive
		// this ensures that requests gets responses at very low
		// rate limits, when the requested size would take a long
		// time to satisfy
#ifdef MSS
		int iTTL;
#endif

		// holds for how long the entry was idle
		int iIdle;

		vector<CBandwidthLimit*> Limits;
	};

	// loops over the bandwidth channels and assigns bandwidth
	// from the most limiting one
	static int			AssignBandwidth(SEntry* pEntry);

	static bool			UpdateEntry(SEntry* pEntry, const vector<CBandwidthLimit*>& Limits);

	// these are the consumers that want bandwidth
	map<CBandwidthLimiter*, SEntry>	m_Queue;

	// the number of bytes all the requests in queue are for
	int					m_iQueuedBytes;

	// this is the channel within the consumers that bandwidth is assigned to (upload or download)
	UINT				m_Channel;
};