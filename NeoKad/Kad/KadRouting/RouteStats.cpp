#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "../Kademlia.h"
#include "../KadConfig.h"
#include "RouteStats.h"

SRouteStats::SRouteStats()
{
	PendingFrames = 0;
	RelayedFrames = 0;
	DroppedFrames = 0;

	uEstimatedRTT = 0;
	uRTTDeviation = 0;
}

void SRouteStats::AddSample(uint32 uSampleRTT)
{
	int iSampleRTT = uSampleRTT; // Must be a signed value !
	// Jacobson and Karels' New Algorithm - TCP
	if(uEstimatedRTT == 0) // its the verry first ACK we got
	{
		uEstimatedRTT = iSampleRTT;
		uRTTDeviation = 0;
		TimeOut.Init(10, iSampleRTT << 1); // better a to log timeout than a to short one
	}
	else // calculate the proper TimeOut and average RTT
	{
		// RTT and TimeOut calculation
		iSampleRTT -= (uEstimatedRTT >> 3);							// EstimatedRTT = 8 * EstRTT; 
																	// Deviation = 8 * Dev
																	// SampleRTT = (SampleRTT – 8 * EstRTT) / 8 = SampleRTT – EstRTT)
		uEstimatedRTT += iSampleRTT;								// 8 * EstRTT = 8EstRTT + SampleRTT = 8EstRTT + (SampleRTT – EstRTT)
																	// EstRTT = EstRTT + 1/8 (SampleRTT – EstRTT)
		if (iSampleRTT < 0)
			iSampleRTT = -iSampleRTT;
		iSampleRTT -= (uRTTDeviation >> 3);							// |SampleRTT – EstRTT| – 8Dev/8
		uRTTDeviation += iSampleRTT;								// 8 * Dev = 8 * Dev + |SampleRTT – EsRTT| - Dev
																	// Dev = Dev + 1/8( |SampleRTT – EstRTT| - Dev )
		TimeOut.Add((uEstimatedRTT >> 3) + (uRTTDeviation >> 1));	// TimeOut = 8 * EstRTT / 8 + 8 * Dev / 2 = EstRTT + 4 * Dev
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//

IMPLEMENT_OBJECT(CRouteStats, CObject)

CRouteStats::CRouteStats(SRouteStats* pStats, uint32 uMinTimeOut, uint32 uMaxTimeOut, uint32 uCongestionLimit, CObject* pParent) 
 : CObject(pParent)
{
	if(!pStats)
		pStats = new SRouteStats();
	m_pStats = pStats;

	m_uMinTimeOut = uMinTimeOut;
	m_uMaxTimeOut = uMaxTimeOut;
	m_TimeOutAverage = m_uMaxTimeOut;

	m_uCongestionWindow = 1;
	m_uCongestionLimit = uCongestionLimit;
	m_uCongestionThreshold = m_uCongestionLimit;
	m_uIncrementalCounter = 0;
}

void CRouteStats::FramePending()
{
	m_pStats->PendingFrames++;
}

void CRouteStats::FrameDropped()
{
	m_pStats->PendingFrames--;
	m_pStats->DroppedFrames++;
		
	// set the exponential increese treshold to the half of the currently reached window size
	//m_uCongestionThreshold = Max(1, m_uCongestionWindow >> 1); // m_uCongestionWindow / 2 // multiplicative decrease
	//m_uCongestionWindow = 1; // and reset the window size to 1
	m_uCongestionThreshold = m_uCongestionWindow;
	m_uCongestionWindow = Max(1, m_uCongestionWindow >> 1);
}

void CRouteStats::FrameRelayed()
{
	m_pStats->PendingFrames--;
	m_pStats->RelayedFrames++;

	// increase the window size exponentialy
	if(m_uCongestionWindow <= m_uCongestionThreshold)
		m_uCongestionWindow += 1; // if we increese the window each time we get an ack this will result in doubling the window size on each round
	else if(++m_uIncrementalCounter >= m_uCongestionWindow) // if we reached the set treshold start increesing only by 1 frame on each round
	{
		m_uIncrementalCounter = 0;
		m_uCongestionWindow += 1;
	}
	if(m_uCongestionWindow > m_uCongestionLimit)
		m_uCongestionWindow  = m_uCongestionLimit;
}

void CRouteStats::UpdateControl()
{
	m_TimeOutAverage = m_pStats->TimeOut.GetAverage();
	if(m_TimeOutAverage < m_uMinTimeOut)
		m_TimeOutAverage = m_uMinTimeOut;
	else if(m_TimeOutAverage > m_uMaxTimeOut)
		m_TimeOutAverage = m_uMaxTimeOut;

	// We schrink the Congestion Window if more than the half of it is unused
	// So that once we start sending more data again we can avoind a high bandwidth spike
	if((m_uCongestionWindow >> 1) > Max(m_pStats->PendingFrames, 2)) 
		m_uCongestionWindow = (m_uCongestionWindow >> 1) + 1;
}

bool CRouteStats::IsWindowFull()
{
	return m_uCongestionWindow <= m_pStats->PendingFrames;
}

//void CRouteStats::UpdateLoad(const CVariant& Load)
//{
//	int PendingFrames = Load["PF"];
//	if(PendingFrames > (m_uCongestionWindow >> 1)) // if there are more pending frames than half of the current window, shrink the window
//		m_uCongestionWindow = m_pStats->PendingFrames;
//}

void CRouteStats::AddSample(uint32 uSampleRTT, size_t uFrameSize)
{
	if(uFrameSize > 1024) // Calculate RTT per 1 KB for large frames
		uSampleRTT /= (uint32)(uFrameSize / 1024);

	m_pStats->AddSample(uSampleRTT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//

SRoutingStat::SRoutingStat()
{
	uLastUpdate = GetCurTick();
	// stats for the last 3 seconds
	RecentlyDelivered.Init(30);
	RecentlyLost.Init(30);
}

CRelayStats::CRelayStats(CObject* pParent)
: CRouteStats(new SRelayStats()
, pParent->GetParent<CKademlia>()->Cfg()->GetInt("MinRelayTimeout")
, pParent->GetParent<CKademlia>()->Cfg()->GetInt("MaxRelayTimeout")
, pParent->GetParent<CKademlia>()->Cfg()->GetInt("MaxWindowSize")
, pParent) 
{
}

void CRelayStats::FrameDelivered(const CVariant& RID, uint32 uSampleRTT, size_t uFrameSize)
{
	if(uFrameSize > 1024) // Calculate RTT per 1 KB for large frames
		uSampleRTT /= (uint32)(uFrameSize / 1024);

	((SRelayStats*)m_pStats.Val())->DeliveredFrames++;

	SRoutingStat &Stat = m_RoutingMap[RID];
	Stat.uLastUpdate = GetCurTick();
	Stat.AddSample(uSampleRTT);
	Stat.RelayedFrames ++;
	Stat.RecentlyDelivered.Update(1);
}

void CRelayStats::FrameLost(const CVariant& RID)
{
	((SRelayStats*)m_pStats.Val())->LostFrames++;

	SRoutingStat &Stat = m_RoutingMap[RID];
	Stat.uLastUpdate = GetCurTick();
	Stat.DroppedFrames ++;
	Stat.RecentlyLost.Update(1);
}

void CRelayStats::CleanupRouting()
{
	uint64 CurTick = GetCurTick();
	uint64 uRouteTimeout = SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("RouteTimeout"));

	for(TRoutingMap::iterator I = m_RoutingMap.begin(); I != m_RoutingMap.end(); )
	{
		if(CurTick - I->second.uLastUpdate > uRouteTimeout)
			I = m_RoutingMap.erase(I);
		else
		{
			I->second.RecentlyDelivered.Add(0);
			I->second.RecentlyLost.Add(0);
			I++;
		}
	}
}

SRoutingStat* CRelayStats::GetStat(const CVariant& RID)
{
	TRoutingMap::iterator I = m_RoutingMap.find(RID);
	if(I == m_RoutingMap.end())
		return NULL;
	return &I->second;
}