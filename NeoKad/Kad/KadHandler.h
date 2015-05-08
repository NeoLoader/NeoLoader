#pragma once

class CAbstractKadSocket;
class CKademlia;
class CKadID;
class CRoutingRoot;
class CFloodDetection;
class CEscrowManager;
class CKadNode;
class CKadRoute;
class CKadHandler;
class CKadRelay;
class CLookupProxy;
class CKadOperation;
class CKadScript;

#include "KadNode.h"
#include "KadLookup.h"
#include "../Networking/SmartSocket.h"
#include "../Networking/SocketSession.h"
#include "../../Framework/Cryptography/KeyExchange.h"

class CNodeAddress;
class CMyAddress;
class CTrustedKey;

struct SKadData: SComData
{
	SKadData()
	{
		Connected = false;
		Authenticated = false;
	}

	CPointer<CKadNode>	pNode;
	CPointer<CKadLookup>pLookup;
	bool				Connected;
	bool				Authenticated;

	list<pair<string, CVariant> > DelayedPackets;
};

struct SKeyExchange
{
	SKeyExchange(bool Encrypt, bool Authenticate, UINT Algorithm)
		: bEncrypt(Encrypt), bAuthenticate(Authenticate), eAlgorithm(Algorithm) {}
	bool					bEncrypt;
	bool					bAuthenticate;
	UINT					eAlgorithm;
	CHolder<CKeyExchange>	pExchange;
};

class CKadHandler: public CObject, public CSmartSocketInterface
{
public:
	DECLARE_OBJECT(CKadHandler)

	CKadHandler(CSmartSocket* pSocket, CObject* pParent = NULL);
	virtual ~CKadHandler();

	virtual void			SendCryptoRequest(CKadNode* pNode, CComChannel* pChannel, SKeyExchange* Key, CTrustedKey* pTrustedKey);
	virtual void			HandleCryptoRequest(const CVariant& Request, CKadNode* pNode, CComChannel* pChannel);
	virtual void			HandleCryptoResponse(const CVariant& Response, CKadNode* pNode, CComChannel* pChannel);

	virtual void			SendHello(CKadNode* pNode, CComChannel* pChannel, bool bReq = true);
	virtual void			HandleHello(const CVariant& Hello, CKadNode* pNode, CComChannel* pChannel, bool bReq);

	virtual UINT			SendNodeReq(CKadNode* pNode, CComChannel* pChannel, const CUInt128& ID, int iMaxState = NODE_2ND_CLASS);
	virtual void			HandleNodeReq(const CVariant& NodeReq, CKadNode* pNode, CComChannel* pChannel);
	virtual void			HandleNodeRes(const CVariant& NodeRes, CKadNode* pNode, CComChannel* pChannel);

	//
	// Lookup Handling
	//

	virtual UINT			SendProxyReq(CKadNode* pNode, CComChannel* pChannel, CKadOperation* pLookup, int SpreadShare, const CVariant &InitParam);
	virtual void			HandleProxyReq(const CVariant& LookupReq, CKadNode* pNode, CComChannel* pChannel, bool bDelayed = false);
	virtual void			HandleProxyRes(const CVariant& LookupRes, CKadNode* pNode, CComChannel* pChannel);

	virtual UINT			SendCodeReq(CKadNode* pNode, CComChannel* pChannel, const CVariant& CID);
	virtual void			HandleCodeReq(const CVariant& LookupReq, CKadNode* pNode, CComChannel* pChannel);
	virtual void			HandleCodeRes(const CVariant& LookupRes, CKadNode* pNode, CComChannel* pChannel);

	virtual UINT			SendMessagePkt(CKadNode* pNode, CComChannel* pChannel, const string& Name, const CVariant& Data);
	virtual void			HandleMessagePkt(const CVariant& Packet, CKadNode* pNode, CComChannel* pChannel);

	virtual UINT			SendExecuteReq(CKadNode* pNode, CComChannel* pChannel, CKadOperation* pLookup, const CVariant& Requests, bool bStateless);
	virtual void			HandleExecuteReq(const CVariant& LookupReq, CKadNode* pNode, CComChannel* pChannel, bool bDelayed = false);
	virtual bool			SendExecuteRes(CLookupProxy* pProxy, const CVariant& Results);
	virtual void			HandleExecuteRes(const CVariant& LookupRes, CKadNode* pNode, CComChannel* pChannel);

	virtual UINT			SendStoreReq(CKadNode* pNode, CComChannel* pChannel, CKadOperation* pLookup, const CVariant& Payload, bool bStateless);
	virtual void			HandleStoreReq(const CVariant& LookupReq, CKadNode* pNode, CComChannel* pChannel);
	virtual void			HandleStoreRes(const CVariant& LookupRes, CKadNode* pNode, CComChannel* pChannel);

	virtual UINT			SendLoadReq(CKadNode* pNode, CComChannel* pChannel, CKadOperation* pLookup, const CVariant& Request, bool bStateless);
	virtual void			HandleLoadReq(const CVariant& LookupReq, CKadNode* pNode, CComChannel* pChannel);
	virtual void			HandleLoadRes(const CVariant& LookupRes, CKadNode* pNode, CComChannel* pChannel);

	virtual void			SendReportPkt(CComChannel* pChannel, UINT Flags, const wstring& Line, const string& Error = "", const CUInt128& NID = 0);
	virtual void			HandleReportPkt(const CVariant& Packet, CKadNode* pNode, CComChannel* pChannel);

	//
	// Routing Handling
	//

	virtual UINT			SendRouteReq(CKadNode* pNode, CComChannel* pChannel, CKadRelay* pRelay, bool bRefresh = false);
	virtual void			HandleRouteReq(const CVariant& RouteReq, CKadNode* pNode, CComChannel* pChannel);
	virtual void			HandleRouteRes(const CVariant& RouteRes, CKadNode* pNode, CComChannel* pChannel);

	virtual bool			SendRelayReq(CKadNode* pNode, CComChannel* pChannel, const CVariant& Frame, uint64 TTL, CKadRelay* pRelay);
	virtual void			HandleRelayReq(const CVariant& RelayReq, CKadNode* pNode, CComChannel* pChannel);
	virtual bool			SendRelayRes(CKadNode* pNode, CComChannel* pChannel, const CVariant& Frame, const string& Error = "", const CVariant& Load = CVariant());
	virtual void			HandleRelayRes(const CVariant& RelayRes, CKadNode* pNode, CComChannel* pChannel);
	virtual bool			SendRelayRet(CKadNode* pNode, CComChannel* pChannel, const CVariant& Ack);
	virtual void			HandleRelayRet(const CVariant& RelayRet, CKadNode* pNode, CComChannel* pChannel);

	virtual void			SendRelayCtrl(CKadRelay* pRelay, const CVariant& Control);
	virtual void			HandleRelayCtrl(const CVariant& ControlReq, CKadNode* pNode, CComChannel* pChannel);
	virtual void			SendRelayStat(CKadRelay* pRelay, const CVariant& StatusReq);
	virtual void			HandleRelayStat(const CVariant& StatusRes, CKadNode* pNode, CComChannel* pChannel);


	virtual void			Process(UINT Tick);
	virtual	bool			ProcessPacket(const string& Name, const CVariant& Packet, CComChannel* pChannel);

	virtual bool			CheckoutNode(CKadNode* pNode);

	virtual bool			ExchangePending(CKadNode* pNode);

	virtual CComChannel*	PrepareChannel(CKadNode* pNode);
	//virtual	void			EncryptChannel(CComChannel* pChannel);

	virtual void			SetLastContact(time_t LastContact = 0)		{m_LastContact = LastContact ? LastContact : GetTime();}
	virtual time_t			GetLastContact()							{return m_LastContact;}

protected:
	virtual void			SendInit(CKadNode* pNode, CComChannel* pChannel);

	virtual CKadScript*		GetKadScript(const CVariant& LookupReq);
	virtual bool			SetKadScript(CKadOperation* pLookup, CVariant& LookupReq);

	virtual CComChannel*	PrepareChannel(const CNodeAddress& Address, CKadNode* pNode);
	//virtual CMyAddress		SelectMyAddress(CSafeAddress::EProtocol eProtocol);
	virtual void			ResumeExchange(CPointer<CKadNode> pNode);

	set<SKadNode>			m_Nodes;
	typedef multimap<SKadNode, SKeyExchange> TExchangeMap;
	TExchangeMap			m_KeyExchanges;

	time_t					m_LastContact;
};

