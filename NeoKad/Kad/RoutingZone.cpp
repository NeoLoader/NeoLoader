#include "GlobalHeader.h"
#include "KadHeader.h"
#include "RoutingZone.h"
#include "KadNode.h"
#include "Kademlia.h"
#include "KadConfig.h"

IMPLEMENT_OBJECT(CRoutingZone, CObject)

CRoutingZone::CRoutingZone(uint8 uLevel, const CUInt128& uZoneIndex, CRoutingFork* pSuperZone, CObject* pParent) 
: CObject(pParent) 
{
	m_uLevel = uLevel;
	m_uZoneIndex = uZoneIndex;
	m_pSuperZone = pSuperZone; 
}

void CRoutingZone::GetBootstrapNodes(const CUInt128& uTargetID, NodeMap& results, uint32 uDesiredCount, CSafeAddress::EProtocol eProtocol, int iMaxState)
{
	while(uDesiredCount-->0)
	{
		if(CKadNode* pNode = GetRandomNode(eProtocol))
			results[uTargetID ^ pNode->GetID()] = pNode;
	}
}

bool CRoutingZone::IsDistantZone()
{
	if (m_uZoneIndex >= GetParent<CKademlia>()->Cfg()->GetInt("ZoneLimit") 
	 && m_uLevel >= GetParent<CKademlia>()->Cfg()->GetInt("LevelLimit"))
		return true;
	return false;
}

CUInt128 CRoutingZone::GetPrefix()
{
	CUInt128 uPrefix(m_uZoneIndex);
	uPrefix.ShiftLeft(uPrefix.GetBitSize() - m_uLevel);
	return uPrefix;
}