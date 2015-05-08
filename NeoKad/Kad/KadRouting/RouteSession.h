#pragma once

#include "../../../Framework/Cryptography/KeyExchange.h"
#include "../../../Framework/Cryptography/AsymmetricKey.h"
#include "../../../Framework/Cryptography/SymmetricKey.h"
#include "../../Networking/PacketQueue.h"

class CKadRoute;

#include "RouteStats.h"

struct SSessionStats: SRouteStats
{
	SSessionStats()
	{
		ProcessedFrames = 0;
	}

	int				ProcessedFrames;
};

class CRouteSession: public CRouteStats
{
public:
	DECLARE_OBJECT(CRouteSession)

	CRouteSession(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent = NULL);
	virtual ~CRouteSession() {}
	virtual void			Destroy();

	virtual bool			Process(UINT Tick);

	virtual bool			IsConnected()							{return m_ConnectionStage == eOpen;}
	virtual const CUInt128&	GetTargetID()							{return m_TargetID;}
	virtual const CVariant&	GetEntityID()							{return m_EntityID;}
	virtual const CVariant&	GetSessionID()							{return m_SessionID;}
	virtual CPublicKey*		GetPublicKey()							{return m_PublicKey;}

	virtual void			InitSession(const CVariant& SessionID, bool bSendHandShake = true);
	virtual void			CloseSession();

	virtual void			Encrypt(CVariant& Data, const CVariant& FID);
	virtual bool			Decrypt(CVariant& Data, const CVariant& FID);

	virtual void			QueueBytes(const CBuffer& Buffer, bool bStream);
	virtual void			HandleBytes(const CBuffer& Buffer, bool bStream) = 0;

	virtual void			ReassemblyStream(CVariant& Segment);

	virtual void			ProcessFrame(const CVariant& Frame, CKadNode* pFromNode, CComChannel* pChannel);
	virtual bool			AckFrame(const CVariant& Ack);

	size_t					GetQueuedFrameCount()			{return m_FrameQueue.size();}
	const SSessionStats&	GetStats() const				{return *((SSessionStats*)m_pStats.Val());}
	uint64					GetRecvOffset() const			{return m_RecvOffset;}
	uint64					GetSendOffset() const			{return m_SendOffset;}
	size_t					GetIncompleteCount() const		{return m_SegmentBuffer.size();}

	void					SetLastUpdate(uint64 uLastUpdate){m_uLastUpdate = uLastUpdate;}
	uint64					GetLastUpdate() const			{return m_uLastUpdate;}

	struct SFrame
	{
		SFrame(const CVariant& frame)
		{
			Frame = frame;
			SendTime = 0;
			SendCount = 0;
			CurTTL = 0;
			bSign = false;
			uSize = 0;
		}

		CVariant			Frame;
		uint64				SendTime;
		uint16				SendCount;
		uint64				CurTTL;
		bool				bSign;
		size_t				uSize;
	};
	typedef list<CScoped<SFrame> > TFrameList;

	CVariant				MakeCryptoReq(UINT eAlgorithm = 0);
	CVariant				HandleCryptoReq(const CVariant& Request);
	void					HandleCryptoRes(const CVariant& Response);

protected:
	friend class CKadRoute;
	virtual void			QueueFrame(const string &Name, const CVariant& Field, bool bEncrypt = false, bool bSign = false);
	virtual void			SendHandShake(const CVariant& KeyPkt);
	virtual void			Closed(bool bError) {m_ConnectionStage = bError ? eBroken : eClosed;}

	virtual void			SendAck(const CVariant& Frame, CKadNode* pFromNode, CComChannel* pChannel, const char* pError = NULL);
	virtual void			HandlePayload(const CVariant& Frame);
	virtual bool			IsBussy()						{return false;} // return true to stop processing dataframes

	CVariant				m_EntityID;
	CUInt128				m_TargetID;
	CHolder<CPublicKey>		m_PublicKey;
	CVariant				m_SessionID;
	CHolder<CKeyExchange>	m_KeyExchange;
	CHolder<CAbstractKey>	m_SessionKey;
	CHolder<CSymmetricKey>	m_CryptoKey;

	TFrameList				m_FrameQueue;

	uint64					m_FIDCounter;
	uint64					m_SendOffset;
	map<uint64, CBuffer>	m_SegmentQueue;
	size_t					m_SegmentQueueSize;
	uint64					m_RecvOffset;
	map<uint64, CBuffer>	m_SegmentBuffer;

	struct SPayload
	{
		CVariant		Frame;
		SKadNode		From;
	};
	typedef list<CScoped<SPayload> > TPayloadList;
	TPayloadList			m_Payloads;

	uint64					m_uLastUpdate;

	struct SFIDTrace
	{
		SFIDTrace()
		{
			Age = GetCurTick();
			Count = 0;
		}
		int		Count;
		uint64	Age;
	};
	typedef map<CVariant, SFIDTrace> FIDMap;
	FIDMap					m_FIDTrace;

	enum EStage
	{
		eNone,
		eInit,
		eOpen,
		eClosing,
		eClosed,
		eBroken
	};
	EStage					m_ConnectionStage;
};