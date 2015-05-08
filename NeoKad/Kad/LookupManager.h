#pragma once

class CKadLookup;
class CLookupProxy;
class CKadRelay;
class CKadBridge;
class CSafeAddress;
class CComChannel;

class CLookupManager: public CObject
{
public:
	DECLARE_OBJECT(CLookupManager)

	CLookupManager(CObject* pParent = NULL);
	~CLookupManager();

	void			Process(UINT Tick);

	CVariant		StartLookup(CPointer<CKadLookup> pLookup);
	void			StopLookup(CPointer<CKadLookup> pLookup);

	void			AddNodes(CKadLookup* pLookup, CKadNode* pNode, CComChannel* pChannel, const NodeMap& Nodes);

	CKadLookup*		GetLookup(const CVariant& LookupID);
	//CLookupProxy*	GetProxy(const CVariant& ReturnID, const CUInt128& ProxyID);
	CKadRelay*		GetRelay(const CVariant& EntityID);
	CKadRelay*		GetRelayEx(const CVariant& EntityID, const CUInt128& TargetID);
	CKadBridge*		GetBridge(const CVariant& EntityID, const CUInt128& TargetID);

	const LookupMap&GetLookups()				{return m_LookupMap;}
	uint16			GetNodeLookupCount()		{return m_NodeLookupCount;}

private:
	LookupMap		m_LookupMap;
	uint16			m_NodeLookupCount;
};
