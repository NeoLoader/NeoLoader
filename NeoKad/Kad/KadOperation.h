#pragma once

class CKadOperator;
class CKadScript;
class CJSException;
#include "Kademlia.h"
#include "KadLookup.h"


class CKadOperation: public CKadLookup
{
public:
	DECLARE_OBJECT(CKadOperation)

	CKadOperation(const CUInt128& ID, CObject* pParent = NULL);

	virtual void		SetSpreadCount(int SpreadCount);
	virtual int			GetSpreadCount()							{return m_SpreadCount;}
	virtual void		SetSpreadShare(int SpreadShare)				{m_SpreadShare = Min(SpreadShare, m_SpreadCount); }
	virtual int			GetSpreadShare()							{return m_SpreadShare;}
	virtual int			GetNeededCount();
	virtual void		InitOperation();

	virtual void		SetStoreTTL(uint32 TTL)						{m_StoreTTL = TTL;}
	virtual uint32		GetStoreTTL()								{return m_StoreTTL;}
	virtual void		SetLoadCount(uint32 Count)					{m_LoadCount = Count;}
	virtual uint32		GetLoadCount()								{return m_LoadCount;}

	virtual void		Process(UINT Tick);

	virtual int			GetActiveCount()							{return m_ActiveCount;}
	virtual int			GetFailedCount()							{return m_FailedCount;}

	virtual void		Start();
	virtual void		Stop();

	virtual bool		IsInRange()									{return m_InRange;}
	virtual bool		NoMoreNodes()								{return m_OutOfNodes;}

	virtual void		SetManualMode()								{m_ManualMode = true;}
	virtual bool		IsManualMode()								{return m_ManualMode;}

	virtual void		PrepareStop();
	virtual bool		HasFoundEnough();
	virtual bool		ReadyToStop();

	virtual void		ProxyingResponse(CKadNode* pNode, CComChannel* pChannel, const string &Error);

	virtual string		SetupScript(CKadScript* pKadScript);
	virtual CKadOperator* GetOperator() {return m_pOperator;}

	virtual void		ProcessMessage(const string& Name, const CVariant& Data, CKadNode* pNode, CComChannel* pChannel);
	virtual bool		SendMessage(const string& Name, const CVariant& Data, CKadNode* pNode);

	struct SOpStatus
	{
		SOpStatus() : Results(0), Done(false) {}
		int	Results;
		bool Done;
	};

	virtual void		SetInitParam(const CVariant& InitParam)	{m_InitParam = InitParam.Clone();}
	virtual const CVariant GetInitParam()						{return m_InitParam;}

	virtual void		SetCodeID(const CVariant& CodeID)		{m_CodeID = CodeID;}
	virtual const CVariant& GetCodeID()							{return m_CodeID;}

	struct SCallOp
	{
		SCallOp() {}
		SCallOp(const CVariant& Call) {this->Call = Call;}
		CVariant	Call;
		SOpStatus	Status;
	};
	typedef map<CVariant, SCallOp> TCallOpMap;
	TCallOpMap&			GetCalls()								{return m_CallMap;}
	virtual int			CountCalls();

	virtual CVariant	GetAccessKey() = 0;
	struct SStoreOp
	{
		SStoreOp() {}
		SStoreOp(const CVariant& Payload) {this->Payload = Payload;}
		CVariant	Payload;
		SOpStatus	Status;
	};
	typedef map<CVariant, SStoreOp> TStoreOpMap;
	virtual const TStoreOpMap & GetStoreReq()					{return m_StoreMap;}
	virtual int			CountStores()							{return m_StoreMap.size();}

	struct SLoadOp
	{
		SLoadOp() {}
		SLoadOp(const string& Path) {this->Path = Path;}
		string		Path;
		SOpStatus	Status;
	};
	typedef map<CVariant, SLoadOp> TLoadOpMap;
	virtual const TLoadOpMap & GetLoadReq()						{return m_LoadMap;}
	virtual int			CountLoads()							{return m_LoadMap.size();}

	virtual int			GetTotalJobs()							{return CountCalls() + CountStores() + CountLoads();}
	virtual int			GetTotalDoneJobs()						{return m_TotalDoneJobs;}
	virtual int			GetTotalReplys()						{return m_TotalReplys;}

	virtual void		HandleError(CKadNode* pNode);
	virtual CVariant	AddCallRes(const CVariant& CallRes, CKadNode* pNode);
	virtual CVariant	AddStoreRes(const CVariant& StoreRes, CKadNode* pNode);
	virtual CVariant	AddLoadRes(const CVariant& RetrieveRes, CKadNode* pNode);

	virtual void		LogReport(UINT Flags, const wstring& ErrorMessage, const string& Error = "", const CVariant& Trace = CVariant());

protected:
	friend class CKadOperator;
	friend class CJSKadLookup;
	friend class CJSKadNode;

	virtual bool		FindCloser()		{if(m_LookupState != eNoLookup) return false; m_LookupState = eLookupActive; return true;}

	virtual bool		AddNode(CKadNode* pNode, bool bStateless, const CVariant &InitParam, int Shares);

	virtual bool		OnCloserNodes(CKadNode* pNode, CComChannel* pChannel, int CloserCount);

	virtual void		ChannelClosed(CKadNode* pNode);
	virtual void		NodeStalling(CKadNode* pNode);

	enum EOpState
	{
		eNoOp,
		eOpPending,
		eOpProxy,
		eOpStateless,
		eOpFailed
	};

	typedef map<CVariant, SOpStatus> TStatusMap;
	struct SOpProgress: SNodeStatus
	{
		SOpProgress() {
			Shares = 0;

			OpState = eNoOp;
		}

		int			Shares;

		EOpState	OpState;

		TStatusMap	Calls;
		TStatusMap	Stores;
		TStatusMap	Loads;

		static TStatusMap& GetCalls(SOpProgress* pProgress)		{return pProgress->Calls;}
		static TStatusMap& GetStores(SOpProgress* pProgress)	{return pProgress->Stores;}
		static TStatusMap& GetLoads(SOpProgress* pProgress)		{return pProgress->Loads;}
	};

	virtual SNodeStatus*NewNodeStatus()	{return new SOpProgress;}

	virtual SOpProgress*GetProgress(CKadNode* pNode)			{return (SOpProgress*)CKadLookup::GetStatus(pNode);}


	virtual void		RequestStateless(CKadNode* pNode, CComChannel* pChannel, SOpProgress* pProgress);
	virtual bool		RequestProxying(CKadNode* pNode, CComChannel* pChannel, SOpProgress* pProgress, int FreeShares, int ShareHolders, const CVariant &InitParam);
	virtual bool		RequestProxying(CKadNode* pNode, CComChannel* pChannel, SOpProgress* pProgress, int Shares, const CVariant &InitParam);
	virtual void		SendRequests(CKadNode* pNode, CComChannel* pChannel, SOpProgress* pProgress, bool bStateless);

	virtual bool		IsDone(TStatusMap& (Get)(SOpProgress*), const CVariant& XID);

	int					m_BrancheCount;
	int					m_SpreadCount;
	int					m_SpreadShare;

	int					m_ActiveCount;
	int					m_FailedCount;

	CVariant			m_InitParam;
	CVariant			m_CodeID;
	CKadOperator*		m_pOperator;

	bool				m_InRange;
	bool				m_OutOfNodes;
	bool				m_ManualMode;

	TCallOpMap			m_CallMap;

	TStoreOpMap			m_StoreMap;
	uint32				m_StoreTTL;

	TLoadOpMap			m_LoadMap;
	uint32				m_LoadCount;

	//map<CVariant, vector<time_t> > m_StoredCounter;
	map<CVariant, set<CVariant> > m_LoadFilter;

	int					m_TotalDoneJobs;
	int					m_TotalReplys;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class CLookupProxy: public CKadOperation
{
public:
	DECLARE_OBJECT(CLookupProxy)

	CLookupProxy(const CUInt128& ID, CObject* pParent = NULL);

	virtual void		Process(UINT Tick);

	virtual void		FlushCaches();

	virtual bool		ReadyToStop();

	virtual void		InitProxy(CKadNode* pNode, CComChannel* pChannel);

	virtual bool		SendMessage(const string& Name, const CVariant& Data, CKadNode* pNode);

	virtual CVariant	GetAccessKey()								{return m_AccessKey;}
	virtual void		SetAccessKey(const CVariant& AccessKey)		{m_AccessKey = AccessKey;}

	virtual CVariant	AddCallReq(const CVariant& Requests);
	virtual void		AddStoreReq(const CVariant& StoreReq);
	virtual void		AddLoadReq(const CVariant& LoadOps);

	
	virtual CKadNode*	GetReturnNode()								{return m_Return.pNode;}
	virtual CComChannel*GetReturnChannel()							{return m_Return.pChannel;}

	virtual void		LogReport(UINT Flags, const wstring& ErrorMessage, const string& Error = "", const CVariant& Trace = CVariant());

protected:
	SKadNode			m_Return;

	CVariant			m_AccessKey;
};
