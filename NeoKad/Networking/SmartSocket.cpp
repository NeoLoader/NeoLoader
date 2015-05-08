#include "GlobalHeader.h"
#include "../Framework/Strings.h"
#include "SmartSocket.h"
#include "SocketSession.h"
#include "./BandwidthControl/BandwidthLimit.h"
#include "./BandwidthControl/BandwidthManager.h"

IMPLEMENT_OBJECT(CComChannel, CObject)

IMPLEMENT_OBJECT(CSmartSocket, CObject)

CSmartSocket::CSmartSocket(uint64 KeepAlive, CObject* pParent)
: CObject(pParent)
{
	m_KeepAlive = KeepAlive;
	m_RecvKey = 0;
	m_pPrivKey = NULL;

	m_UpManager = new CBandwidthManager(CBandwidthLimiter::eUpChannel, this);
	m_UpLimit = new CBandwidthLimit(this);
	m_DownManager = new CBandwidthManager(CBandwidthLimiter::eDownChannel , this);
	m_DownLimit = new CBandwidthLimit(this);
}

CSmartSocket::~CSmartSocket()
{
	// Cleanup manually for bandwidth cotnroll
	for(ListnerMap::iterator I = m_Listners.begin(); I != m_Listners.end(); I++)
		delete I->second;
}

void CSmartSocket::SetupCrypto(uint64 RecvKey, CPrivateKey* pPrivKey)
{
	m_RecvKey = RecvKey;
	m_pPrivKey = pPrivKey;
	m_pPubKey = pPrivKey->PublicKey();
}

void CSmartSocket::Process(bool bSecond)
{
	m_UpManager->Process();
	m_DownManager->Process();

	for(ListnerMap::iterator I = m_Listners.begin(); I != m_Listners.end(); I++)
		I->second->Process();

	for(list<CPointer<CSocketSession> >::iterator I = m_Sessions.begin(); I != m_Sessions.end();)
	{
		CPointer<CSocketSession> &pSession = *I;
		pSession->Process();
		// if there are no more strong pointers and the timeout was reached, terminate the session for good
		if((pSession.IsLast() && pSession->GetIdleTime() > m_KeepAlive) || !pSession->IsValid()) 
			I = m_Sessions.erase(I);
		else
			I++;
	}
}

void CSmartSocket::ProcessPacket(const string& Name, const CVariant& Packet, CComChannel* pChannel)
{
	string::size_type pos = Name.find(L':');
	string Prefix = Name.substr(0, pos);
	InterfaceMap::iterator I = m_Interfaces.find(Prefix);
	if(I == m_Interfaces.end())
		LogLine(LOG_ERROR, L"unsupported prefix: %S; from: %s", Name.c_str(), pChannel->GetAddress().ToString().c_str());
	else
		I->second->ProcessPacket(Name, Packet, pChannel);
}

CSocketListner* CSmartSocket::GetListner(const CSafeAddress& Address)
{
	ListnerMap::iterator I = m_Listners.find(Address.GetProtocol());
	if(I == m_Listners.end())
		return NULL;
	return I->second;
}

void CSmartSocket::InstallListener(CSocketListner* pListener)
{
	m_Listners.insert(ListnerMap::value_type(pListener->GetProtocol(), pListener));
}

void CSmartSocket::RegisterInterface(CSmartSocketInterface* pInterface, const string& Prefix)
{
	m_Interfaces.insert(InterfaceMap::value_type(Prefix, pInterface));
}

void CSmartSocket::UnregisterInterface(CSmartSocketInterface* pInterface)
{
	for(InterfaceMap::iterator I = m_Interfaces.begin(); I != m_Interfaces.end(); I++)
	{
		if(I->second == pInterface)
		{
			m_Interfaces.erase(I);
			return;
		}
	}
	ASSERT(0);
}

list<CSafeAddress::EProtocol> CSmartSocket::GetProtocols()
{
	list<CSafeAddress::EProtocol> Protocols;
	for(ListnerMap::iterator I = m_Listners.begin(); I != m_Listners.end(); I++)
		Protocols.push_back(I->first);
	return Protocols;
}

CComChannel* CSmartSocket::NewChannel(const CSafeAddress& Address, bool bDirect, bool bRendevouz, bool bEmpty)
{
	if(bDirect)
	{
		ListnerMap::iterator L = m_Listners.find(Address.GetProtocol());
		if(L == m_Listners.end())
			return NULL;
		return new CMessageSession(L->second, Address);
	}
	
	CSocketListner* pListener = GetListner(Address);
	if(!pListener)
		return NULL;

	CPointer<CSocketSession> pSession = pListener->CreateSession(Address, bRendevouz, bEmpty);
	if(!pSession)
		return NULL;

	m_Sessions.push_back(pSession);
	return pSession;		
}

list<CSocketSession*> CSmartSocket::GetChannels(const CSafeAddress& Address, bool bIgnorePort)
{
	list<CSocketSession*> Channels;
	for(list<CPointer<CSocketSession> >::iterator I = m_Sessions.begin(); I != m_Sessions.end(); I++)
	{
		CPointer<CSocketSession> &pSession = *I;
		if(Address.Compare(pSession->GetAddress(), bIgnorePort) == 0)
			Channels.push_back(pSession);
	}
	return Channels;
}
