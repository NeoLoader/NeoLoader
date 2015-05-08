#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadHandler.h"
#include "LookupManager.h"
#include "Kademlia.h"
#include "KadLookup.h"
#include "KadTask.h"
#include "KadRouting/KadRelay.h"
#include "KadConfig.h"
#include "KadNode.h"
#include "RoutingRoot.h"

IMPLEMENT_OBJECT(CLookupManager, CObject)

CLookupManager::CLookupManager(CObject* pParent)
 : CObject(pParent) 
{
	m_NodeLookupCount = 0;
}

CLookupManager::~CLookupManager()
{
	for(LookupMap::iterator I = m_LookupMap.begin(); I != m_LookupMap.end(); I++)
	{
		CKadLookup* pLookup = I->second;
		if(pLookup->IsStarted())
			pLookup->Stop();
	}
}

void CLookupManager::Process(UINT Tick)
{
	for(LookupMap::iterator I = m_LookupMap.begin(); I != m_LookupMap.end();)
	{
		CKadLookup* pLookup = I->second;

		if((Tick & EPer5Sec) != 0 && pLookup->StoppedSince() > ((pLookup->Inherits("CKadTask") || pLookup->Inherits("CKadRoute")) ? MIN2MS(3) : 0)) // K-ToDo-Now: customize
		{
			I = m_LookupMap.erase(I); // get rid of orphan lookups
			continue;
		}
		I++;

		if(!pLookup->IsStarted())
			continue;

		if(pLookup->IsStopping())
		{
			if(pLookup->CachesEmpty())
				pLookup->Stop();
		}
		else if(pLookup->ReadyToStop())
			pLookup->PrepareStop();
		else
			pLookup->Process(Tick);
	}
}

CVariant CLookupManager::StartLookup(CPointer<CKadLookup> pLookup)
{
	if(!pLookup->GetLookupID().IsValid())
		pLookup->InitLookupID();

	ASSERT(m_LookupMap.find(pLookup->GetLookupID()) == m_LookupMap.end());
	m_LookupMap.insert(LookupMap::value_type(pLookup->GetLookupID(), pLookup)); 

	pLookup->Start();

	if(!pLookup->Inherits("CKadOperation") && !pLookup->Inherits("CKadRelay"))
		m_NodeLookupCount++;

	return pLookup->GetLookupID();
}

void CLookupManager::StopLookup(CPointer<CKadLookup> pLookup)
{
	for(LookupMap::iterator I = m_LookupMap.begin(); I != m_LookupMap.end(); I++)
	{
		if(I->second == pLookup)
		{
			m_LookupMap.erase(I);
			break;
		}
	}

	if(!pLookup->Inherits("CKadOperation") && !pLookup->Inherits("CKadRelay"))
		m_NodeLookupCount--;

	if(pLookup->IsStarted())
		pLookup->Stop();
}

void CLookupManager::AddNodes(CKadLookup* pLookup, CKadNode* pNode, CComChannel* pChannel, const NodeMap& Nodes)
{
	pLookup->AddNodes(Nodes, pNode->GetID());

	CUInt128 uDistance = pLookup->GetID() ^ pNode->GetID();
	int Count = 0;
	for(NodeMap::const_iterator I = Nodes.begin(); I != Nodes.end(); I++)
	{
		CKadNode* pFoundNode = I->second;
		if(I->first >= uDistance)
			continue;
		
		if(pLookup->WasNodeUsed(pFoundNode)) // checks if the node was already asked - dont count already asked node
			continue;
		
		if(Count > 0 || pLookup->RequestNodes(pFoundNode, pChannel, pNode->GetID())) // count first node only if the request went out
			Count++;
	}
	pLookup->OnCloserNodes(pNode, pChannel, Count);
}

CKadLookup* CLookupManager::GetLookup(const CVariant& LookupID)
{
	LookupMap::iterator I = m_LookupMap.find(LookupID);
	if(I != m_LookupMap.end())
		return I->second;
	return NULL;
}

//CLookupProxy* CLookupManager::GetProxy(const CVariant& ReturnID, const CUInt128& ProxyID)
//{
//	for(LookupMap::iterator I = m_LookupMap.begin(); I != m_LookupMap.end();I++)
//	{
//		if(CLookupProxy* pProxy = I->second->Cast<CLookupProxy>())
//		{
//			if(pProxy->GetRetrunID() == ReturnID && pProxy->GetProxyID() == ProxyID)
//				return pProxy;
//		}
//	}
//	return NULL;
//}

CKadRelay* CLookupManager::GetRelay(const CVariant& EntityID)
{
	for(LookupMap::iterator I = m_LookupMap.begin(); I != m_LookupMap.end();I++)
	{
		if(CKadRelay* pRelay = I->second->Cast<CKadRelay>())
		{
			if(pRelay->Inherits("CKadBridge"))
				continue;
			if(pRelay->GetEntityID() == EntityID)
				return pRelay;
		}
	}
	return NULL;
}

CKadRelay* CLookupManager::GetRelayEx(const CVariant& EntityID, const CUInt128& TargetID)
{
	CKadRelay* pFoundRelay = NULL;
	for(LookupMap::iterator I = m_LookupMap.begin(); I != m_LookupMap.end();I++)
	{
		if(CKadBridge* pBridge = I->second->Cast<CKadBridge>())
		{
			if(pBridge->GetEntityID() == EntityID && pBridge->GetID() == TargetID)
				return pBridge;
		}
		else if(CKadRelay* pRelay = I->second->Cast<CKadRelay>())
		{
			if(pRelay->GetEntityID() == EntityID)
			{
				ASSERT(pFoundRelay == NULL);
				pFoundRelay = pRelay;
			}
		}
	}
	return pFoundRelay;
}

CKadBridge* CLookupManager::GetBridge(const CVariant& EntityID, const CUInt128& TargetID)
{
	for(LookupMap::iterator I = m_LookupMap.begin(); I != m_LookupMap.end();I++)
	{
		if(CKadBridge* pBridge = I->second->Cast<CKadBridge>())
		{
			if(pBridge->GetEntityID() == EntityID && pBridge->GetID() == TargetID)
				return pBridge;
		}
	}
	return NULL;
}