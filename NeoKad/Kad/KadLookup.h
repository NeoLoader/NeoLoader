#pragma once

#include "KadNode.h"
#include "LookupHistory.h"

class CKadLookup: public CObject
{
public:
	DECLARE_OBJECT(CKadLookup)

	CKadLookup(const CUInt128& ID, CObject* pParent = NULL);
	virtual ~CKadLookup();

	virtual CUInt128&	GetID()										{return m_ID;}
	virtual const CUInt128& GetID() const							{return m_ID;}

	virtual void		SetTimeOut(int TimeOut, bool bFromNow = false);
	virtual uint64		GetTimeOut()								{return m_TimeOut;}

	virtual void		SetJumpCount(int JumpCount);
	virtual int			GetJumpCount()								{return m_JumpCount;}
	virtual void		SetHopLimit(int HopLimit);
	virtual int			GetHopLimit()								{return m_HopLimit;}
	virtual int			GetNeededCount();

	virtual void		Start();
	virtual void		Stop();

	virtual bool		IsLookingForNodes()							{return m_LookupState == eLookupActive;}

	virtual void		AddNodes(const NodeMap& Nodes, const CUInt128& ByID = 0);
	virtual CLookupHistory* GetHistory()							{return m_LookupHistory;}

	virtual bool		RequestNodes(CKadNode* pNode, CComChannel* pChannel, const CUInt128& ByID = 0);
	virtual bool		WasNodeUsed(CKadNode* pNode);
	virtual bool		OnCloserNodes(CKadNode* pNode, CComChannel* pChannel, int CloserCount);

	virtual void		RecivedTraceResults(const CVariant& Trace);

	enum EStatus
	{
		ePending,
		eStarted,
		eStopped
	};

	virtual bool		HasTimedOut();
	virtual uint64		GetStartTime()								{return m_StartTime;}
	virtual uint64		GetDuration();
	virtual uint64		StoppedSince();
	virtual void		Process(UINT Tick);
	virtual bool		IsStarted()									{return m_Status == eStarted;}
	virtual bool		ReadyToStop();
	virtual void		PrepareStop();
	virtual bool		IsStopping()								{return m_StopTime != -1;}
	virtual bool		IsStopped()									{return m_Status == eStopped;}
	virtual uint64		GetTimeRemaining();

	virtual void		InitLookupID();
	virtual void		SetLookupID(const CVariant& LookupID)		{m_LookupID = LookupID;}
	virtual const CVariant& GetLookupID()							{return m_LookupID;}

	virtual void		EnableTrace()								{m_Trace = true;}
	virtual bool		IsTraced() const							{return m_Trace;}

	CBandwidthLimit*	GetUpLimit()								{return m_UpLimit;}
	CBandwidthLimit*	GetDownLimit()								{return m_DownLimit;}

	virtual void		FlushCaches()								{}
	virtual bool		CachesEmpty();

protected:
	virtual void		InitOperation();

	void				CleanUpNodes();

	enum ELookupState
	{
		eNoLookup = 0,		// do not do a node lookup
		eLookupActive,		// node lookup in progress
		eLookupFinished,	// we already have looked for closest nodes
		eLookupClosest		// node only status - like finished
	};

	struct SNodeStatus
	{
		SNodeStatus(){
			LookupState = eNoLookup;
			uTimeOut = -1;
		}
		virtual ~SNodeStatus() {}

		ELookupState	LookupState;

		uint64			uTimeOut;
	};

	virtual SNodeStatus*NewNodeStatus()	{return new SNodeStatus;}

	virtual SNodeStatus*GetStatus(CKadNode* pNode);

	virtual CComChannel*GetChannel(CKadNode* pNode);
	virtual void		ChannelClosed(CKadNode* pNode)				{}
	virtual void		NodeStalling(CKadNode* pNode)				{}

	CUInt128			m_ID;

	uint64				m_TimeOut;
	int					m_HopLimit;
	int					m_JumpCount;

	ELookupState		m_LookupState;

	typedef map<SKadNode, SNodeStatus*> TNodeStatusMap;
	TNodeStatusMap		m_Nodes;
	int					m_ClosestNodes;

	
	uint64				m_StartTime;
	uint64				m_StopTime;
	EStatus				m_Status;

	CVariant			m_LookupID;

	CLookupHistory*		m_LookupHistory;
	bool				m_Trace;

	CBandwidthLimit*	m_UpLimit;
	CBandwidthLimit*	m_DownLimit;
};
