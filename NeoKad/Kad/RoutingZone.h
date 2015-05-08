#pragma once

class CKadHandler;
class CRoutingBin;
class CRoutingFork;
class CRoutingZone;
class CKadNode;

class CRoutingZone: public CObject
{
public:
	DECLARE_OBJECT(CRoutingZone)

	CRoutingZone(uint8 uLevel, const CUInt128& uZoneIndex, CRoutingFork* pSuperZone, CObject* pParent = NULL);

	virtual void			Checkout() = 0;

	virtual bool			CanAdd(const CUInt128& ID) = 0;
	virtual bool			AddNode(CPointer<CKadNode>& pNode) = 0;
	virtual bool			RemoveNode(const CUInt128& ID) = 0;
	virtual CKadNode*		GetNode(const CUInt128& ID) = 0;

	struct SIterator
	{
		SIterator(int iCount = 1){
			MinNodes = iCount; // 0 means fully random, 1 means best, > 1 means means semi random
			GivenNodes = 0;
			TreeDepth = 0;
			Path.reserve(31);
		}
		int				MinNodes;
		int				GivenNodes;
		int				TreeDepth;
		enum EPath
		{
			eRight = 0x00,
			eLeft = 0x01,
			eAux = 0x02,
		};
		vector<byte>	Path;
		NodeMap			TempNodes;
	};
	virtual bool			GatherNode(SIterator& Iter, const CUInt128& uTargetID, CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS) = 0;

	virtual void			GetClosestNodes(const CUInt128& uTargetID, NodeMap& results, uint32 uDesiredCount, CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS) = 0;
	virtual void			GetBootstrapNodes(const CUInt128& uTargetID, NodeMap& results, uint32 uDesiredCount, CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS);
	virtual CKadNode*		GetRandomNode(CSafeAddress::EProtocol eProtocol = CSafeAddress::eInvalid, int iMaxState = NODE_DEFAULT_CLASS) = 0;

	virtual int				GetLevel()	{return m_uLevel;}
	virtual CUInt128		GetPrefix();
	virtual bool			IsDistantZone();

protected:
	friend class CRoutingFork;
	friend class CKadHandler;

	virtual size_t			Consolidate(bool bCleanUp = false) = 0;

	CRoutingFork*			m_pSuperZone;

	/** Comment copyed form eMule:
	* The level indicates what size chunk of the address space
	* this zone is representing. Level 0 is the whole space,
	* level 1 is 1/2 of the space, level 2 is 1/4, etc.
	*/
	uint8					m_uLevel;
	/** Comment copyed form eMule:
	* This is the distance in number of zones from the zone at this level
	* that contains the center of the system; distance is wrt the XOR metric.
	*/
	CUInt128				m_uZoneIndex;
};
