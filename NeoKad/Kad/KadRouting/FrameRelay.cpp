#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "KadRelay.h"
#include "FrameRelay.h"
#include "../Kademlia.h"
#include "../KadConfig.h"
#include "../KadNode.h"
#include "../KadHandler.h"
#include "../../../Framework/Maths.h"

IMPLEMENT_OBJECT(CFrameRelay, CObject)

CFrameRelay::CFrameRelay(CObject* pParent)
 :CObject(pParent)
{
	m_AllowDelay = false;
}

void CFrameRelay::Process(UINT Tick)
{
	CKadHandler* pHandler = GetParent<CKademlia>()->Handler();

	uint64 CurTick = GetCurTick();
	size_t SizeOnRoute = 0;
	for(TFrameList::iterator I = m_Frames.begin(); I != m_Frames.end();)
	{
		SFrame* pFrame = *I;

		uint64 Age = CurTick - pFrame->ReciveTime;
		uint64 AckTimeOut = 0;
		if(Age < pFrame->TTL * 2) // the sender node waits twice as long for the ack
			AckTimeOut = pFrame->TTL * 2 - Age;

		if(pFrame->SendTime) // if the frame was sent
		{
			ASSERT(pFrame->Frame.GetSize() != 0);
			SizeOnRoute += pFrame->Frame.GetSize();

			CRelayStats* pRelay = NULL;
			TRelayMap::iterator J = m_Nodes.find(pFrame->To);
			if(J != m_Nodes.end())
				pRelay = J->second;

			uint64 CurTimeOut = 0;
			if(pRelay)
			{
				CurTimeOut = pRelay->GetTimeOut() * 2	// The frame is allowed to live twice as long as the normal frame needs for its way
						* ((SizeOnRoute / 1024) + 1);	// Note: the average timeout is calculated per KB soe we must multiply here for each started PB
				if(CurTimeOut > AckTimeOut)
					CurTimeOut = AckTimeOut;
			}
			if(pFrame->RelayTime || CurTick - pFrame->SendTime < CurTimeOut) // if teh frame was relayed or the relay hasnt timed out
			{
				I++;
				continue; // we must wait for this client to timeout, we can not drop the frame befoure
			}
			else
			{
				pFrame->SendTime = 0;
				pFrame->RelayTime = 0;
				if(pFrame->To.pNode)
				{
					pFrame->Failed[pFrame->To.pNode->GetID()] = "TimeOut";
					pFrame->To.Clear();
				}

				m_Stats.PendingFrames--;
				if(pRelay)
					pRelay->FrameDropped();
			}
		}

		if(Age >= pFrame->TTL) // this frame timed out, and we are not resending
		{
			if(pFrame->SendTime == 0 || AckTimeOut == 0) // if the frame wasnt sent or the source isnt waiting for the ack anymore
			{
				if(pFrame->RelayTime == 0)
					m_Stats.DroppedFrames++; 
				I = m_Frames.erase(I);
			}
			else
				I++;
			continue; // this frame is faiding
		}

		ASSERT(pFrame->SendTime == 0);

		TRelayMap::iterator J;
		int iOk = 0;
		if(CKadNode* pNode = SelectNode(pFrame->Frame["RID"]))
		{
			J = m_Nodes.find(SKadNode(pNode));
			if(J != m_Nodes.end())
			{
				if(pFrame->Failed.find(J->first.pNode->GetID()) != pFrame->Failed.end())
					iOk = 1;
			}
		}	

		if(iOk == 0 && !m_Nodes.empty()) // If the smart sellection failed or the selected node failed, use this old random method as fallback
		{
			int iRand = rand() % m_Nodes.size();
			for(J = m_Nodes.begin(); J != m_Nodes.end(); J++)
			{
				if(pFrame->Failed.find(J->first.pNode->GetID()) == pFrame->Failed.end())
				{
					if(iOk++ >= iRand)
					{
						// Check if this node has some free space of if the counguestion window is already full
						if(J->second->IsWindowFull()) 
							continue;
						break;
					}
				}
			}
		}

		if(iOk == 0) // cancel all packets that can not be delivered and nack to the upper level
		{
			if(!GetParent<CKadRelay>()->SearchingRelays())
			{
				if(pFrame->From.pNode)
					GetParent<CKademlia>()->Handler()->SendRelayRes(pFrame->From.pNode, pFrame->From.pChannel, pFrame->Frame, "NoNodes"); // send an nack up the chanin

				m_Stats.DroppedFrames++; 
				I = m_Frames.erase(I);
				continue;
			}
		}
		else if(J != m_Nodes.end()) // did we head guud luck
		{
			ASSERT(Age < pFrame->TTL); // we are not alowed to relay packets that are timed out
			uint64 TTL = pFrame->TTL - Age;
			if(pHandler->SendRelayReq(J->first.pNode, J->first.pChannel, pFrame->Frame, TTL, GetParent<CKadRelay>())) // returns false when the channel is not verifyed
			{
				pFrame->SendTime = CurTick;
				pFrame->To = J->first;

				m_Stats.PendingFrames++;
				J->second->FramePending();
			}
		}
		//else // we have bad luck better luck next round

		I++;
	}

	if((Tick & E10PerSec) == 0)
		return;

	for(TRelayMap::iterator I = m_Nodes.begin(); I != m_Nodes.end();)
	{
		const SKadNode &Node = I->first;
		CRelayStats* pRelay = I->second;
		pRelay->UpdateControl();
		pRelay->CleanupRouting();
		if(Node.pChannel->IsConnected())
			I++;
		else
			I = m_Nodes.erase(I);
	}
}

bool CFrameRelay::Relay(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel)
{
	if(!m_AllowDelay && m_Nodes.empty()) // if we dont have nodes routing is not available
	{
		if(pFromNode)
			GetParent<CKademlia>()->Handler()->SendRelayRes(pFromNode, pChannel, Frame, "NoNodes");
		return false;
	}

	// do not relay teh same frame twice
	// see AcceptDownLink, this shouldnt happen
	/*for(list<SFrame>::iterator I = m_Frames.begin(); I != m_Frames.end();)
	{
		if(Frame["FID"] == I->Frame["FID"] && Frame["RID"] == I->Frame["RID"] && Frame["EID"] == I->Frame["EID"])
		{
			if(pFromNode)
				GetParent<CKademlia>()->Handler()->SendRelayRes(pFromNode, Frame, "Recursion");
			return false;
		}
	}*/

	if(pFromNode)
	{
		//CVariant Load;
		//Load["PF"] = m_Frames.size();
		GetParent<CKademlia>()->Handler()->SendRelayRes(pFromNode, pChannel, Frame/*, "", Load*/); // send ack first so the Load Info won't count the just recived frame
	}

	SFrame* pFrame = new SFrame(Frame, TTL);
	pFrame->From.pNode = pFromNode;
	pFrame->From.pChannel = pChannel;
	m_Frames.push_back(CScoped<SFrame>()); m_Frames.back() = pFrame;
	return true;
}

bool CFrameRelay::Ack(const CVariant& Ack, bool bDelivery)
{
	for(TFrameList::iterator I = m_Frames.begin(); I != m_Frames.end(); I++)
	{
		SFrame* pFrame = *I;
		if(Ack["FID"] == pFrame->Frame["FID"] && Ack["RID"] == pFrame->Frame["RID"] && Ack["EID"] == pFrame->Frame["EID"])
		{
			bool bErr = Ack.Has("ERR");

			CRelayStats* pRelay = NULL;
			TRelayMap::iterator J = m_Nodes.find(pFrame->To);
			if(J != m_Nodes.end())
				pRelay = J->second;

			uint64 CurTick = GetCurTick();
			if(pFrame->SendTime != 0 && pFrame->RelayTime == 0) // it does not matehr if this is the Res or Ret
			{
				pFrame->RelayTime = CurTick;

				m_Stats.PendingFrames--;
				if(!bErr)
					m_Stats.RelayedFrames++;
				if(pRelay)
				{
					pRelay->FrameRelayed(); // if we got an Ack or a Nack the frame counts as relayes
					pRelay->AddSample(CurTick - pFrame->SendTime, pFrame->Frame.GetSize());
					//if(!bDelivery && Ack.Has("LOAD"))
					//	pRelay->UpdateLoad(Ack["LOAD"]);
				}
			}

			if(bDelivery)
			{
				if(pFrame->From.pNode)
					GetParent<CKademlia>()->Handler()->SendRelayRet(pFrame->From.pNode, pFrame->From.pChannel, Ack);

				if(bErr)
					m_Stats.LostFrames++;
				else
					m_Stats.DeliveredFrames++;
				if(pRelay)
					pRelay->FrameDelivered(pFrame->Frame["RID"], CurTick - pFrame->SendTime, pFrame->Frame.GetSize());

				m_Frames.erase(I);
			}
			else if(bErr) // issue resend, the frame still counts as relayed
			{
				if(pRelay)
					pRelay->FrameLost(pFrame->Frame["RID"]);

				pFrame->SendTime = 0;
				pFrame->RelayTime = 0;
				if(pFrame->To.pNode)
				{
					pFrame->Failed[pFrame->To.pNode->GetID()] = Ack.At("ERR").To<string>();
					pFrame->To.Clear();
				}

				if(Ack["ERR"] == "UnknownRoute")
					Remove(pFrame->To);
			}
			return true;
		}
	}
	return false;
}

bool CFrameRelay::Has(CKadNode* pNode)
{
	return m_Nodes.find(SKadNode(pNode)) != m_Nodes.end();
}

void CFrameRelay::Add(CKadNode* pNode, CComChannel* pChannel)
{
	TRelayMap::iterator I = m_Nodes.find(SKadNode(pNode));
	if(I != m_Nodes.end())
	{
		ASSERT(0);
		return;
	}

	// Should not happen but might
	for(TRelayMap::iterator I = m_Nodes.begin(); I != m_Nodes.end(); I++)
	{
		if(I->first.pNode->GetID() == pNode->GetID())
		{
			for(TFrameList::iterator J = m_Frames.begin(); J != m_Frames.end(); J++)
			{
				if((*J)->To == I->first)
				{
					(*J)->To.pNode = pNode;
					(*J)->To.pChannel = pChannel;
				}
			}
			m_Nodes[SKadNode(pNode, pChannel)] = I->second;
			m_Nodes.erase(I);
			return;
		}
	}

	m_Nodes[SKadNode(pNode, pChannel)] = new CRelayStats(this);
}

void CFrameRelay::Remove(const SKadNode& Node)
{
	TRelayMap::iterator I = m_Nodes.find(Node);
	if(I != m_Nodes.end())
		m_Nodes.erase(I);
}

struct SRoutingEntry
{
	SRoutingEntry()
	{
		pNode = NULL;
		TimeOut = 0;
		LostRate = 0;
	}
	CKadNode* pNode;
	uint32 TimeOut;
	int LostRate;
};

CKadNode* CFrameRelay::SelectNode(const CVariant& RID)
{
	SRoutingPlan &RoutingPlan = m_RoutingCache[RID];

	uint64 CurTick = GetCurTick();
	if(RoutingPlan.Routes.empty() || CurTick - RoutingPlan.uLastUpdate > SEC2MS(3)) // k-ToDo: customize
	{
		RoutingPlan.uLastUpdate = CurTick;

		uint32 BestTimeOut = UINT_MAX;

		size_t i = 0;
		vector<SRoutingEntry> Entrys;
		Entrys.resize(m_Nodes.size());
		for(TRelayMap::iterator J = m_Nodes.begin(); J != m_Nodes.end(); J++)
		{
			SRoutingEntry &RoutingEntry = Entrys[i++];
			if(SRoutingStat* pStat = J->second->GetStat(RID))
			{
				RoutingEntry.LostRate = pStat->GetLostRate();
				RoutingEntry.TimeOut = pStat->GetTimeOut();
			}

			if(RoutingEntry.TimeOut == 0)
				RoutingEntry.TimeOut = J->second->GetStats().TimeOut.GetAverage();

			if(RoutingEntry.TimeOut < BestTimeOut)
				BestTimeOut = RoutingEntry.TimeOut;
		}

		RoutingPlan.Routes.clear();

		int TotalScore = 0;
		for(i=0; i < Entrys.size(); i++)
		{
			SRoutingEntry &RoutingEntry = Entrys[i];
			int Score;
			if(RoutingEntry.TimeOut == 0)
				Score = 50;
			else
				Score = 100 * BestTimeOut / RoutingEntry.TimeOut; // = 0 - 100
			Score -= RoutingEntry.LostRate;
			if(Score < 10)
				Score = 10;
			TotalScore += Score;
			RoutingPlan.Routes[TotalScore] = RoutingEntry.pNode;
		}
	}

	if(RoutingPlan.Routes.empty())
		return NULL;

	int MaxVal = (--RoutingPlan.Routes.end())->first;
	int iRand = rand()%MaxVal;
	return RoutingPlan.Routes.upper_bound(iRand)->second;
}