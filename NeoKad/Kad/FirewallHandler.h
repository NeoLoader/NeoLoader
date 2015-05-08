#pragma once

#include "../Networking/SmartSocket.h"
#include "../Networking/SocketSession.h"

class CNodeAddress;

class CMyAddress: public CSafeAddress
{
public:
	CMyAddress(const CSafeAddress& Address = CSafeAddress(), EFWStatus eAccess = eFWOpen)
	 : CSafeAddress(Address)
	{
		m_Access = eAccess;
		m_AuxPort = 0;
		m_AssistentPending = 0;
	}
	virtual ~CMyAddress(){}

	EFWStatus				GetStatus() const		{return m_Access;}

	virtual void			FromVariant(const CVariant& Variant) { ASSERT(0);}
	virtual CVariant		ToVariant(bool bWithAssistent = true) const
	{
		CVariant Variant = CSafeAddress::ToVariant();
		if(m_Access != eFWOpen)
		{
			if(m_pAssistentChannel && m_AssistentPending == 0)
				Variant["REL"] = m_pAssistentChannel->GetAddress().ToVariant();
			else
				Variant["REL"] = CVariant();
		}
		return Variant;
	}

	virtual bool			IsVerifyed() const				{return true;}

	virtual void			SetAuxPort(uint16 AuxPort)		{m_AuxPort = AuxPort;}
	virtual uint16			GetAuxPort() const				{return m_AuxPort;}

	virtual const CSafeAddress*	GetAssistent() const		
	{
		if(CComChannel* pChannel = GetAssistentChannel())
			return &pChannel->GetAddress();
		return NULL;
	}
	virtual CComChannel*	GetAssistentChannel() const
	{
		if(m_Access != eFWOpen && m_AssistentPending == 0) 
			return m_pAssistentChannel; 
		return NULL;
	}
	virtual void			ConfirmAssistent()				{m_pAssistentChannel = 0;}
	virtual void			ClearAssistent()
	{
		m_AssistentPending = 0;
		m_pAssistentChannel = NULL;
	}

	virtual bool			IsAssistentPending()			{return m_AssistentPending;}
	virtual bool			IsAssistentPending(CComChannel* pChannel) {return m_AssistentPending && m_pAssistentChannel == pChannel;}
	virtual void			AddPendingAssistent(CPointer<CComChannel> pChannel)
	{
		m_pAssistentChannel = pChannel; 
		m_AssistentPending = GetCurTick();
	}

	virtual bool			NeedsAssistance()				{return m_Access != eFWOpen && m_pAssistentChannel == NULL;}

protected:
	EFWStatus				m_Access;
	uint16					m_AuxPort;
	uint64					m_AssistentPending;
	CPointer<CComChannel>	m_pAssistentChannel;
};

typedef map<CSafeAddress::EProtocol, CMyAddress>	TMyAddressMap;

class CFirewallHandler: public CObject, public CSmartSocketInterface
{
public:
	DECLARE_OBJECT(CFirewallHandler)

	CFirewallHandler(CSmartSocket* pSocket, CObject* pParent = NULL);

	virtual void			Process(UINT Tick);
	virtual	bool			ProcessPacket(const string& Name, const CVariant& Packet, CComChannel* pChannel);

	virtual	bool			RelayPacket(const string& Name, const CVariant& Packet, const CNodeAddress& Address);
	virtual	void			HandleRelay(const CVariant& Packet, CComChannel* pChannel);
	virtual	void			HandleRelayed(const CVariant& Packet, CComChannel* pChannel);

	virtual void			SendCheckReq(CComChannel* pChannel);
	virtual void			HandleCheckReq(const CVariant& CheckReq, CComChannel* pChannel);
	virtual void			HandleCheckRes(const CVariant& CheckReq, CComChannel* pChannel);

	virtual void			SendTestReq(CComChannel* pChannel, const CSafeAddress& TestAddress);
	virtual void			HandleTestReq(const CVariant& TestReq, CComChannel* pChannel);
	virtual void			HandleTestRelReq(const CVariant& RelTestReq, CComChannel* pChannel);
	virtual void			HandleTestRelAck(const CVariant& RelTestAck, CComChannel* pChannel);
	virtual void			SendTestAck(CComChannel* pChannel, const CSafeAddress& HlprAddress, const CVariant& Err = CVariant());
	virtual void			HandleTestAck(const CVariant& TestAck, CComChannel* pChannel);
	virtual void			SendTestRes(CComChannel* pChannel);
	virtual void			HandleTestRes(const CVariant& TestRes, CComChannel* pChannel);

	virtual void			SendAssistanceReq(CComChannel* pChannel);
	virtual void			HandleAssistanceReq(const CVariant& AssistanceReq, CComChannel* pChannel);
	virtual void			HandleAssistanceRes(const CVariant& AssistanceRes, CComChannel* pChannel);

	virtual void			SendTunnel(const CNodeAddress& Address, bool bCallBack);
	virtual void			HandleTunnel(const CVariant& RdvReq, CComChannel* pChannel);

	virtual const TMyAddressMap& AddrPool() {return m_AddrPool;}
	virtual const CMyAddress* GetAddress(CSafeAddress::EProtocol Protocol) const;
	virtual EFWStatus		GetFWStatus(CSafeAddress::EProtocol Protocol) const;

protected:
	TMyAddressMap			m_AddrPool; // last confirmed address

	map<CSafeAddress, pair<CPointer<CComChannel>, uint64> > m_PersistentTunnels;
	map<CSafeAddress, CPointer<CComChannel> > m_PendingTestRelays; // testAddr, reqAddr

	struct SWatchDog
	{
	public:
		enum EStage
		{
			eNone = 0,
			eDetectAddr,
			eCheckPort,
			eCheckAuxPort
		};

		SWatchDog()
		{
			NextCheck = 0;
			TestStage = eNone;
		}

		uint64					NextCheck;
		EStage					TestStage;

		CSafeAddress			CurAddress;

		struct SResult
		{
			SResult() {Count = 0;}
			int Count;
			map<uint16, int> Ports;
		};
		map<CSafeAddress, SResult>	Results;
		map<CSafeAddress, uint64>	Pending;
		map<CSafeAddress, pair<bool, bool> > Reports;
	};

	map<CSafeAddress::EProtocol, SWatchDog> m_WatchDogs;
};