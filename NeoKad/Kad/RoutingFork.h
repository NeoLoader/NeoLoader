#pragma once

#include "RoutingZone.h"


class CRoutingFork: public CRoutingZone
{
public:
	DECLARE_OBJECT(CRoutingFork)

	CRoutingFork(uint8 uLevel, const CUInt128& uZoneIndex, CRoutingFork* pSuperZone, CObject* pParent = NULL);

	virtual void			Checkout();

	virtual bool			CanAdd(const CUInt128& ID);
	virtual bool			AddNode(CPointer<CKadNode>& pNode);
	virtual bool			RemoveNode(const CUInt128& ID);
	virtual CKadNode*		GetNode(const CUInt128& ID);

	virtual bool			GatherNode(SIterator& Iter, const CUInt128& uTargetID, CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS);

	virtual void			GetClosestNodes(const CUInt128& uTargetID, NodeMap& results, uint32 uDesiredCount, CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS);
	virtual CKadNode*		GetRandomNode(CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS);

	virtual ZoneList		GetZones()
	{
		ZoneList Zones;
		Zones.reserve(2);
		Zones.push_back(m_pLeftZone);
		Zones.push_back(m_pRightZone);
		return Zones;
	}

	virtual void			SetLookup(CPointer<CKadLookup> pRandomLookup);

protected:
	friend class CRoutingBin;

	virtual void			Replace(CRoutingZone* pOld, CRoutingZone* pNew);
	virtual size_t			Consolidate(bool bCleanUp = false);

	virtual CRoutingZone*	GetZone(const CUInt128& ID);

	CPointer<CRoutingZone>	m_pLeftZone;
	CPointer<CRoutingZone>	m_pRightZone;
};
