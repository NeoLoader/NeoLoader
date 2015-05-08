#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadNode.h"
#include "RoutingFork.h"
#include "RoutingBin.h"
#include "RoutingRoot.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "KadHandler.h"
#include "LookupManager.h"
#include "KadLookup.h"

IMPLEMENT_OBJECT(CRoutingFork, CRoutingZone)

CRoutingFork::CRoutingFork(uint8 uLevel, const CUInt128& uZoneIndex, CRoutingFork* pSuperZone, CObject* pParent)
: CRoutingZone(uLevel, uZoneIndex, pSuperZone, pParent) 
{
	CUInt128 uNewIndex(uZoneIndex);
	uNewIndex.ShiftLeft(1);
	m_pRightZone = new CRoutingBin(m_uLevel+1, uNewIndex, this, GetParent());
	m_pLeftZone = new CRoutingBin(m_uLevel+1, uNewIndex + 1, this, GetParent());
}

void CRoutingFork::Checkout()
{
	m_pLeftZone->Checkout();
	m_pRightZone->Checkout();
}

CRoutingZone* CRoutingFork::GetZone(const CUInt128& ID)
{
	CUInt128 uDistance = ID ^ GetParent<CKademlia>()->Root()->GetID();
	if(uDistance.GetBit(m_uLevel))
		return m_pLeftZone;
	else
		return m_pRightZone;
}

bool CRoutingFork::CanAdd(const CUInt128& ID)
{
	ASSERT(m_uLevel < ID.GetSize());
	if(IsDistantZone())
		return false;
	return GetZone(ID)->CanAdd(ID);
}

bool CRoutingFork::AddNode(CPointer<CKadNode>& pNode)
{
	return GetZone(pNode->GetID())->AddNode(pNode);
}

bool CRoutingFork::RemoveNode(const CUInt128& ID)
{
	return GetZone(ID)->RemoveNode(ID);
}

CKadNode* CRoutingFork::GetNode(const CUInt128& ID)
{
	return GetZone(ID)->GetNode(ID);
}

bool CRoutingFork::GatherNode(SIterator& Iter, const CUInt128& uTargetID, CSafeAddress::EProtocol eProtocol, int iMaxState)
{
	if(Iter.TreeDepth == Iter.Path.size())
	{
		CUInt128 uDistance = uTargetID ^ GetParent<CKademlia>()->Root()->GetID();
		Iter.Path.push_back(uDistance.GetBit(m_uLevel) ? SIterator::eLeft : SIterator::eRight);
	}
	bool Ret = false;
	byte& uPath = Iter.Path.at(Iter.TreeDepth);
	Iter.TreeDepth++;
repeat:
	if(((uPath & SIterator::eLeft) ? m_pLeftZone : m_pRightZone)->GatherNode(Iter, uTargetID, eProtocol, iMaxState)) // if we exhausted the right path
	{
		if((uPath & SIterator::eAux) == 0) // we havnt tryed teh wrong path yet
		{
			uPath = ((uPath & SIterator::eLeft) ? SIterator::eRight : SIterator::eLeft) | SIterator::eAux; // make sure the next try wil take the wrong apth
			if(Iter.TempNodes.size() < Iter.MinNodes)
				goto repeat; // we dont have ebough nodes, we take the wrong way right away
		}
		else if(Iter.TreeDepth == Iter.Path.size())
		{
			Iter.Path.pop_back(); 
			Ret = true; // this path is exhausted remove its data
		}
	}
	Iter.TreeDepth--;
	return Ret;
}

void CRoutingFork::GetClosestNodes(const CUInt128& uTargetID, NodeMap& results, uint32 uDesiredCount, CSafeAddress::EProtocol eProtocol, int iMaxState)
{
	CUInt128 uDistance = uTargetID ^ GetParent<CKademlia>()->Root()->GetID();
	bool Left = uDistance.GetBit(m_uLevel);

	(Left ? m_pLeftZone : m_pRightZone)->GetClosestNodes(uTargetID, results, uDesiredCount, eProtocol, iMaxState);
	if(results.size() < uDesiredCount)
		(!Left ? m_pLeftZone : m_pRightZone)->GetClosestNodes(uTargetID, results, uDesiredCount, eProtocol, iMaxState);
}

CKadNode* CRoutingFork::GetRandomNode(CSafeAddress::EProtocol eProtocol, int iMaxState)
{
	bool Left = (rand() % 2);

	CKadNode* pNode = (Left ? m_pLeftZone : m_pRightZone)->GetRandomNode(eProtocol, iMaxState);
	if(!pNode) 
		pNode = (!Left ? m_pLeftZone : m_pRightZone)->GetRandomNode(eProtocol, iMaxState);
	return pNode; // Note: this fails only if the entier zone does not contain eveon one elegable node
}

void CRoutingFork::Replace(CRoutingZone* pOld, CRoutingZone* pNew)
{
	ASSERT(m_pLeftZone == pOld || m_pRightZone == pOld);

	if(m_pLeftZone == pOld)
		m_pLeftZone = pNew;
	else
		m_pRightZone = pNew;
	pNew->m_pSuperZone = this;
}

size_t CRoutingFork::Consolidate(bool bCleanUp)
{
	size_t uCount = 0;
	uCount += m_pLeftZone->Consolidate(bCleanUp);
	uCount += m_pRightZone->Consolidate(bCleanUp);

	if(m_pSuperZone == NULL || uCount >= (size_t)GetParent<CKademlia>()->Cfg()->GetInt("BucketSize"))
		return uCount;

	CRoutingBin* pLeftBin = m_pLeftZone->Cast<CRoutingBin>();
	CRoutingBin* pRightBin = m_pRightZone->Cast<CRoutingBin>();
	if(pLeftBin && pRightBin) //sometimes we get here still with a zone or two...
	{
		if(pLeftBin->IsLooking() || pRightBin->IsLooking())
			return uCount; // dont consolidate zones that are looking right now

		CPointer<CRoutingZone> pRoutingBin = new CRoutingBin(m_uLevel, m_uZoneIndex, m_pSuperZone, GetParent());

		for(NodeList::iterator I = pLeftBin->m_Nodes.begin(); I != pLeftBin->m_Nodes.end();I++)
			pRoutingBin->AddNode(*I);

		for(NodeList::iterator I = pRightBin->m_Nodes.begin(); I != pRightBin->m_Nodes.end();I++)
			pRoutingBin->AddNode(*I);

		m_pSuperZone->Replace(this, pRoutingBin); // this will delete this
	}
	return uCount;
}

void CRoutingFork::SetLookup(CPointer<CKadLookup> pRandomLookup)
{
	CRoutingZone* pZone = GetZone(pRandomLookup->GetID());
	if(CRoutingFork* pFork = pZone->Cast<CRoutingFork>())
		pFork->SetLookup(pRandomLookup);
	else if(CRoutingBin* pBin = pZone->Cast<CRoutingBin>())
		pBin->SetLookup(pRandomLookup);
}