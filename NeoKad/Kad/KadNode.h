#pragma once

#include "KadID.h"
#include "NodeAddress.h"
#include "../Networking/BandwidthControl/BandwidthLimit.h"

typedef multimap<CSafeAddress::EProtocol, CNodeAddress> AddressMap;

class CTrustedKey: public CAbstractKey
{
public:
	CTrustedKey(byte* pKey, size_t uSize, bool bAuthenticated, UINT eAlgorithm = CAbstractKey::eUndefined);

	bool		IsAuthenticated()	{return m_bAuthenticated;}
	uint64		GetFingerPrint()	{return m_FingerPrint;}

protected:
	bool		m_bAuthenticated;
	uint64		m_FingerPrint;
};

class CKadNode: public CObject
{
public:
	DECLARE_OBJECT(CKadNode)

	CKadNode(CObject* pParent = NULL);
	~CKadNode() {}

	void					SetProtocol(const uint32& Protocol) {m_Protocol = Protocol;}
	uint32					GetProtocol()				{return m_Protocol;}

	void					SetVersion(const string& Version) {m_Version = Version;}
	const string&			GetVersion()				{return m_Version;}

	void					SetID(const CUInt128 &ID)	{m_ID = ID;}
	CKadID&					GetID()						{return m_ID;}
	const CKadID&			GetID() const				{return m_ID;}

	void					Load(const CVariant& Node, bool bFull = false);
	CVariant				Store(bool bFull = true) const;

	CPublicKey*				SetIDKey(const CVariant& Info);

	void					Merge(CKadNode* pNode);

	void					UpdateAddress(const CNodeAddress& Address);
	void					RemoveAddress(const CNodeAddress& Address);
	const AddressMap&		GetAddresses() const {return m_AddressMap;}
	CNodeAddress			GetAddress(CSafeAddress::EProtocol Protocol) const;
	bool 					CheckAddress(const CNodeAddress& Address);

	void					UpdateClass(bool bOk, const CNodeAddress& Address);
	int						GetClass() const;
	time_t					GetTimeToKeep() const;
	bool					IsNeeded() const;
	bool					IsFading(bool bHalf = false) const;

	void					IncrFailed()		{m_iFailed++;}
	bool					HasFailed() const	{return m_iFailed > 3;}

	void					SetTrustedKey(const CHolder<CTrustedKey>& pKey)	{m_pTrustedKey = pKey;}
	CHolder<CTrustedKey>&	GetTrustedKey()		{return m_pTrustedKey;}

	void					SetLastHello() {m_uLastHello = GetTime();}
	uint64					GetLastHello() {return m_uLastHello;}
	uint64					GetLastSeen() {return m_uLastSeen;}

	uint64					GetPassKey() const;

	CBandwidthLimit*		GetUpLimit()		{return m_UpLimit;}
	CBandwidthLimit*		GetDownLimit()		{return m_DownLimit;}

protected:
	void					InsertAddress(const CNodeAddress& Address);

	string					m_Version;
	uint32					m_Protocol;
	CKadID					m_ID;

	CHolder<CTrustedKey>	m_pTrustedKey;

	AddressMap				m_AddressMap;

	int						m_iFailed;
	time_t					m_uFirstSeen;
	time_t					m_uLastSeen;
	time_t					m_uLastHello;

	CBandwidthLimit*		m_UpLimit;
	CBandwidthLimit*		m_DownLimit;
};


struct SKadNode
{
	SKadNode() {}
	explicit SKadNode(CKadNode* pNode)				{this->pNode = pNode;}
	explicit SKadNode(CKadNode* pNode, CComChannel* pChannel) {this->pNode = pNode; this->pChannel = pChannel;}

	void					Clear()					{pNode = NULL; pChannel = NULL;}

	// this must be usable as a Map Key, channel is irrelevant
	bool operator<  (const SKadNode &Otehr) const	{return pNode < Otehr.pNode;}
	bool operator== (const SKadNode &Otehr) const	{return pNode == Otehr.pNode;}

	CPointer<CKadNode>				pNode;
	mutable CPointer<CComChannel>	pChannel;
};