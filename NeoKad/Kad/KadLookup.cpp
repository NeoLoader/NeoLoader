#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadLookup.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "KadNode.h"
#include "KadHandler.h"
#include "RoutingRoot.h"

IMPLEMENT_OBJECT(CKadLookup, CObject)

CKadLookup::CKadLookup(const CUInt128& ID, CObject* pParent)
 : CObject(pParent)
{
	m_ID.SetValue(ID);

	m_TimeOut = SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt64("LookupTimeout"));
	m_HopLimit = 1; // If this is 0 we needer allowed to look for nodes not to send requests
	m_JumpCount = 0;

	m_ClosestNodes = 0;
	m_LookupState = eNoLookup;

	m_StopTime = -1;
	m_StartTime = 0;
	m_Status = ePending;

	m_LookupHistory = new CLookupHistory(this);
	m_Trace = false;

	m_UpLimit = new CBandwidthLimit(this);
	m_DownLimit = new CBandwidthLimit(this);
}

CKadLookup::~CKadLookup()
{
	ASSERT(m_Nodes.empty());
}

void CKadLookup::InitLookupID()
{
	CAbstractKey LookupID(KEY_64BIT, true);
	m_LookupID = CVariant(LookupID.GetKey(), LookupID.GetSize());
}

void CKadLookup::Start()
{
	InitOperation();

	ASSERT(m_LookupID.IsValid());
	ASSERT(m_StartTime == 0);
	m_StartTime = GetCurTick();
	m_Status = eStarted;
}

void CKadLookup::InitOperation()
{
	// We dont look for closer nodes if we are using recursive mode
	if(m_HopLimit == 1)
	{
		ASSERT(m_JumpCount == 0);
		m_LookupState = eLookupActive;
	}
}

bool CKadLookup::ReadyToStop()
{
	bool bTimedOut = HasTimedOut();
	if(!bTimedOut && m_LookupState == eLookupActive)
		return false;
	return true;
}

void CKadLookup::PrepareStop()
{
	FlushCaches();

	m_StopTime = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("RequestTimeOut")); 
}

bool CKadLookup::CachesEmpty()
{
	for(TNodeStatusMap::iterator I = m_Nodes.begin(); I != m_Nodes.end(); I++)
	{
		if(I->first.pChannel && I->first.pChannel->IsBussy())
			return false;
	}
	return true;
}

void CKadLookup::Stop()
{
	CleanUpNodes();

	m_StopTime = GetCurTick();
	m_Status = eStopped;
}

void CKadLookup::CleanUpNodes()
{
	for(TNodeStatusMap::iterator I = m_Nodes.begin(); I != m_Nodes.end(); I++)
	{
		if(I->first.pChannel)
			I->first.pChannel->Close();
		delete I->second;
	}
	m_Nodes.clear();
	m_ClosestNodes = 0;
}

int	CKadLookup::GetNeededCount()
{
	// Note: this is the amount of nodes to be asked for closer nodes
	return GetParent<CKademlia>()->Cfg()->GetInt("BrancheCount");
}

CKadLookup::SNodeStatus* CKadLookup::GetStatus(CKadNode* pNode)
{
	TNodeStatusMap::iterator I = m_Nodes.find(SKadNode(pNode));
	if(I == m_Nodes.end())
		return NULL;
	return I->second;
}

void CKadLookup::Process(UINT Tick)
{
	if((Tick & E10PerSec) == 0)
		return;

	uint64 uNow = GetCurTick();

	// check out all open channels
	int ActiveNodes = 0;
	for(TNodeStatusMap::iterator I = m_Nodes.begin(); I != m_Nodes.end(); I++)
	{
		if(!I->first.pChannel)
			continue;

		// Note: a sub node is not allowed to disconnect on its during a lookup, if it does so its considdered a failure
		if(I->first.pChannel->IsDisconnected())
		{
			I->first.pChannel = NULL;

			I->first.pNode->IncrFailed();

			if(I->second->uTimeOut != 0)
				NodeStalling(I->first.pNode);

			ChannelClosed(I->first.pNode);

			if(I->second->LookupState == eLookupClosest)
			{
				ASSERT(m_ClosestNodes > 0);
				m_ClosestNodes--;
			}
		}
		else 
		{
			if(I->second->uTimeOut < uNow)
			{
				if(I->second->uTimeOut != 0)
				{
					NodeStalling(I->first.pNode);
					I->second->uTimeOut = 0; // mark this node as stalled
				}
			}
			else if(I->second->LookupState == eLookupActive)
				ActiveNodes++; // active node requests that are not stalled
		}
	}

	if(m_LookupState != eLookupActive)
		return;

	int AskForCloser = GetNeededCount();
	AskForCloser -= Min(AskForCloser, ActiveNodes + m_ClosestNodes);
	ASSERT(AskForCloser >= 0);

	CRoutingZone::SIterator Iter;
	for(CKadNode* pNode; (pNode = GetParent<CKademlia>()->Root()->GetClosestNode(Iter, m_ID)) && AskForCloser > 0;)
	{
		if(WasNodeUsed(pNode))
			continue;

		if(RequestNodes(pNode, CKadLookup::GetChannel(pNode)))
		{
			AskForCloser --;
			ActiveNodes++;
		}
	}

	// are we done, doer cause we got what we wanted or cause there isnt any more
	if(ActiveNodes == 0)
		m_LookupState = eLookupFinished;
}

CComChannel* CKadLookup::GetChannel(CKadNode* pNode)
{
	TNodeStatusMap::iterator I = m_Nodes.find(SKadNode(pNode));
	if(I == m_Nodes.end())
		I = m_Nodes.insert(TNodeStatusMap::value_type(SKadNode(pNode, NULL), NewNodeStatus())).first;

	if(!I->first.pChannel)
	{
		I->first.pChannel = GetParent<CKademlia>()->Handler()->PrepareChannel(pNode);
		if(!I->first.pChannel)
			return NULL;
	
		SKadData* pData = I->first.pChannel->GetData<SKadData>();

		pData->pLookup = CPointer<CKadLookup>(this, true); // weak pointer
		I->first.pChannel->AddUpLimit(GetUpLimit());
		I->first.pChannel->AddDownLimit(GetDownLimit());
	}
	return I->first.pChannel;
}

void CKadLookup::SetTimeOut(int TimeOut, bool bFromNow)
{
	m_TimeOut = Min(TimeOut, SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt64("MaxLookupTimeout")));
	if(bFromNow)
		m_TimeOut += GetDuration();
}

void CKadLookup::SetJumpCount(int JumpCount)
{
	m_JumpCount = Min(JumpCount, GetParent<CKademlia>()->Cfg()->GetInt64("MaxJumpCount"));
}
void CKadLookup::SetHopLimit(int HopLimit)
{
	m_HopLimit = Min(HopLimit, GetParent<CKademlia>()->Cfg()->GetInt64("MaxHopLimit"));
}

bool CKadLookup::HasTimedOut()
{
	if(m_TimeOut == -1)
		return false; // no timeout for this lookup
	return GetDuration() > m_TimeOut;
}

uint64 CKadLookup::GetDuration()
{
	if(m_StartTime)
	{
		if(m_StopTime != -1)
			return m_StopTime - m_StartTime;
		return GetCurTick() - m_StartTime;
	}
	return 0;
}

uint64 CKadLookup::StoppedSince()
{
	if(m_Status == eStopped)
		return GetCurTick() - m_StopTime;
	return 0;
}

uint64 CKadLookup::GetTimeRemaining()
{
	uint64 uUpTime = GetDuration();
	if(m_TimeOut != -1 && m_TimeOut > uUpTime)
		return m_TimeOut - uUpTime;
	return 0;
}

void CKadLookup::AddNodes(const NodeMap& Nodes, const CUInt128& ByID)
{
	if(m_LookupHistory)
		m_LookupHistory->AddNodes(Nodes, ByID);
}

bool CKadLookup::RequestNodes(CKadNode* pNode, CComChannel* pChannel, const CUInt128& ByID)
{
	SNodeStatus* pStatus = GetStatus(pNode);
	if(!pStatus || !pChannel || pStatus->uTimeOut != -1) // error or node already in use
		return false;

	if(GetParent<CKademlia>()->Handler()->SendNodeReq(pNode, pChannel, m_ID) == 0)
		return false;

	pStatus->LookupState = eLookupActive;
	pStatus->uTimeOut = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("RequestTimeOut"));

	if(m_LookupHistory)
		m_LookupHistory->ExpectNodes(pNode->GetID(), ByID);
	return true;
}

bool CKadLookup::WasNodeUsed(CKadNode* pNode)
{
	if(SNodeStatus* pStatus = GetStatus(pNode))
		return pStatus->LookupState != eNoLookup;
	return false;
}

bool CKadLookup::OnCloserNodes(CKadNode* pNode, CComChannel* pChannel, int CloserCount)
{
	if(m_LookupHistory)
		m_LookupHistory->RegisterNodes(pNode->GetID(), CloserCount);

	SNodeStatus* pStatus = GetStatus(pNode);
	if(!pStatus)
		return false;
	
	pStatus->uTimeOut = -1;

	if(CloserCount > 0)
		pStatus->LookupState = eLookupFinished;
	else if (pStatus->LookupState != eLookupClosest)
	{
		pStatus->LookupState = eLookupClosest;
		m_ClosestNodes++;
	}

	return CloserCount == 0; // return true if this is teh closest node found
}

void CKadLookup::RecivedTraceResults(const CVariant& Trace)
{
	if(m_LookupHistory)
		m_LookupHistory->RecivedTraceResults(Trace);
}

