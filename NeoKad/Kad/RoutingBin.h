#pragma once

#include "RoutingZone.h"

class CRoutingBin: public CRoutingZone
{
public:
	DECLARE_OBJECT(CRoutingBin)

	CRoutingBin(uint8 uLevel, const CUInt128& uZoneIndex, CRoutingFork* pSuperZone, CObject* pParent = NULL);

	virtual void			Checkout();

	virtual bool			CanAdd(const CUInt128& ID)	{return true;}
	virtual bool			AddNode(CPointer<CKadNode>& pNode);
	virtual bool			RemoveNode(const CUInt128& ID);
	virtual CKadNode*		GetNode(const CUInt128& ID);

	virtual bool			GatherNode(SIterator& Iter, const CUInt128& uTargetID, CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS);

	virtual void			GetClosestNodes(const CUInt128& uTargetID, NodeMap& results, uint32 uDesiredCount, CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS);
	virtual CKadNode*		GetRandomNode(CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS);

	virtual const NodeList&	GetNodes()	{return m_Nodes;}

	virtual void			SetLookup(CPointer<CKadLookup> pRandomLookup)	{m_pRandomLookup = pRandomLookup;}
	virtual bool			IsLooking()	{return m_pRandomLookup != NULL;}
	virtual CUInt128		GetRandomID();
	virtual uint64			GetNextLooking() {return m_NextNodeLookup;}

protected:
	friend class CRoutingFork;

	virtual size_t			Consolidate(bool bCleanUp = false);
	virtual void			Split();

	NodeList				m_Nodes;

	CPointer<CKadLookup>	m_pRandomLookup;
	uint64					m_NextNodeLookup;
};
