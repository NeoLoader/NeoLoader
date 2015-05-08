#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadHandler.h"
#include "RoutingBin.h"
#include "RoutingFork.h"
#include "RoutingRoot.h"
#include "KadNode.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "LookupManager.h"
#include "KadLookup.h"

IMPLEMENT_OBJECT(CRoutingBin, CRoutingZone)

CRoutingBin::CRoutingBin(uint8 uLevel, const CUInt128& uZoneIndex, CRoutingFork* pSuperZone, CObject* pParent)
: CRoutingZone(uLevel, uZoneIndex, pSuperZone, pParent) 
{
	m_Nodes.reserve((size_t)GetParent<CKademlia>()->Cfg()->GetInt("BucketSize"));
	m_NextNodeLookup = GetCurTick();
}

void CRoutingBin::Checkout()
{
	//if(!IsDistantZone()) // Do not check out nodes in distant zones Note: we need to ensure all nodes in our tree are valid so we check always all
	
	for(NodeList::iterator I = m_Nodes.begin(); I != m_Nodes.end();I++)
	{
		CPointer<CKadNode> pNode = *I;

		if(GetTime() - pNode->GetLastHello() < GetParent<CKademlia>()->Cfg()->GetInt("HelloInterval"))
			continue; // we already tryed that oen recently

		if(pNode->IsFading(true) || pNode->GetLastHello() == 0) // if the node is fayding or we never head contact with it
		{
			if(GetParent<CKademlia>()->Handler()->CheckoutNode(pNode))
			{
				m_Nodes.erase(I);
				m_Nodes.push_back(pNode);
				break;
			}
		}
	}
	

	if(m_NextNodeLookup < GetCurTick())
	{	
		CLookupManager* pLookupManager = GetParent<CKademlia>()->m_pLookupManager;
		if(!m_pRandomLookup)
		{
			if(IsDistantZone() || m_Nodes.size() >= (size_t)GetParent<CKademlia>()->Cfg()->GetInt("BucketSize")*8/10)
				return;

			if(pLookupManager->GetNodeLookupCount() >= GetParent<CKademlia>()->Cfg()->GetInt64("MaxNodeLookups"))
				return;

			// Look-up a random client in this zone
			CUInt128 uRandom(GetPrefix(), m_uLevel);
			uRandom.Xor(GetParent<CKademlia>()->Root()->GetID());
			// Note: we start this with a strong pointer (extern lookups use weak ponters) 
			// so that when the pointer gets removed when the lookup finishes it will be deleted
			m_pRandomLookup = new CKadLookup(uRandom, pLookupManager);
			pLookupManager->StartLookup(m_pRandomLookup);
		}
		else if(m_pRandomLookup->IsStopped())
		{
			pLookupManager->StopLookup(m_pRandomLookup);
			m_pRandomLookup = NULL;
			m_NextNodeLookup = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt64("NodeLookupInterval"));
		}
	}
}

bool CRoutingBin::AddNode(CPointer<CKadNode>& pNode)
{
	if(CKadNode* pCurNode = GetNode(pNode->GetID()))
	{
		pCurNode->Merge(pNode);
		pNode = pCurNode; // update pointer
	}
	else
	{
		m_Nodes.push_back(pNode);

		if(m_Nodes.size() > (size_t)GetParent<CKademlia>()->Cfg()->GetInt("BucketSize"))
			Split();
		return true;
	}
	return false;
}

bool CRoutingBin::RemoveNode(const CUInt128& ID)
{
	for(NodeList::iterator I = m_Nodes.begin(); I != m_Nodes.end();I++)
	{
		if((*I)->GetID() == ID)
		{
			m_Nodes.erase(I);
			return true;
		}
	}
	return false;
}

CKadNode* CRoutingBin::GetNode(const CUInt128& ID)
{
	for(NodeList::iterator I = m_Nodes.begin(); I != m_Nodes.end();I++)
	{
		if((*I)->GetID() == ID)
			return *I;
	}
	return NULL;
}

void CRoutingBin::GetClosestNodes(const CUInt128& uTargetID, NodeMap& results, uint32 uDesiredCount, CSafeAddress::EProtocol eProtocol, int iMaxState)
{
	for(NodeList::iterator I = m_Nodes.begin(); I != m_Nodes.end();I++)
	{
		if((*I)->GetClass() > iMaxState)
			continue;
		if(eProtocol && !(*I)->GetAddress(eProtocol).IsValid())
			continue;
		if((*I)->HasFailed())
			continue;

		CUInt128 uDistance = uTargetID ^ (*I)->GetID();
		results[uDistance] = *I;
	}

	while(results.size() > uDesiredCount)
		results.erase(--results.end());
}

bool CRoutingBin::GatherNode(SIterator& Iter, const CUInt128& uTargetID, CSafeAddress::EProtocol eProtocol, int iMaxState)
{
	GetClosestNodes(uTargetID, Iter.TempNodes, -1, eProtocol, iMaxState); // we always take the whole bucker
	return true;
}

CKadNode* CRoutingBin::GetRandomNode(CSafeAddress::EProtocol eProtocol, int iMaxState)
{
	int Rand = rand();
	for(int i=0; i < m_Nodes.size(); i++)
	{
		int Index = (Rand + i) % m_Nodes.size();

		CKadNode* pNode = m_Nodes.at(Index);
		if (pNode->GetClass() > iMaxState)
			continue;
		if (eProtocol && !pNode->GetAddress(eProtocol).IsValid())
			continue;
		if (pNode->HasFailed())
			continue;

		return pNode;
	}
	return NULL; // Note: this function fails only if in the entire bucket is not even one elegable node
}

void CRoutingBin::Split()
{
	CPointer<CRoutingFork> pRoutingZone = new CRoutingFork(m_uLevel, m_uZoneIndex, m_pSuperZone, GetParent());
	for(NodeList::iterator I = m_Nodes.begin(); I != m_Nodes.end();I++)
		pRoutingZone->AddNode(*I);
	if(m_pRandomLookup)
		pRoutingZone->SetLookup(m_pRandomLookup);
	m_pSuperZone->Replace(this, pRoutingZone); // this will delete this
}

size_t CRoutingBin::Consolidate(bool bCleanUp)
{	
	size_t MaxPurge = ((m_Nodes.size() + 1) / 2); // never purge more than half a bucket at once

	for(NodeList::iterator I = m_Nodes.begin(); I != m_Nodes.end() && MaxPurge > 1;)
	{
		if((*I)->IsFading() || (IsDistantZone() && (bCleanUp || !(*I)->IsNeeded())))
		{
			I = m_Nodes.erase(I);
			MaxPurge --;
		}
		else
			I++;
	}
	return m_Nodes.size();
}

CUInt128 CRoutingBin::GetRandomID()
{
	return m_pRandomLookup ? m_pRandomLookup->GetID() : 0;
}