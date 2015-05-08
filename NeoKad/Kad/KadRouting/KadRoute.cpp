#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "../Kademlia.h"
#include "KadRoute.h"
#include "../KadConfig.h"
#include "../KadNode.h"
#include "../KadHandler.h"
#include "RouteSession.h"
#include "FrameRelay.h"
#include "../LookupManager.h"

IMPLEMENT_OBJECT(CKadRoute, CKadRelay)

CKadRoute::CKadRoute(const CUInt128& ID, CPrivateKey* pEntityKey, CObject* pParent)
 : CKadRelay(ID, pParent) 
{
	Refresh(); // make this type temporary

	if(pEntityKey)
		m_pPrivateKey = pEntityKey;
	else
	{
		m_pPrivateKey = new CPrivateKey(CAbstractKey::eECP);
		//m_pPrivateKey->GenerateKey("secp128r1"); // Note: this is equivalent to a 64 bit symetric key, so not very secure
		m_pPrivateKey->GenerateKey("brainpoolP160r1"); // dont trust NIST (NSA) curves, use brainpool
	}

	m_pPublicKey = m_pPrivateKey->PublicKey();

	m_EntityID = CVariant((byte*)NULL, KEY_64BIT);
	CKadID::MakeID(m_pPublicKey, m_EntityID.GetData(), m_EntityID.GetSize());

	m_UpLink = new CFrameRelay(this);
	// m_DownLink == NULL;
}

void CKadRoute::Process(UINT Tick)
{
	for(SessionMap::iterator I = m_SessionMap.begin(); I != m_SessionMap.end(); )
	{
		if(!I->second->Process(Tick))
		{
			I->second->Destroy();
			I = m_SessionMap.erase(I);
		}
		else
			I++;
	}

	CKadRelay::Process(Tick);

	while(m_Acks.size() > 0)
	{
		SAck* pAck = m_Acks.front();
		if(pAck->From.pNode) //if we sent ourself a packet we wont crash
			GetParent<CKademlia>()->Handler()->SendRelayRet(pAck->From.pNode, pAck->From.pChannel, pAck->Ack); 
		m_Acks.pop_front();
	}
}

CRouteSession* CKadRoute::OpenSession(const CVariant& EntityID, const CUInt128& TargetID)
{
	CAbstractKey SessionID(KEY_64BIT, true);

	CRouteSession* pSession = MkRouteSession(EntityID, TargetID, this);
	pSession->InitSession(CVariant(SessionID.GetKey(), SessionID.GetSize()));
	m_SessionMap.insert(SessionMap::value_type(EntityID, pSession));
	return pSession;
}

bool CKadRoute::CloseSession(const CVariant& EntityID, const CVariant& SessionID)
{
	SessionMap::iterator I = m_SessionMap.find(EntityID);
	for(;I != m_SessionMap.end() && I->second->GetSessionID() != SessionID; I++);
	if(I != m_SessionMap.end())
	{
		I->second->CloseSession();
		return true;
	}
	return false;
}

bool CKadRoute::AckUp(const CVariant& Ack, bool bDelivery)
{
	if(bDelivery)
	{
		if(IsTraced())
			TraceFrameAck(Ack);

		if(Ack.Has("SID")) 
		{
			// Find the session with the right ReciverID as well as the matching Session ID
			const CVariant& EntityID = Ack["RID"];
			const CVariant& SessionID = Ack["SID"];
			SessionMap::iterator I = m_SessionMap.find(EntityID);
			for(;I != m_SessionMap.end() && I->first == EntityID && I->second->GetSessionID() != SessionID; I++);

			if(I != m_SessionMap.end())
			{
				if(!I->second->AckFrame(Ack))
					return false; // forged !!!
			}
		}
		//else // it was a raw frame
	}

	return CKadRelay::AckUp(Ack, bDelivery);
}

bool CKadRoute::RelayUp(const CVariant& Frame, uint64 TTL)
{
	if(IsTraced())
		TraceFrameSend(Frame);

	// handle the case when we are the closest node ourselves
	if(CPointer<CKadRelay> pDownRelay = GetParent<CKademlia>()->Manager()->GetRelay(Frame["RID"])) // relay down to the source
	{
		// K-ToDo-Now: send trace down to source
		if(pDownRelay->RelayDown(Frame, TTL, NULL, NULL))
			return true;
	}

	return CKadRelay::RelayUp(Frame, TTL, NULL, NULL);
}

bool CKadRoute::RelayDown(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel)
{
	if(pFromNode)
	{
		//CVariant Load;
		//Load["PF"] = ;
		GetParent<CKademlia>()->Handler()->SendRelayRes(pFromNode, pChannel, Frame/*, "", Load*/);
	}

	if(IsTraced())
		TraceFrameRecv(Frame);

	if(Frame.Has("RAW"))
	{
		ProcessRawFrame(Frame, pFromNode, pChannel);
		return true;
	}

	if(Frame.Has("SID"))
	{
		const CVariant& EntityID = Frame["EID"]; // sender entity
		const CVariant& SessionID = Frame["SID"]; // session ID
		// Find the session with the right ReciverID as well as the matching Session ID
		SessionMap::iterator I = m_SessionMap.find(EntityID);
		for(;I != m_SessionMap.end() && I->first == EntityID && I->second->GetSessionID() != SessionID; I++);

		if(I == m_SessionMap.end())
		{
			I = m_SessionMap.insert(SessionMap::value_type(EntityID, MkRouteSession(EntityID, Frame.Has("TID") ? Frame["TID"] : CVariant(GetID()), this)));
			I->second->InitSession(SessionID, false);
		}

		I->second->ProcessFrame(Frame, pFromNode, pChannel);
	}
	return true;
}

void CKadRoute::QueueAck(const CVariant& Ack, CKadNode* pFromNode, CComChannel* pChannel)
{
	// Note: we queue this to not giv hints through the timing that we are the actual target
	SAck* pAck = new SAck();
	pAck->Ack = Ack;
	pAck->From = SKadNode(pFromNode, pChannel);
	m_Acks.push_back(CScoped<SAck>()); m_Acks.back() = pAck;
}

void CKadRoute::SendRawFrame(const CVariant& EntityID, const CUInt128& TargetID, const CVariant& Data)
{
	uint64 TTL = GetParent<CKademlia>()->Cfg()->GetInt("MaxFrameTTL");  //K-ToDo-Now: randomise TTL to make sure we are not the obviuse origin!!!!!!!

	CVariant Frame;
	Frame["FID"] = GetRand64();
	Frame["EID"] = m_EntityID;
	Frame["RID"] = EntityID;
	// Add target ID fiels if this is an bridged route
	if(TargetID != GetID()) // K-ToDo-Now: send always for indistinguishability?
		Frame["TID"] = TargetID;

	Frame["RAW"] = Data;

	//Frame.Sign(m_pPrivateKey);

	RelayUp(Frame, TTL);
}

void CKadRoute::ProcessRawFrame(const CVariant& Frame, CKadNode* pFromNode, CComChannel* pChannel)
{
	CVariant Ack;
	if(Frame.Has("TID"))
		Ack["TID"] = Frame["TID"];
	Ack["EID"] = Frame["EID"];
	Ack["RID"] = Frame["RID"];
	Ack["FID"] = Frame["FID"];

	ProcessRawFrame(Frame["RAW"], Frame["EID"], Frame.Has("TID") ? Frame["TID"] : CVariant(GetID()));

	//Ack.Sign(m_pPrivateKey);
	
	QueueAck(Ack, pFromNode, pChannel);
}

// Tracing

void CKadRoute::TraceFrameSend(const CVariant& Frame)
{
	m_FrameHistory.push_back(SFrameTrace());
	SFrameTrace &FrameTrace = m_FrameHistory.back();

	FrameTrace.Frame = Frame.Clone();
	/*FrameTrace.Frame["EID"] = Frame["EID"];
	FrameTrace.Frame["RID"] = Frame["RID"];
	FrameTrace.Frame["FID"] = Frame["FID"];
	if(Frame.Has("SID"))
		FrameTrace.Frame["SID"] = Frame["SID"];*/
}

void CKadRoute::TraceFrameAck(const CVariant& Frame)
{
	for(FrameTraceList::reverse_iterator I = m_FrameHistory.rbegin(); I != m_FrameHistory.rend(); I++)
	{
		SFrameTrace &FrameTrace = *I;
		if(Frame["FID"] == FrameTrace.Frame["FID"] && Frame["RID"] == FrameTrace.Frame["RID"] && Frame["EID"] == FrameTrace.Frame["EID"])
		{
			FrameTrace.AckTime = GetCurTick();
			if(Frame.Has("ERR"))
				FrameTrace.Frame["ERR"] = Frame["ERR"];
			else if(FrameTrace.Frame.Has("ERR"))
				FrameTrace.Frame.Remove("ERR");
		}
	}
}

void CKadRoute::TraceFrameRecv(const CVariant& Frame)
{
	m_FrameHistory.push_back(SFrameTrace());
	SFrameTrace &FrameTrace = m_FrameHistory.back();
	FrameTrace.AckTime = -1; // incomming packet

	FrameTrace.Frame = Frame.Clone();
	/*FrameTrace.Frame["EID"] = Frame["EID"];
	FrameTrace.Frame["RID"] = Frame["RID"];
	FrameTrace.Frame["FID"] = Frame["FID"];
	if(Frame.Has("SID"))
		FrameTrace.Frame["SID"] = Frame["SID"];
	if(Frame.Has("ERR"))
		FrameTrace.Frame["ERR"] = Frame["ERR"];*/
}

///////////////////////////////////////////////////////////////////////////
//

IMPLEMENT_OBJECT(CRouteSessionImpl, CRouteSession)

CRouteSessionImpl::CRouteSessionImpl(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent)
: CRouteSession(EntityID, TargetID, pParent) 
{
	m_PacketsSize = 0;
}

void CRouteSessionImpl::HandleBytes(const CBuffer& Buffer, bool bStream)
{
	if(bStream)
		m_Stream.SetData(-1, Buffer.GetData(), Buffer.GetSize());
	else
	{
		m_Packets.push_back(Buffer);
		m_PacketsSize += Buffer.GetSize();
	}
}

void CRouteSessionImpl::PullBytes(CBuffer& Buffer, bool &bStream, size_t MaxBytes)
{
	for(list<CBuffer>::iterator I = m_Packets.begin(); I != m_Packets.end(); I++)
	{
		if(I->GetSize() <= MaxBytes)
		{
			size_t uLen = I->GetSize();
			byte* pBuffer = I->GetBuffer(true);
			m_Packets.erase(I);
			m_PacketsSize += uLen;
			ASSERT(m_PacketsSize >= 0);

			Buffer.SetBuffer(pBuffer, uLen);
			bStream = true;
			return;
		}
	}

	if(MaxBytes > m_Stream.GetSize())
		MaxBytes = m_Stream.GetSize();
	if(MaxBytes)
	{
		Buffer.SetData(m_Stream.GetBuffer(), MaxBytes);
		m_Stream.ShiftData(MaxBytes);
	}
}

bool CRouteSessionImpl::IsBussy()
{
	return GetPendingSize() > KB2B(128);
}

///////////////////////////////////////////////////////////////////////////
//

IMPLEMENT_OBJECT(CKadRouteImpl, CKadRoute)

CKadRouteImpl::CKadRouteImpl(const CUInt128& ID, CPrivateKey* pEntityKey, CObject* pParen)
: CKadRoute(ID, pEntityKey, pParen) 
{
}

bool CKadRouteImpl::QueueBytes(const CVariant& EntityID, const CVariant& SessionID, const CBuffer& Buffer, bool bStream)
{
	SessionMap::iterator I = m_SessionMap.find(EntityID);
	for(;I != m_SessionMap.end() && I->second->GetSessionID() != SessionID; I++);
	if(I != m_SessionMap.end())
	{
		I->second->QueueBytes(Buffer, bStream);
		return true;
	}
	return false;
}

void CKadRouteImpl::QuerySessions(list<SRouteSession>& Sessions)
{
	for(SessionMap::iterator I = m_SessionMap.begin(); I != m_SessionMap.end(); I++)
	{
		if(CRouteSessionImpl* pSession = I->second->Cast<CRouteSessionImpl>())
		{
			Sessions.push_back(SRouteSession());
			SRouteSession& Session = Sessions.back();
			Session.EntityID = I->first;
			Session.TargetID = I->second->GetTargetID();
			Session.SessionID = I->second->GetSessionID();

			Session.QueuedBytes = pSession->GetQueuedSize();
			Session.PendingBytes = pSession->GetPendingSize();

			Session.Connected = pSession->IsConnected();
		}
	}
}

bool CKadRouteImpl::PullBytes(const CVariant& EntityID, const CVariant& SessionID, CBuffer& Buffer, bool &bStream, size_t MaxBytes)
{
	SessionMap::iterator I = m_SessionMap.find(EntityID);
	for(;I != m_SessionMap.end() && I->second->GetSessionID() != SessionID; I++);
	if(I != m_SessionMap.end())
	{
		if(CRouteSessionImpl* pSession = I->second->Cast<CRouteSessionImpl>())
			pSession->PullBytes(Buffer, bStream, MaxBytes);
		return true;
	}
	return false;
}

CRouteSession* CKadRouteImpl::MkRouteSession(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent)
{
	return new CRouteSessionImpl(EntityID, TargetID, pParent);
}
