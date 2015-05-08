#pragma once

#include "../Common/Object.h"
#include "../Common/Variant.h"
#include "../Framework/Buffer.h"
#include "SmartSocket.h"
#include "PacketQueue.h"
#include "./BandwidthControl/BandwidthLimiter.h"

#include "../../Framework/Cryptography/KeyExchange.h"
#include "../../Framework/Cryptography/AsymmetricKey.h"
class CSocketSession;
class CSocketListner;

///////////////////////////////////////////////////////////////////////////////////////
//

class CMessageSession: public CComChannel
{
public:
	DECLARE_OBJECT(CMessageSession)

	CMessageSession(CSocketListner* pListner, const CSafeAddress& Address/*, bool bEncrypted = false*/) {m_pListner = pListner; m_Address = Address; /*m_bEncrypted = bEncrypted;*/}
	virtual ~CMessageSession() {}

	virtual UINT					QueuePacket(const string& Name, const CVariant& Packet, int iPriority = 0/*, uint32 uTTL = -1*/) {SendPacket(Name, Packet); return -1;}
	virtual bool					IsQueued(UINT uID) const	{return false;}
	virtual void					SendPacket(const string& Name, const CVariant& Packet);

	virtual const CSafeAddress&		GetAddress() const	{return m_Address;}

	virtual void					Close() {}

	virtual void					SetQueueLock(bool bSet)	{ASSERT(0);}
	virtual void					Encrypt(CSymmetricKey* pCryptoKey) {ASSERT(0);}
	//virtual bool					Encrypt(CPublicKey* pPubKey = NULL, UINT eAlgorithm = 0) {return false;}
	virtual bool					IsEncrypted() const	{return false;}
	virtual bool					IsConnected() const	{return true;} // connected means we can send, this is always true for direct packets
	virtual bool					IsBussy() const {return false;}
	virtual bool					IsDisconnected() const {return false;}

protected:
	CSocketListner*					m_pListner;
	CSafeAddress					m_Address;
	//bool							m_bEncrypted;
};

///////////////////////////////////////////////////////////////////////////////////////
//

class CSocketListner: public CObject
{
public:
	DECLARE_OBJECT(CSocketListner)

	CSocketListner(CSmartSocket* pSocket);

	virtual void					Process() = 0;

	virtual	CSocketSession*			CreateSession(const CSafeAddress& Address, bool bRendevouz = false, bool bEmpty = false) = 0;
	virtual	bool					SendPacket(const string& Name, const CVariant& Packet, const CSafeAddress& Address);

	virtual CSafeAddress::EProtocol	GetProtocol() = 0;

	//virtual CSafeAddress			GetAddress() = 0;

protected:
	virtual	bool					SendTo(const CBuffer& Packet, const CSafeAddress& Address) = 0;
	virtual	void					ReceiveFrom(const CBuffer& Packet, const CSafeAddress& Address);
};

///////////////////////////////////////////////////////////////////////////////////////
//

//#define _LOG_STREAMS_

class CSocketSession: public CComChannel, public CBandwidthLimiter
{
public:
	DECLARE_OBJECT(CSocketSession)

	CSocketSession(CSocketListner* pListener, const CSafeAddress& Address);
	virtual ~CSocketSession() {}

	virtual void					Process();

	virtual	void					ProcessPacket(const string& Name, const CVariant& Packet);
	virtual UINT					QueuePacket(const string& Name, const CVariant& Packet, int iPriority = 0/*, uint32 uTTL = -1*/);
	virtual bool					IsQueued(UINT uID) const;
	virtual void					SendPacket(const string& Name, const CVariant& Packet);

	virtual void					SetQueueLock(bool bSet)	{m_LockQueue = bSet;}
	virtual void					Encrypt(CSymmetricKey* pCryptoKey);
	virtual bool					IsEncrypted() const		{return m_pCryptoKey != 0;}

	virtual uint64					GetIdleTime();
	virtual	void					KeepAlive();

	virtual bool					IsValid() const = 0;
	virtual void					Swap(CSocketSession* pNew) = 0;

	virtual const CSafeAddress&		GetAddress() const	{return m_Address;}

	virtual void					AddUpLimit(CBandwidthLimit* pUpLimit)		{CBandwidthLimiter::AddLimit(pUpLimit, eUpChannel);}
	virtual void					AddDownLimit(CBandwidthLimit* pDownLimit)	{CBandwidthLimiter::AddLimit(pDownLimit, eDownChannel);}

	//CBandwidthLimit*				GetUpLimit()		{return m_UpLimit;}
	//CBandwidthLimit*				GetDownLimit()		{return m_DownLimit;}

protected:
	virtual void					StreamIn(byte* Data, size_t Length);
	virtual void					StreamOut(byte* Data, size_t Length);

	CSafeAddress					m_Address;
	
	CPacketQueue					m_PacketQueue;
	CBuffer							m_Sending;
	bool							m_LockQueue;
	CBuffer							m_Receiving;
#ifdef _LOG_STREAMS_
	QFile							m_InBound;
	QFile							m_OutBound;
#endif

	CHolder<CSymmetricKey>			m_pCryptoKey;

	uint64							m_LastActivity;

	// LocalLimit
	//CBandwidthLimit*				m_UpLimit;
	//CBandwidthLimit*				m_DownLimit;
};
