#include "GlobalHeader.h"
#include "BandwidthLimit.h"
#include "BandwidthLimiter.h"
#include "BandwidthManager.h"
#include "../SmartSocket.h"
#include "../SocketSession.h"

CBandwidthLimiter::CBandwidthLimiter(CSmartSocket* pSmartSocket)
{
	ASSERT(pSmartSocket);

	m_Quota[eUpChannel] = 0;
	m_Quota[eDownChannel] = 0;

	m_pManager[eUpChannel] = pSmartSocket->GetUpManager();
	m_pManager[eDownChannel] = pSmartSocket->GetDownManager();

	// Install default global bandwidth limit
	AddLimit(pSmartSocket->GetUpLimit(), eUpChannel);
	AddLimit(pSmartSocket->GetDownLimit(), eDownChannel);
}

CBandwidthLimiter::~CBandwidthLimiter()
{
	for(int i=0; i < eCount; i++)
	{
		m_pManager[i]->ReturnBandwidth(this);

		for(vector<CBandwidthLimit*>::iterator I = m_Limits[i].begin(); I != m_Limits[i].end(); I = m_Limits[i].erase(I))
			(*I)->Remove(this, i);
	}
}

void CBandwidthLimiter::AssignBandwidth(UINT Channel, int iAmount)
{
	ASSERT(iAmount >= 0);
	m_Quota[Channel] += iAmount;
}

void CBandwidthLimiter::RequestBandwidth(UINT Channel, int iAmount)
{
	int Quota = m_Quota[Channel];
	if(Quota < iAmount)
		m_pManager[Channel]->RequestBandwidth(m_Limits[Channel], this, iAmount - Quota);
}

void CBandwidthLimiter::CountBandwidth(UINT Channel, int iAmount/*, bool bPayload*/)
{
	m_Quota[Channel] -= iAmount;

	for(size_t i=0; i < m_Limits[Channel].size(); i++)
		m_Limits[Channel].at(i)->CountBytes(iAmount/*, bPayload*/);

	int Quota = m_Quota[Channel];
	if(Quota < 0) // if we are in det request imminetly
		m_pManager[Channel]->RequestBandwidth(m_Limits[Channel], this, -Quota);
}

void CBandwidthLimiter::AddLimit(CBandwidthLimit* pLimit, UINT Channel)
{
	pLimit->Add(this, Channel);
	m_Limits[Channel].push_back(pLimit);
	m_pManager[Channel]->UpdateLimits(m_Limits[Channel], this);
}

void CBandwidthLimiter::RemoveLimit(CBandwidthLimit* pLimit, UINT Channel)
{
	vector<CBandwidthLimit*>::iterator I = find(m_Limits[Channel].begin(), m_Limits[Channel].end(), pLimit);
	if(I != m_Limits[Channel].end())
	{
		pLimit->Remove(this, Channel);
		m_Limits[Channel].erase(I);
		m_pManager[Channel]->UpdateLimits(m_Limits[Channel], this);
	}
}