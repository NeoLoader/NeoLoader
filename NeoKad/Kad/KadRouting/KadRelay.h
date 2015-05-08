#pragma once

#include "../KadLookup.h"

class CFrameRelay;

class CKadRelay: public CKadLookup
{
public:
	DECLARE_OBJECT(CKadRelay)

	virtual void		SetBrancheCount(int SpreadCount);
	virtual int			GetBrancheCount()										{return m_BrancheCount;}

	CKadRelay(const CUInt128& ID, CObject* pParent = NULL);
	virtual bool		InitRelay(const CVariant& RouteReq);

	virtual CVariant	GetEntityID()											{return m_EntityID;}

	virtual bool		SearchingRelays();

	virtual void		Process(UINT Tick);

	virtual string		AddDownLink(CKadNode* pNode, CComChannel* pChannel);
	virtual bool		RequestUpLink(CKadNode* pNode, CComChannel* pChannel);
	virtual void 		UpLinkResponse(CKadNode* pNode, CComChannel* pChannel, const string &Error);

	virtual bool		RelayUp(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel);		// send something into the target area
	virtual bool		RelayDown(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel);		// send something back to the source

	virtual bool		AckUp(const CVariant& Ack, bool bDelivery = false);				// ack a frame sent into the target area
	virtual bool		AckDown(const CVariant& Ack, bool bDelivery = false);			// ack a frame sent back to the source

	virtual bool		OnCloserNodes(CKadNode* pNode, CComChannel* pChannel, int CloserCount);

	virtual bool		IsNodeUsed(CKadNode* pNode);

	virtual void		Refresh();
	virtual bool		ReadyToStop();

	virtual CFrameRelay*GetUpLink()												{return m_UpLink;}
	virtual CFrameRelay*GetDownLink()											{return m_DownLink;}

	virtual CPublicKey*	GetPublicKey()											{return m_pPublicKey;}

protected:
	virtual bool		AcceptDownLink(CKadNode* pNode);

	virtual void		NodeStalling(CKadNode* pNode)							{if(m_PendingRelays > 0) m_PendingRelays--;}


	struct SRelayState: SNodeStatus
	{
		SRelayState() {}
		string Error;
	};

	virtual SNodeStatus*NewNodeStatus()	{return new SRelayState();}

	virtual SRelayState*GetState(CKadNode* pNode)			{return (SRelayState*)CKadLookup::GetStatus(pNode);}

	int					m_BrancheCount;

	int					m_PendingRelays;

	CHolder<CPublicKey>	m_pPublicKey;
	CVariant			m_EntityID;
	
	uint64				m_LastReset;

	CFrameRelay*		m_UpLink;	// link to be used to send something into the target area
	CFrameRelay*		m_DownLink;	// link to be used to send something back to the sender
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class CKadBridge: public CKadRelay
{
public:
	DECLARE_OBJECT(CKadBridge);

	CKadBridge(const CUInt128& ID, CObject* pParent = NULL);

	virtual void		InitBridge(CKadRelay* pRelay);

	virtual void		Process(UINT Tick);

	virtual bool		RelayUp(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel);
	virtual bool		RelayDown(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel)	{ASSERT(0); return false;}

	virtual bool		AckUp(const CVariant& Ack, bool bDelivery = false);
	virtual bool		AckDown(const CVariant& Ack, bool bDelivery = false)				{ASSERT(0); return NULL;}

	virtual bool		ReadyToStop();

protected:

	CPointer<CKadRelay> m_pRelay;
};
