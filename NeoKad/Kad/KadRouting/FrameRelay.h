#pragma once

#include "RouteStats.h"

class CFrameRelay: public CObject
{
public:
	DECLARE_OBJECT(CFrameRelay)

	CFrameRelay(CObject* pParent = NULL);

	virtual void		AllowDelay(bool bAllow = true)		{m_AllowDelay = bAllow;}

	virtual void		Process(UINT Tick);

	virtual bool		Relay(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel);
	virtual bool		Ack(const CVariant& Ack, bool bDelivery);

	virtual bool		Has(CKadNode* pNode);
	virtual void		Add(CKadNode* pNode, CComChannel* pChannel);
	virtual void		Remove(const SKadNode& Node);
	virtual bool		IsEmpty(bool bNodesOnly = false)		{return m_Nodes.empty() && (bNodesOnly || m_Frames.empty());}


	typedef map<SKadNode, CPointer<CRelayStats> > TRelayMap;

	virtual TRelayMap&	GetNodes()								{return m_Nodes;}
	const SRelayStats&	GetStats() const						{return m_Stats;}

protected:
	TRelayMap			m_Nodes;

	struct SFrame
	{
		SFrame(const CVariant& frame, uint64 ttl)	
		{
			Frame = frame;
			ReciveTime = GetCurTick();
			TTL = ttl;
			SendTime = 0;
			RelayTime = 0;
		}

		CVariant		Frame;
		uint64			ReciveTime;		// when the frame was recived
		uint64			TTL;
		uint64			SendTime;		// when the frame was sent
		uint64			RelayTime;		// when the relaying has ben acknowledge
		SKadNode		To;
		SKadNode		From;

		map<CUInt128, string> Failed;	// list of already tryed but failed nodes
	};
	typedef list<CScoped<SFrame> > TFrameList;
	TFrameList			m_Frames;

	bool				m_AllowDelay;

	SRelayStats			m_Stats;

	struct SRoutingPlan
	{
		SRoutingPlan() {
			uLastUpdate = 0;
		}

		uint64				uLastUpdate;
		map<int, CKadNode*>	Routes;
	};

	CKadNode*			SelectNode(const CVariant& RID);

	map<CVariant, SRoutingPlan> m_RoutingCache;
};