#pragma once

#include "KadRelay.h"
#include "RouteSession.h"

struct SFrameTrace
{
	SFrameTrace()
	{
		static uint64 Counter = 0;
		Counter++;
		AbsID = Counter;
		SendTime = GetCurTick();
		AckTime = 0;
	}

	uint64		SendTime;
	uint64		AckTime;
	CVariant	Frame;
	uint64		AbsID; // AbstractID 
};

typedef list<SFrameTrace>	FrameTraceList;

class CKadRoute: public CKadRelay
{
public:
	DECLARE_OBJECT(CKadRoute)

	CKadRoute(const CUInt128& ID, CPrivateKey* pEntityKey = NULL, CObject* pParent = NULL);

	virtual void		Process(UINT Tick);

	virtual CPrivateKey*GetPrivateKey()								{return m_pPrivateKey;}
	virtual CPublicKey*	GetPublicKey()								{return m_pPublicKey;}

	virtual CRouteSession* OpenSession(const CVariant& EntityID, const CUInt128& TargetID);

	virtual bool		CloseSession(const CVariant& EntityID, const CVariant& SessionID);

	virtual bool		RelayUp(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel)		{ASSERT(0); return false;}
	virtual bool		RelayDown(const CVariant& Frame, uint64 TTL, CKadNode* pFromNode, CComChannel* pChannel);

	virtual bool		AckUp(const CVariant& Ack, bool bDelivery = false);
	virtual bool		AckDown(const CVariant& Ack, bool bDelivery = false)				{ASSERT(0); return NULL;}

	virtual void		SendRawFrame(const CVariant& EntityID, const CUInt128& TargetID, const CVariant& Data);
	virtual void		ProcessRawFrame(const CVariant& Data, const CVariant& EntityID, const CUInt128& TargetID) {}

	virtual uint64		GetTimeRemaining()							{return 0;}

	typedef multimap<CVariant, CRouteSession*>	SessionMap;
	virtual SessionMap&	GetSessions()								{return m_SessionMap;}

	virtual FrameTraceList& GetFrameHistory()						{return m_FrameHistory;}

protected:
	virtual CRouteSession*	MkRouteSession(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent) = 0;
	virtual void		IncomingSession(CRouteSession* pSession) {}

	friend class CRouteSession;
	virtual bool		RelayUp(const CVariant& Frame, uint64 TTL);
	virtual void		QueueAck(const CVariant& Ack, CKadNode* pFromNode, CComChannel* pChannel);

	virtual void		ProcessRawFrame(const CVariant& Frame, CKadNode* pFromNode, CComChannel* pChannel);

	CHolder<CPrivateKey>m_pPrivateKey;

	map<CVariant,int>	m_RecivedFrames;
	struct SAck
	{
		CVariant		Ack;
		SKadNode		From;
	};
	typedef list<CScoped<SAck> > TAckList;
	TAckList			m_Acks;

	SessionMap			m_SessionMap;

	virtual void		TraceFrameSend(const CVariant& Frame);
	virtual void		TraceFrameAck(const CVariant& Frame);
	virtual void		TraceFrameRecv(const CVariant& Frame);
	FrameTraceList		m_FrameHistory;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class CRouteSessionImpl: public CRouteSession
{
public:
	DECLARE_OBJECT(CRouteSessionImpl)

	CRouteSessionImpl(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent = NULL);

	virtual void			HandleBytes(const CBuffer& Buffer, bool bStream);

	// Note: For incomming data we know how much of the stream have been reassembleed 
	//			and we know the size of the single packets
	//			we dont know and dont care how much incompelte stream there is cached
	virtual size_t			GetPendingSize()	{return m_Stream.GetSize() + m_PacketsSize;}
	// Note: For outgoing data we have only infos on the stream not on the single packets
	virtual size_t			GetQueuedSize()		{return m_SegmentQueueSize;} 

	virtual void			PullBytes(CBuffer& Buffer, bool &bStream, size_t MaxBytes);

protected:
	virtual bool			IsBussy();

	CBuffer					m_Stream;
	list<CBuffer>			m_Packets;
	size_t					m_PacketsSize;
};

class CKadRouteImpl: public CKadRoute
{
public:
	DECLARE_OBJECT(CKadRouteImpl)

	CKadRouteImpl(const CUInt128& ID, CPrivateKey* pEntityKey = NULL, CObject* pParent = NULL);

	virtual bool			QueueBytes(const CVariant& EntityID, const CVariant& SessionID, const CBuffer& Buffer, bool bStream);
	virtual void			QuerySessions(list<SRouteSession>& Sessions);
	virtual bool			PullBytes(const CVariant& EntityID, const CVariant& SessionID, CBuffer& Buffer, bool &bStream, size_t MaxBytes);

protected:
	virtual CRouteSession*	MkRouteSession(const CVariant& EntityID, const CUInt128& TargetID, CObject* pParent);
};