#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadHandler.h"
#include "KadNode.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "../Common/Crypto.h"
#include "FirewallHandler.h"
#include "RoutingRoot.h"
#include "PayloadIndex.h"
#include "LookupManager.h"
#include "KadLookup.h"
#include "KadRouting/KadRelay.h"
#include "KadTask.h"
#include "../Common/FileIO.h"
#include "KadEngine/KadEngine.h"

IMPLEMENT_OBJECT(CKadHandler, CObject)

CKadHandler::CKadHandler(CSmartSocket* pSocket, CObject* pParent)
: CObject(pParent), CSmartSocketInterface(pSocket, "KAD")
{
	m_LastContact = 0;
}

CKadHandler::~CKadHandler()
{
}

////////////////////////////////////////////////////////////////////////
// Hello

void CKadHandler::SendHello(CKadNode* pNode, CComChannel* pChannel, bool bReq)
{
	SKadData* pData = pChannel->GetData<SKadData>();

	CVariant Hello(CVariant::EMap);

	CVariant Addr;
	TMyAddressMap AddressMap = GetParent<CKademlia>()->Fwh()->AddrPool();
	for(TMyAddressMap::const_iterator I = AddressMap.begin(); I != AddressMap.end(); I++)
		Addr.Append(I->second.ToVariant());
	Hello["ADDR"] = Addr;

	pNode->SetLastHello();

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRT"))
		LogLine(LOG_DEBUG, L"Sending 'Hello %s' to %s", bReq ? L"Request" : L"Response", pNode->GetID().ToHex().c_str());
	pChannel->QueuePacket(bReq ? KAD_HELLO_REQUEST : KAD_HELLO_RESPONSE, Hello);
}

void CKadHandler::HandleHello(const CVariant& Hello, CKadNode* pNode, CComChannel* pChannel, bool bReq)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRT"))
		LogLine(LOG_DEBUG, L"Recived 'Hello %s' to %s", bReq ? L"Request" : L"Response", pNode->GetID().ToHex().c_str());

	SKadData* pData = pChannel->GetData<SKadData>();

	CVariant Addr = Hello["ADDR"];
	vector<CNodeAddress> AuthAddress;
	for(uint32 i=0; i < Addr.Count(); i++)
	{
		AuthAddress.push_back(CNodeAddress());
		AuthAddress.back().FromVariant(Addr.At(i));
	}

	// update node addresses
	for(vector<CNodeAddress>::iterator I = AuthAddress.begin(); I != AuthAddress.end(); I++)
	{
		if(pData->Authenticated)
			I->SetVerifyed();
		pNode->UpdateAddress(*I);
	}

	if(bReq)
		SendHello(pNode, pChannel, false);
		
	SetLastContact();
	pNode->UpdateClass(true, pChannel->GetAddress());
}

////////////////////////////////////////////////////////////////////////
// Node

UINT CKadHandler::SendNodeReq(CKadNode* pNode, CComChannel* pChannel, const CUInt128& ID, int iMaxState)
{
	CVariant NodeReq;
	if(ID != 0)
		NodeReq["TID"] = ID;

	NodeReq["RCT"] = GetParent<CKademlia>()->Cfg()->GetInt("NodeReqCount");
	NodeReq["MNC"] = iMaxState;
	
	if(UINT uID = pChannel->QueuePacket(KAD_NODE_REQUEST, NodeReq))
	{
		if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRT"))
			LogLine(LOG_DEBUG, L"Sending 'Node Resuest' to %s", pNode->GetID().ToHex().c_str());
		return uID;
	}
	return 0;
}

void CKadHandler::HandleNodeReq(const CVariant& NodeReq, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRT"))
		LogLine(LOG_DEBUG, L"Recived 'Node Resuest' to %s", pNode->GetID().ToHex().c_str());

	CVariant NodeRes;

	uint32 uDesiredCount = NodeReq["RCT"];
	if(uDesiredCount == 0)
		throw CException(LOG_ERROR, L"node requested 0 nodes");

	int iMaxState = NodeReq.Get("MNC", NODE_2ND_CLASS);

	NodeMap Nodes;
	if(!NodeReq.Has("TID"))
		GetParent<CKademlia>()->Root()->GetBootstrapNodes(GetParent<CKademlia>()->Root()->GetID(), Nodes, uDesiredCount, pChannel->GetAddress().GetProtocol(), iMaxState);
	else
		GetParent<CKademlia>()->Root()->GetClosestNodes(NodeReq["TID"], Nodes, uDesiredCount, pChannel->GetAddress().GetProtocol(), iMaxState);
	
	CVariant List;
	for(NodeMap::iterator I = Nodes.begin(); I != Nodes.end(); I++)
		List.Append(I->second->Store());
	NodeRes["LIST"] = List;

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRT"))
		LogLine(LOG_DEBUG, L"Sending 'Node Response' to %s", pNode->GetID().ToHex().c_str());
	pChannel->QueuePacket(KAD_NODE_RESPONSE, NodeRes);
}

void CKadHandler::HandleNodeRes(const CVariant& NodeRes, CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugRT"))
		LogLine(LOG_DEBUG, L"Recived 'Node Response' from %s", pNode->GetID().ToHex().c_str());

	CVariant List = NodeRes["LIST"];
	if(List.Count() > (uint32)GetParent<CKademlia>()->Cfg()->GetInt("NodeReqCount"))
		throw CException(LOG_ERROR, L"Node returned more nodes than requested (spam)");
	
	SKadData* pData = pChannel->GetData<SKadData>();
	// Note: if pData->pLookup is NULL this was just a bootstrap request

	NodeMap Nodes;
	for(uint32 i = 0; i<List.Count(); i++)
	{
		CPointer<CKadNode> pNewNode = new CKadNode(GetParent<CKademlia>()->Root());
		pNewNode->Load(List.At(i));
		if(GetParent<CKademlia>()->Root()->AddNode(pNewNode) && pData->pLookup)
		{
			CUInt128 uDistance = pData->pLookup->GetID() ^ pNewNode->GetID();
			Nodes.insert(NodeMap::value_type(uDistance, pNewNode));
		}
	}

	if(pData->pLookup)
		GetParent<CKademlia>()->Manager()->AddNodes(pData->pLookup, pNode, pChannel, Nodes);
}

////////////////////////////////////////////////////////////////////////
// 

void CKadHandler::SendInit(CKadNode* pNode, CComChannel* pChannel)
{
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugTL"))
		LogLine(LOG_DEBUG, L"Sent 'Transaction Init' to %s", pNode->GetID().ToHex().c_str());

	CVariant Packet;
	Packet["PROT"] = GetParent<CKademlia>()->GetProtocol();
	Packet["VER"] = GetParent<CKademlia>()->GetVersion();
	Packet["NID"] = GetParent<CKademlia>()->Root()->GetID();
	pChannel->SendPacket(KAD_INIT, Packet); // must always be first - using SendPacket ensures it gets directly into the sending
}

bool CKadHandler::ProcessPacket(const string& Name, const CVariant& Packet, CComChannel* pChannel)
{
	CKadNode* pNode = NULL;
	try
	{
		// Process cahnnel initialisation
		if(Name == KAD_INIT)
		{
			if(GetParent<CKademlia>()->Cfg()->GetBool("DebugTL"))
				LogLine(LOG_DEBUG, L"Recived 'Transaction Init' from %s", CUInt128(Packet["NID"]).ToHex().c_str());

			bool bAdded = false;
			SKadData* pData = pChannel->GetData<SKadData>(&bAdded);
			
			if(!pData->pNode)
			{
				pData->pNode = GetParent<CKademlia>()->Root()->GetNode(Packet["NID"]);
				if(!pData->pNode)
				{
					pData->pNode = new CKadNode(GetParent<CKademlia>()->Root());
					pData->pNode->SetID(Packet["NID"]);
					GetParent<CKademlia>()->Root()->AddNode(pData->pNode);
				}

				pChannel->AddUpLimit(pData->pNode->GetUpLimit());
				pChannel->AddDownLimit(pData->pNode->GetDownLimit());
			}
			else if(pData->pNode->GetID() != Packet["NID"])
				throw CException(LOG_ERROR, L"KadNode Miss Match"); // this should never ever happen!

			pData->pNode->SetProtocol(Packet["PROT"]);
			pData->pNode->SetVersion(Packet.Get("VER")); // Version is optional

			pData->Connected = true;

			if(bAdded) // if it was added it means its an incomming connectziona dnwe have to answer
			{
				SendInit(pData->pNode, pChannel);

				pData->pNode->UpdateAddress(pChannel->GetAddress()); // If this is incomming update addresses
			}
			return true;
		}
		else
		{
			if(SKadData* pData = pChannel->GetData<SKadData>())
				pNode = pData->pNode;
			if(!pNode || !pNode->GetParent())
				throw CException(LOG_WARNING, L"Kad Packet Recived %S on not initialized channel", Name.c_str());
		}

		if(Name.compare(KAD_CRYPTO_REQUEST) == 0)			HandleCryptoRequest(Packet, pNode, pChannel);
		else if(Name.compare(KAD_CRYPTO_RESPONSE) == 0)		HandleCryptoResponse(Packet, pNode, pChannel);

		else if(Name.compare(KAD_HELLO_REQUEST) == 0)		HandleHello(Packet, pNode, pChannel, true);
		else if(Name.compare(KAD_HELLO_RESPONSE) == 0)		HandleHello(Packet, pNode, pChannel, false);

		else if(Name.compare(KAD_NODE_REQUEST) == 0)		HandleNodeReq(Packet, pNode, pChannel);
		else if(Name.compare(KAD_NODE_RESPONSE) == 0)		HandleNodeRes(Packet, pNode, pChannel);

		// Lookup Handling
		else if(Name.compare(KAD_PROXY_REQUEST) == 0)		HandleProxyReq(Packet, pNode, pChannel);
		else if(Name.compare(KAD_PROXY_RESPONSE) == 0)		HandleProxyRes(Packet, pNode, pChannel);

		else if(Name.compare(KAD_CODE_REQUEST) == 0)		HandleCodeReq(Packet, pNode, pChannel);
		else if(Name.compare(KAD_CODE_RESPONSE) == 0)		HandleCodeRes(Packet, pNode, pChannel);

		else if(Name.compare(KAD_LOOKUP_MESSAGE) == 0)		HandleMessagePkt(Packet, pNode, pChannel);

		else if(Name.compare(KAD_EXECUTE_REQUEST) == 0)		HandleExecuteReq(Packet, pNode, pChannel);
		else if(Name.compare(KAD_EXECUTE_RESPONSE) == 0)	HandleExecuteRes(Packet, pNode, pChannel);

		else if(Name.compare(KAD_STORE_REQUEST) == 0)		HandleStoreReq(Packet, pNode, pChannel);
		else if(Name.compare(KAD_STORE_RESPONSE) == 0)		HandleStoreRes(Packet, pNode, pChannel);

		else if(Name.compare(KAD_LOAD_REQUEST) == 0)		HandleLoadReq(Packet, pNode, pChannel);
		else if(Name.compare(KAD_LOAD_RESPONSE) == 0)		HandleLoadRes(Packet, pNode, pChannel);

		else if(Name.compare(KAD_LOOKUP_REPORT) == 0)		HandleReportPkt(Packet, pNode, pChannel);

		// Routing Handling
		else if(Name.compare(KAD_ROUTE_REQUEST) == 0)		HandleRouteReq(Packet, pNode, pChannel);
		else if(Name.compare(KAD_ROUTE_RESPONSE) == 0)		HandleRouteRes(Packet, pNode, pChannel);

		else if(Name.compare(KAD_RELAY_REQUEST) == 0)		HandleRelayReq(Packet, pNode, pChannel);
		else if(Name.compare(KAD_RELAY_RESPONSE) == 0)		HandleRelayRes(Packet, pNode, pChannel);
		else if(Name.compare(KAD_RELAY_RETURN) == 0)		HandleRelayRet(Packet, pNode, pChannel);

		else if(Name.compare(KAD_RELAY_CONTROL) == 0)		HandleRelayCtrl(Packet, pNode, pChannel);
		else if(Name.compare(KAD_RELAY_STATUS) == 0)		HandleRelayStat(Packet, pNode, pChannel);

		else 
			throw CException(LOG_WARNING, L"Unsupported Kad Packet Recived %S", Name.c_str());
	}
	catch(const CException& Exception)
	{
		LogLine(Exception.GetFlag(), L"Packet \'%S\' Error: '%s' from: %s (%s)", Name.c_str(), Exception.GetLine().c_str(), pNode ? pNode->GetID().ToHex().c_str() : L"Unknown", pChannel->GetAddress().ToString().c_str());
		pChannel->Close();
		return false;
	}
	return true;
}

void CKadHandler::Process(UINT Tick)
{
	// check out all open channels
	for(set<SKadNode>::iterator I = m_Nodes.begin(); I != m_Nodes.end();)
	{
		// Note: a sub node is not allowed to disconnect on its during a lookup, if it does so its considdered a failure
		if(I->pChannel->IsDisconnected())
		{
			SKadData* pData = I->pChannel->GetData<SKadData>();
			if(!pData->Connected)
				I->pNode->UpdateClass(false, I->pChannel->GetAddress());
			I = m_Nodes.erase(I);
		}
		else 
			I++;
	}

	for(TExchangeMap::iterator I = m_KeyExchanges.begin(); I != m_KeyExchanges.end();)
	{
		CPointer<CKadNode> pNode = I->first.pNode;
		for(; I != m_KeyExchanges.end() && I->first.pNode == pNode; )
		{
			if(I->first.pChannel->IsDisconnected())
				I = m_KeyExchanges.erase(I);
			else 
				I++;
		}
	}
}

bool CKadHandler::CheckoutNode(CKadNode* pNode)
{
	set<SKadNode>::iterator I = m_Nodes.find(SKadNode(pNode));
	// Note: we should not come here if we already opened the channel
	if(I != m_Nodes.end())
		return false; // already in progress

	CComChannel* pChannel = PrepareChannel(pNode);
	if(!pChannel)
		return false; // failes
	I = m_Nodes.insert(SKadNode(pNode, pChannel)).first;

	SendHello(pNode, pChannel);
	return true; // sent
}

CComChannel* CKadHandler::PrepareChannel(CKadNode* pNode)
{
	map<CSafeAddress::EProtocol, const CNodeAddress*> AddrMap;
	for(AddressMap::const_iterator J = pNode->GetAddresses().begin(); J != pNode->GetAddresses().end(); J++)
	{
		if(J->second.IsBlocked())
			continue;

		const CNodeAddress* &pAddress = AddrMap[J->first];
		if(!pAddress) {
			pAddress = &J->second;
			continue;
		}

		bool v; // always prefer verifyed addresses
		if((v = J->second.IsVerifyed()) != (pAddress->IsVerifyed()))
		{
			if(v) pAddress = &J->second;
			continue;
		}

		bool a; // always prefer unfirewalled addresses
		if((a = (J->second.GetAssistent() != NULL)) != (pAddress->GetAssistent() != NULL))
		{
			if(!a) pAddress = &J->second;
			continue;
		}

		// always pick most reliable address
		if(J->second.GetClass() < pAddress->GetClass())
			pAddress = &J->second;
	}

	const CNodeAddress* pAddress = NULL;
	// Note: We take the IPv6 if it is the only one or if it is not firewalled, and we know we support it, else we stick to IPv4
	const CNodeAddress* pAddressV4 = AddrMap[CSafeAddress::eUTP_IP4];
	const CNodeAddress* pAddressV6 = AddrMap[CSafeAddress::eUTP_IP6];
	bool bIPv6 = GetParent<CKademlia>()->Fwh()->AddrPool().count(CSafeAddress::eUTP_IP6) > 0;
	if(!pAddressV4 || (pAddressV6 && bIPv6 && !pAddressV6->GetAssistent()))
		pAddress = pAddressV6;
	else
		pAddress = pAddressV4;
	
	if(!pAddress)
		return NULL;
	return PrepareChannel(*pAddress, pNode);
}

CComChannel* CKadHandler::PrepareChannel(const CNodeAddress& Address, CKadNode* pNode)
{
	CComChannel* pChannel = NULL;
	if(CSafeAddress* pAddress = Address.GetAssistent()) // if node is firewalled
	{
		// Note: we can not send a direct packet we must always stream
		// we dont want to use assistant nodes to relay packets, its enough thay broker the connection for us

		if(!pAddress->IsValid())
			return NULL; //node dont have an assistant and is not directly reachable, we cant communicate right now

		const CMyAddress* pMyAddress = GetParent<CKademlia>()->Fwh()->GetAddress(Address.GetProtocol());
		if(pMyAddress->GetAssistent()) // and we are two
		{
			GetParent<CKademlia>()->Fwh()->SendTunnel(Address, false);
			if(pAddress->GetPort() == 0)
				return NULL; // node can't tunnel, we could only use a callback if we ware open, but we are closed, so we cant communicate at all
			pChannel = Rendevouz(Address); 
		}
		else
		{
			GetParent<CKademlia>()->Fwh()->SendTunnel(Address, true);
			pChannel = CallBack(Address, false);
		}
	}
	else
		pChannel = Connect(Address);
	
	if(pChannel)
	{
		bool bAdded = false;
		SKadData* pData = pChannel->GetData<SKadData>(&bAdded);
		ASSERT(bAdded);

		pData->pNode = pNode;
		pChannel->AddUpLimit(pNode->GetUpLimit());
		pChannel->AddDownLimit(pNode->GetDownLimit());

		SendInit(pNode, pChannel);
		
		// Encrypt and authenticate all channels by default
		bool bEncrypt = true;
		bool bAuthenticate = true;
		pChannel->SetQueueLock(true);

		m_KeyExchanges.insert(TExchangeMap::value_type(SKadNode(pNode, pChannel), SKeyExchange(bEncrypt, bAuthenticate, 0)));
		ResumeExchange(pNode);
	}
	return pChannel;
}

//void CKadHandler::EncryptChannel(CComChannel* pChannel)
//{
//	SKadData* pData = pChannel->GetData<SKadData>();
//	ASSERT(pData->pNode);
//	pChannel->SetQueueLock(true); // this locks the packet queue such that nothing can pass
//
//	SendCryptoRequest(pData->pNode, pChannel);
//}

void CKadHandler::SendCryptoRequest(CKadNode* pNode, CComChannel* pChannel, SKeyExchange* Key, CTrustedKey* pTrustedKey)
{
	CVariant Request(CVariant::EMap);

	if(pTrustedKey)
	{
		Request["FP"] = pTrustedKey->GetFingerPrint();

		Key->pExchange = new CKeyExchange(CAbstractKey::eNone, NULL);
		Key->pExchange->SetKey(pTrustedKey->GetKey(), pTrustedKey->GetSize());
	}
	else
	{
		string Param;
		UINT eExAlgorithm = PrepKA(Key->eAlgorithm, Param);
		Key->pExchange = NewKeyExchange(eExAlgorithm, Param);
		if(Key->eAlgorithm != 0)
			Request["KA"] = CAbstractKey::Algorithm2Str(eExAlgorithm) + "-" + Param;

		CScoped<CAbstractKey> pKey = Key->pExchange->InitialsieKeyExchange();
		Request["EK"] = CVariant(pKey->GetKey(), pKey->GetSize());
	}

	if(Key->bEncrypt)
	{
		UINT eSymAlgorithm = PrepSC(Key->eAlgorithm);
		CAbstractKey* pIV = new CAbstractKey(KEY_128BIT, true);
		pIV->SetAlgorithm(eSymAlgorithm);
		if(Key->eAlgorithm)
			Request["SC"] = CAbstractKey::Algorithm2Str(eSymAlgorithm);

		Key->pExchange->SetIV(pIV);
		Request["IV"] = CVariant(Key->pExchange->GetIV()->GetKey(), Key->pExchange->GetIV()->GetSize());
	}

	if(Key->bAuthenticate && !pTrustedKey) // if we have a preauthenticated keywe can skip authentication
	{
		const CMyKadID& MyID = GetParent<CKademlia>()->Root()->GetID();
		Request["PK"] = CVariant(MyID.GetKey()->GetKey(), MyID.GetKey()->GetSize());
		if((MyID.GetKey()->GetAlgorithm() & CAbstractKey::eHashFunkt) != 0)
			Request["HK"] = CAbstractKey::Algorithm2Str((MyID.GetKey()->GetAlgorithm() & CAbstractKey::eHashFunkt));

		Request.Sign(MyID.GetPrivateKey());
	}

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugTL"))
		LogLine(LOG_DEBUG, L"Sending 'CryptoRequest' to %s", pNode->GetID().ToHex().c_str());
	pChannel->SendPacket(KAD_CRYPTO_REQUEST, Request);
}

void CKadHandler::HandleCryptoRequest(const CVariant& Request, CKadNode* pNode, CComChannel* pChannel)
{
	SKadData* pData = pChannel->GetData<SKadData>();
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugTL"))
		LogLine(LOG_DEBUG, L"Recived 'CryptoRequest' to %s", pNode->GetID().ToHex().c_str());

	CVariant Response(CVariant::EMap);

	CScoped<CSymmetricKey> pCryptoKey;
	try
	{
		CScoped<CAbstractKey> pSessionKey;
		if(Request.Has("EK"))
		{
			UINT eExAlgorithm = 0;
			string Param;
			if(Request.Has("KA"))
				eExAlgorithm = ParseKA(Request["KA"], Param);
			UINT eAlgorithm = PrepKA(eExAlgorithm, Param);
			CScoped<CKeyExchange> pKeyExchange = NewKeyExchange(eAlgorithm, Param);

			CScoped<CAbstractKey> pKey = pKeyExchange->InitialsieKeyExchange();
			Response["EK"] = CVariant(pKey->GetKey(), pKey->GetSize());

			CScoped<CAbstractKey> pRemKey = new CAbstractKey(Request["EK"].GetData(), Request["EK"].GetSize());
			pSessionKey = pKeyExchange->FinaliseKeyExchange(pRemKey);
			if(!pSessionKey)
				throw "ExchangeFailed";

			bool bAuthenticated = Request.IsSigned();
			if(bAuthenticated)
			{
				CPublicKey* pPubKey;
				if(Request.Has("PK"))
					pPubKey = pNode->SetIDKey(Request);
				else
					pPubKey = pNode->GetID().GetKey();
			
				if(!pPubKey)
					throw "MissingKey";

				if(!Request.Verify(pPubKey))
					throw "InvalidSign";
			}

			pData->Authenticated = bAuthenticated;
			pNode->SetTrustedKey(new CTrustedKey(pSessionKey->GetKey(), pSessionKey->GetSize(), bAuthenticated, eAlgorithm & CAbstractKey::eHashFunkt));
		}
		else if(Request.Has("FP"))
		{
			CTrustedKey* pTrustedKey = pNode->GetTrustedKey();
			if(!pTrustedKey || pTrustedKey->GetFingerPrint() != Request["FP"].To<uint64>())
				throw "UnknownKey";

			pData->Authenticated = pTrustedKey->IsAuthenticated();
			pSessionKey = new CAbstractKey(pTrustedKey->GetKey(), pTrustedKey->GetSize());
		}
		else
			throw "UnknownMethod";

		if(Request.Has("IV"))
		{
			CAbstractKey OutIV(KEY_128BIT, true);
			Response["IV"] = CVariant(OutIV.GetKey(), OutIV.GetSize());

			UINT eSymAlgorithm = 0;
			if(Request.Has("SC"))
				eSymAlgorithm = ParseSC(Request["SC"]);
			UINT eAlgorithm = PrepSC(eSymAlgorithm);

			CAbstractKey InIV(Request["IV"].GetData(), Request["IV"].GetSize());

			pCryptoKey = NewSymmetricKey(eAlgorithm, pSessionKey, &InIV, &OutIV);
		}

		if(Request.IsSigned() && pData->Authenticated)
		{
			const CMyKadID& MyID = GetParent<CKademlia>()->Root()->GetID();
			Response["PK"] = CVariant(MyID.GetKey()->GetKey(), MyID.GetKey()->GetSize());
			if((MyID.GetKey()->GetAlgorithm() & CAbstractKey::eHashFunkt) != 0)
				Response["HK"] = CAbstractKey::Algorithm2Str((MyID.GetKey()->GetAlgorithm() & CAbstractKey::eHashFunkt));
		
			Response.Sign(MyID.GetPrivateKey());
		}
	}
	catch(const char* Error)
	{
		delete pCryptoKey.Detache();
		Response["ERR"] = Error;
	}

	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugTL"))
		LogLine(LOG_DEBUG, L"Sending 'CryptoResponse' to %s", pNode->GetID().ToHex().c_str());
	pChannel->SendPacket(KAD_CRYPTO_RESPONSE, Response);

	if(pCryptoKey)
	{
		pChannel->SetQueueLock(true);
		pChannel->Encrypt(pCryptoKey.Detache());
		pChannel->SetQueueLock(false);
	}
}

void CKadHandler::HandleCryptoResponse(const CVariant& Response, CKadNode* pNode, CComChannel* pChannel)
{
	SKadData* pData = pChannel->GetData<SKadData>();
	if(GetParent<CKademlia>()->Cfg()->GetBool("DebugTL"))
		LogLine(LOG_DEBUG, L"Recived 'CryptoResponse' to %s", pNode->GetID().ToHex().c_str());

	SKeyExchange* Key = NULL;
	TExchangeMap::iterator I = m_KeyExchanges.find(SKadNode(pNode));
	for(; I != m_KeyExchanges.end() && I->first.pNode == pNode; I++)
	{
		if(I->first.pChannel == pChannel)
		{
			Key = &I->second;
			break;
		}
	}
	if(!Key)
		throw CException(LOG_ERROR | LOG_DEBUG, L"Unsolicited Crypto Response");

	CScoped<CSymmetricKey> pCryptoKey;
	try
	{
		if(Response.Has("ERR"))
		{
			string Error = Response["ERR"];
			if(Error == "UnknownKey")
			{
				pNode->SetTrustedKey(NULL);
				Key->pExchange = NULL;
				ResumeExchange(I->first.pNode);
				return;
			}
			else
				throw CException(LOG_ERROR | LOG_DEBUG, L"Crypto Response Returned Error: %S", Error.c_str());
		}

		CScoped<CAbstractKey> pSessionKey;
		if(Key->pExchange->GetAlgorithm() & CAbstractKey::eKeyExchange)
		{
			if(!Response.Has("EK"))
				throw "MissingEK";
			CScoped<CAbstractKey> pRemKey = new CAbstractKey(Response["EK"].GetData(), Response["EK"].GetSize());
			pSessionKey = Key->pExchange->FinaliseKeyExchange(pRemKey);
			if(!pSessionKey)
				throw "ExchangeFailed";

			if(Key->bAuthenticate)
			{
				CPublicKey* pPubKey;
				if(Response.Has("PK")) 
					pPubKey = pNode->SetIDKey(Response);
				else
					pPubKey = pNode->GetID().GetKey();

				if(!pPubKey)
					throw "MissingKey";

				if(!Response.Verify(pPubKey))
					throw "InvalidSign";
			}

			pData->Authenticated = Key->bAuthenticate;
			pNode->SetTrustedKey(new CTrustedKey(pSessionKey->GetKey(), pSessionKey->GetSize(), Key->bAuthenticate));

			ResumeExchange(I->first.pNode);
		}
		else
		{
			pData->Authenticated = Key->bAuthenticate;
			pSessionKey = Key->pExchange->FinaliseKeyExchange(NULL);
		}

		if(Response.Has("IV"))
		{
			UINT eAlgorithm = PrepSC(Key->eAlgorithm); // UINT eAlgorithm = PrepSC(Key->pExchange->GetIV()->GetAlgorithm());

			CAbstractKey InIV(Response["IV"].GetData(), Response["IV"].GetSize());

			pCryptoKey = NewSymmetricKey(eAlgorithm, pSessionKey, &InIV, Key->pExchange->GetIV());
		}
	}
	catch(const char* Error)
	{
		throw CException(LOG_ERROR | LOG_DEBUG, L"Crypto Response Error: %S", Error);
	}

	m_KeyExchanges.erase(I);

	if(pCryptoKey)
	{
		pChannel->Encrypt(pCryptoKey.Detache());
		pChannel->SetQueueLock(false); // release the packet queue lock such that we can start sending encrypted payload packets
	}
}

bool CKadHandler::ExchangePending(CKadNode* pNode)
{
	TExchangeMap::iterator X = m_KeyExchanges.find(SKadNode(pNode));
	return X != m_KeyExchanges.end();
}

void CKadHandler::ResumeExchange(CPointer<CKadNode> pNode)
{
	TExchangeMap::iterator X = m_KeyExchanges.find(SKadNode(pNode));
	if(CTrustedKey* pTrustedKey = pNode->GetTrustedKey())
	{
		TExchangeMap::iterator I = X;
		X = m_KeyExchanges.end();
		for(; I != m_KeyExchanges.end() && I->first.pNode == pNode; I++) // for each exchange for this node
		{
			if(I->second.pExchange)
				continue;

			if(pTrustedKey->IsAuthenticated() || !I->second.bAuthenticate) // the key we know is eider authenticated or we dont need authentication
				SendCryptoRequest(I->first.pNode, I->first.pChannel, &I->second, pTrustedKey); // try to activate this key
			else if(X == m_KeyExchanges.end())
				X = I;
		}
	}
	if(X != m_KeyExchanges.end() && X->second.pExchange == NULL)
		SendCryptoRequest(X->first.pNode, X->first.pChannel, &X->second, NULL);
}

//CMyAddress CKadHandler::SelectMyAddress(CSafeAddress::EProtocol eProtocol)
//{
//	TMyAddressMap MyAddressMap = GetParent<CKademlia>()->Fwh()->AddrPool();
//	TMyAddressMap::iterator I = MyAddressMap.find(eProtocol);
//	if(I != MyAddressMap.end())
//		return I->second;
//	return CMyAddress();
//}
