#pragma once

#include "../Common/Object.h"
#include "../Common/Variant.h"
#include "SafeAddress.h"
#include "../Framework/Scope.h"

class CSmartSocketInterface;
class CSocketSession;
class CSocketListner;
class CPrivateKey;
class CPublicKey;
class CBandwidthManager;
class CBandwidthLimit;
class CComChannel;

struct SComData
{
	SComData() {}
	virtual ~SComData() {}
};

class CComChannel: public CObject
{
public:
	DECLARE_OBJECT(CComChannel)
	CComChannel(CObject* pParent = NULL) : CObject(pParent) {m_pComData = NULL;}
	~CComChannel() {delete m_pComData;}

	virtual const CSafeAddress&		GetAddress() const = 0;

	virtual void					Close() = 0;

	virtual UINT					QueuePacket(const string& Name, const CVariant& Packet, int iPriority = 0/*, uint32 uTTL = -1*/) = 0;
	virtual bool					IsQueued(UINT uID) const = 0;
	virtual void					SendPacket(const string& Name, const CVariant& Packet) = 0;

	virtual void					SetQueueLock(bool bSet) = 0;
	virtual void					Encrypt(CSymmetricKey* pCryptoKey) = 0;
	//virtual bool					Encrypt(CPublicKey* pPubKey = NULL, UINT eAlgorithm = 0) = 0;
	virtual bool					IsEncrypted() const = 0;
	virtual bool					IsConnected() const = 0;
	virtual bool					IsBussy() const = 0;
	virtual bool					IsDisconnected() const = 0;

	void							SetData(SComData* pData)	{ASSERT(m_pComData == NULL); m_pComData = pData;}
	template<class T> T*			GetData(bool* pAdded = NULL)
	{
		if(m_pComData == NULL && pAdded)
		{
			*pAdded = true;
			m_pComData = new T();
		}
		return (T*)m_pComData;
	}

	virtual void					AddUpLimit(CBandwidthLimit* pUpLimit) {}
	virtual void					AddDownLimit(CBandwidthLimit* pDownLimit) {}

protected:
	SComData*						m_pComData;
};

typedef map<CSafeAddress::EProtocol, CSafeAddress>	TAddressMap;

class CSmartSocket: public CObject
{
public:
	DECLARE_OBJECT(CSmartSocket)

	CSmartSocket(uint64 KeepAlive, CObject* pParent = NULL);
	virtual ~CSmartSocket();

	virtual void					SetupCrypto(uint64 RecvKey, CPrivateKey* pPrivKey);

	virtual void					Process(bool bSecond);

	virtual	void					ProcessPacket(const string& Name, const CVariant& Packet, CComChannel* pChannel);

	virtual void					InstallListener(CSocketListner* pListener);

	virtual void					RegisterInterface(CSmartSocketInterface* pInterface, const string& Prefix);
	virtual void					UnregisterInterface(CSmartSocketInterface* pInterface);

	virtual list<CSafeAddress::EProtocol> GetProtocols();

	virtual CComChannel*			NewChannel(const CSafeAddress& Address, bool bDirect = false, bool bRendevouz = false, bool bEmpty = false); 
	virtual void					InsertSession(CSocketSession* pSession)	{m_Sessions.push_back(pSession);}

	virtual list<CSocketSession*>	GetChannels(const CSafeAddress& Address, bool bIgnorePort = true);

	virtual uint64					GetRecvKey()		{return m_RecvKey;}
	virtual CPrivateKey*			GetPrivateKey()		{return m_pPrivKey;}
	virtual CPublicKey*				GetPublicKey()		{return m_pPubKey;}

	virtual CSocketListner*			GetListner(const CSafeAddress& Address);

	CBandwidthManager*				GetUpManager()		{return m_UpManager;}
	CBandwidthManager*				GetDownManager()	{return m_DownManager;}

	CBandwidthLimit*				GetUpLimit()		{return m_UpLimit;}
	CBandwidthLimit*				GetDownLimit()		{return m_DownLimit;}

	typedef multimap<CSafeAddress::EProtocol, CSocketListner*> ListnerMap;
	virtual ListnerMap&				GetListners()		{return m_Listners;}

protected:
	ListnerMap						m_Listners;

	list<CPointer<CSocketSession> >	m_Sessions;

	typedef map<string, CSmartSocketInterface*> InterfaceMap;
	InterfaceMap					m_Interfaces;

	uint64							m_KeepAlive;
	uint64							m_RecvKey;
	CPrivateKey*					m_pPrivKey;
	CScoped<CPublicKey>				m_pPubKey;

	CBandwidthManager*				m_UpManager;
	CBandwidthManager*				m_DownManager;

	// GlobalLimit
	CBandwidthLimit*				m_UpLimit;
	CBandwidthLimit*				m_DownLimit;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Abstract Socket Interface Class

class CSmartSocketInterface
{
public:
	CSmartSocketInterface(CSmartSocket* pSocket, const string& sPrefix) {
		m_pSmartSocket = pSocket;
		m_pSmartSocket->RegisterInterface(this, sPrefix);
	}
	virtual ~CSmartSocketInterface() {
		m_pSmartSocket->UnregisterInterface(this);
	}

	virtual	bool	ProcessPacket(const string& Name, const CVariant& Packet, CComChannel* pChannel) = 0;

	virtual list<CSafeAddress::EProtocol> GetProtocols() {
		return m_pSmartSocket->GetProtocols();
	}

	virtual CComChannel* Connect(const CSafeAddress& Address, bool bDirect = false) {
		return m_pSmartSocket->NewChannel(Address, bDirect);
	}

	virtual CComChannel* Rendevouz(const CSafeAddress& Address) {
		return m_pSmartSocket->NewChannel(Address, false, true);
	}
	virtual CComChannel* CallBack(const CSafeAddress& Address, bool bActive) {
		return m_pSmartSocket->NewChannel(Address, false, false, bActive == false);
	}

	virtual list<CSocketSession*> GetChannels(const CSafeAddress& Address, bool bIgnorePort = true) {
		return m_pSmartSocket->GetChannels(Address, bIgnorePort);
	}

protected:
	CSmartSocket*		m_pSmartSocket;
};