#pragma once

class CBandwidthLimit;
class CBandwidthManager;
class CSmartSocket;

#define MSS 1402 // exact MTU for UTP IPV4
//#define MSS 1232 // exact MTU for UTP IPV6 (Toredo)

class CBandwidthLimiter
{
public:
	CBandwidthLimiter(CSmartSocket* pSmartSocket);
	~CBandwidthLimiter();
	
	enum EChannel
	{
		eUpChannel = 0,
		eDownChannel = 1,
		eCount
	};

	//virtual void		BandwidthAssigned(UINT Channel) {}
	virtual void		AssignBandwidth(UINT Channel, int iAmount);

	virtual void		AddLimit(CBandwidthLimit* pUpLimit, UINT Channel);
	virtual void		RemoveLimit(CBandwidthLimit* pLimit, UINT Channel);

	void				RequestBandwidth(UINT Channel, int iAmount);
	void				CountBandwidth(UINT Channel, int iAmount/*, bool bPayload*/);

protected:
	int					m_Quota[eCount];
	CBandwidthManager*  m_pManager[eCount];
	vector<CBandwidthLimit*>	m_Limits[eCount];
};
