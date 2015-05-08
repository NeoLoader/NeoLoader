#pragma once

#include "RoutingFork.h"

#include "KadID.h"

class CRoutingRoot: public CRoutingFork
{
public:
	DECLARE_OBJECT(CRoutingRoot)

	CRoutingRoot(CPrivateKey* pKey, CObject* pParent = NULL);

	CMyKadID&				GetID()				{return m_ID;}
	const CMyKadID&			GetID() const		{return m_ID;}

	virtual bool			AddNode(CPointer<CKadNode>& pNode);
	virtual bool			RemoveNode(const CUInt128& ID);

	virtual CKadNode*		GetClosestNode(SIterator& Iter, const CUInt128& uTargetID, CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS);

	virtual const CUInt128& GetMaxDistance()	{return m_pMaxDistance;}

	virtual bool			IsLooking()			{return m_pSelfLookup != NULL;}
	virtual uint64			GetNextLooking()	{return m_NextSelfLookup;}

	virtual void			Process(UINT Tick);

	virtual size_t			Consolidate(bool bCleanUp = false);
	virtual size_t			GetNodeCount()								{return m_NodeCount;}

	virtual NodeList		GetAllNodes();

protected:
	CMyKadID				m_ID;

	CPointer<CKadLookup>	m_pSelfLookup;
	CUInt128				m_pMaxDistance;

	uint64					m_NextCheckout;
	uint64					m_NextConsolidate;
	uint64					m_NextSelfLookup;

	size_t					m_NodeCount;
};
