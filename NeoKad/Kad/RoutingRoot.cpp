#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadNode.h"
#include "RoutingRoot.h"
#include "KadNode.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "LookupManager.h"
#include "KadLookup.h"

IMPLEMENT_OBJECT(CRoutingRoot, CRoutingFork)

CRoutingRoot::CRoutingRoot(CPrivateKey* pKey, CObject* pParent)
:CRoutingFork(0, 0, NULL, pParent), m_ID(pKey) 
{
	m_NextCheckout = 0;
	m_NextConsolidate = 0;
	m_NextSelfLookup = GetCurTick();

	m_pMaxDistance = CUInt128(true); // -1 

	m_NodeCount = 0;
}

CKadNode* CRoutingRoot::GetClosestNode(SIterator& Iter, const CUInt128& uTargetID, CSafeAddress::EProtocol eProtocol, int iMaxState)
{
	if(Iter.MinNodes == 0) // request for randome nodes
	{
		for(int i = 0; i < 10; i++)
		{
			CKadNode* pNode = GetRandomNode(eProtocol);
			if(!pNode)
				continue;
			CUInt128 uDistance = uTargetID ^ pNode->GetID();
			pair<NodeMap::iterator, bool> Res = Iter.TempNodes.insert(NodeMap::value_type(uDistance, pNode));
			if(Res.second)
				return pNode;
		}
		return NULL;
	}

	int MinNodes = Iter.MinNodes;
	if(MinNodes > 1)
	{
		if(MinNodes > Iter.GivenNodes)
			MinNodes -= Iter.GivenNodes;
		else // this happens if we head to get more nodes that we projected to need from teh target area
			MinNodes = 1;
	}

	if(Iter.TempNodes.size() < Iter.MinNodes && Iter.TreeDepth != -1) // if we dont have enough nodes in stock, grab the next bucket
	{
		CRoutingFork::GatherNode(Iter, uTargetID, eProtocol, iMaxState);
		if(Iter.Path.empty())
			Iter.TreeDepth = -1; // No more nodes prevent reply
	}

	if(Iter.TempNodes.empty())
		return NULL;

	NodeMap::iterator I = Iter.TempNodes.begin();
	if(MinNodes > 1)
	{
		for(int Index = (rand() % Iter.TempNodes.size()); Index > 0; Index--)
		{
			I++;
			ASSERT(I != Iter.TempNodes.end());
		}
	}
	CKadNode* pNode = I->second;
	Iter.TempNodes.erase(I);
	Iter.GivenNodes++;
	return pNode;
}

void CRoutingRoot::Process(UINT Tick)
{
	if(m_NextCheckout < GetCurTick())
	{
		Checkout();
		m_NextCheckout = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt64("CheckoutInterval"));
	}
	
	if(m_NextConsolidate < GetCurTick())
		Consolidate();

	if(m_NextSelfLookup < GetCurTick())
	{
		CLookupManager* pLookupManager = GetParent<CKademlia>()->Manager();
		if(!m_pSelfLookup)
		{
			m_pSelfLookup = new CKadLookup(m_ID, pLookupManager);
			pLookupManager->StartLookup(m_pSelfLookup);
		}
		else if(m_pSelfLookup->IsStopped())
		{
			size_t LookupWidth = 2 * (size_t)GetParent<CKademlia>()->Cfg()->GetInt("BucketSize");

			NodeMap Nodes;
			GetClosestNodes(m_ID, Nodes, LookupWidth);
			if(Nodes.size() < LookupWidth) // if we dont even filled 2 buckets allow any distance
				m_pMaxDistance = CUInt128(true); // -1
			else // Set the default lookup distance to the end of the 2nd bucket or the 
			{
				ASSERT(Nodes.size() == LookupWidth);
				m_pMaxDistance = (--Nodes.end())->first; // = (--Nodes.end())->second->GetID() ^ I->second->GetID();
			}

			pLookupManager->StopLookup(m_pSelfLookup);
			m_pSelfLookup = NULL;
			m_NextSelfLookup = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt64("SelfLookupInterval"));
		}
	}
}

size_t CRoutingRoot::Consolidate(bool bCleanUp)
{
	m_NextConsolidate = GetCurTick() + SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt64("ConsolidateInterval"));
	m_NodeCount = CRoutingFork::Consolidate(bCleanUp);
	return m_NodeCount;
}

bool CRoutingRoot::AddNode(CPointer<CKadNode>& pNode)
{
	if(pNode->GetID() == m_ID)
		return false; // dont add ourselves to our own routing tree

	pNode->SetParent(this);
	if(CRoutingFork::AddNode(pNode))
		m_NodeCount++;
	return true;
}

bool CRoutingRoot::RemoveNode(const CUInt128& ID)
{
	if(!CRoutingFork::RemoveNode(ID))
		return false;
	m_NodeCount--;
	return true;
}

NodeList CRoutingRoot::GetAllNodes()
{
	const list<CObject*>& Children = GetChildren();
	NodeList Nodes;
	Nodes.reserve(Children.size());
	for(list<CObject*>::const_iterator I = Children.begin(); I != Children.end(); I++)
	{
		if(CKadNode* pNode = (*I)->Cast<CKadNode>())
			Nodes.push_back(pNode);
	}
	return Nodes;
}