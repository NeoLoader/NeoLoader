#pragma once

struct SSummWindow
{
	SSummWindow()
	{
		TotalSumm = 0;
		Position = 0;
	}

	void Init(int Size, uint32 Value = 0)
	{
		if(Value)
			TotalSumm = Value * Size;
		MovingWindow.resize(Size, Value);
	}

	void Update(uint32 Value)
	{
		MovingWindow[Position] += Value;
		TotalSumm += Value;
	}

	void Add(uint32 Value)
	{
		if(++Position >= MovingWindow.size())
			Position = 0;
		TotalSumm -= MovingWindow[Position];
		MovingWindow[Position] = Value;
		TotalSumm += MovingWindow[Position];
	}

	uint64			GetAverage() const	{return MovingWindow.empty() ? 0 : (TotalSumm / MovingWindow.size());}
	uint64			GetSumm() const		{return TotalSumm;}

	uint64			TotalSumm;
	vector<uint32>	MovingWindow;
	size_t			Position;
};

struct SRouteStats
{
	SRouteStats();

	void			AddSample(uint32 uSampleRTT);

	uint32			GetEstimatedRTT() const			{return uEstimatedRTT >> 3;}
	uint32			GetRTTDeviation() const			{return uRTTDeviation >> 3;}

	uint32			uEstimatedRTT; // connection latency time (from pasket send to recive ack)
	uint32			uRTTDeviation; // diviation ouf our rtt based timeout vlaues

	SSummWindow		TimeOut;

	// lets make this signed in case of a -- to much its easyer to spot and debug
	uint32			PendingFrames;
	uint32			RelayedFrames;
	uint32			DroppedFrames;
};

class CRouteStats: public CObject
{
public:
	DECLARE_OBJECT(CRouteStats)

	CRouteStats(SRouteStats* pStats, uint32 uMinTimeOut, uint32 uMaxTimeOut, uint32 uCongestionLimit, CObject* pParent = NULL);

	void			FramePending();
	void			FrameDropped();
	void			FrameRelayed();

	void			AddSample(uint32 uSampleRTT, size_t uFrameSize = 1024);

	uint32			GetTimeOut() const				{return m_TimeOutAverage;}
	uint32			GetWindowSize() const			{return m_uCongestionWindow;}

	//void			UpdateLoad(const CVariant& Load);

	void			UpdateControl();
	bool			IsWindowFull();

protected:
	CScoped<SRouteStats> m_pStats;

	uint32			m_uMinTimeOut;
	uint32			m_uMaxTimeOut;

	uint32			m_TimeOutAverage;

	uint32			m_uCongestionWindow; 
	uint32			m_uCongestionThreshold;
	uint16			m_uIncrementalCounter;
	uint32			m_uCongestionLimit;
};


struct SRelayStats: SRouteStats
{
	SRelayStats()
	{
		DeliveredFrames = 0;
		LostFrames = 0;
	}

	int				DeliveredFrames;
	int				LostFrames;
};

struct SRoutingStat: SRouteStats
{
	SRoutingStat();

	uint64			uLastUpdate;
	SSummWindow		RecentlyDelivered;
	SSummWindow		RecentlyLost;

	uint32			GetTimeOut()
	{
		return TimeOut.GetAverage();
	}
	uint32			GetLostRate()
	{
		uint64 Total = RecentlyDelivered.GetSumm() + RecentlyLost.GetSumm();
		if(!Total)
			return 0;
		return 100 * RecentlyLost.GetSumm() / Total;
	}
};

class CRelayStats: public CRouteStats
{
public:
	CRelayStats(CObject* pParent = NULL);

	void			FrameDelivered(const CVariant& RID, uint32 uSampleRTT, size_t uFrameSize = 1024);
	void			FrameLost(const CVariant& RID);

	const SRelayStats& GetStats() const				{return *((SRelayStats*)m_pStats.Val());}

	void			CleanupRouting();

	typedef map<CVariant, SRoutingStat> TRoutingMap;
	const TRoutingMap& GetRouting() const				{return m_RoutingMap;}
	SRoutingStat*	GetStat(const CVariant& RID);

protected:
	TRoutingMap		m_RoutingMap;
};