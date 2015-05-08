#pragma once

class CPayloadIndex;
class CKadHandler;
class CFirewallHandler;
class CLookupManager;
class CKadPayload;
class CKadLookup;
class CKademlia;
class CAbstractKadFactory;
class CKadConfig;
class CSmartSocket;
class CKadEngine;
class CSafeAddress;
class CRoutingRoot;
#include "../../Framework/Cryptography/AsymmetricKey.h"

#define KAD_PROT_MJR 0
#define KAD_PROT_MIN 1
#define KAD_PROT_VER ((0 << 24) | (0 << 16) | (KAD_PROT_MJR << 8) | (KAD_PROT_MIN << 0)) // 0.1

struct SKadStore
{
	string			Path;
	CVariant		Data;
};
typedef map<CVariant, SKadStore>	TStoreMap;

struct SKadStored
{
	uint32			Count;
	time_t			Expire;
};
typedef map<CVariant, SKadStored>	TStoredMap;

struct SKadLoad
{
	string			Path;
};
typedef map<CVariant, SKadLoad> TLoadMap;

struct SKadLoaded
{
	string			Path;
	CVariant		Data;
};
typedef multimap<CVariant, SKadLoaded> TLoadedMap;

struct SKadCall
{
	string		Function;
	CVariant	Parameters;
};
typedef map<CVariant, SKadCall> TCallMap;

struct SKadRet
{
	string		Function;
	CVariant	Return;
};
typedef multimap<CVariant, SKadRet> TRetMap;

struct SKadMessage
{
	string		Name;
	CVariant	Data;
	CUInt128	Node;
};
typedef list<SKadMessage> TMessageList;

struct SKadEntryInfo
{
	uint64		Index;
	CUInt128	ID;
	time_t		Date;
	time_t		Expire;
	string		Path;
};

struct SKadEntryInfoEx
{
	uint64		Index;
	CUInt128	ID;
	time_t		Date;
	time_t		Expire;
	string		Path;
	CHolder<CAbstractKey> pAccessKey;
	CVariant	ExclusiveCID;
};

struct SRouteSession
{
	SRouteSession() : QueuedBytes(0),PendingBytes(0), Connected(false) {}
	CVariant	EntityID; // Emiter / Reciver ID
	CVariant	TargetID;
	CVariant	SessionID;
	uint64		QueuedBytes;
	uint64		PendingBytes;
	bool		Connected;
};

struct SKadScriptInfo
{
	CVariant	CodeID;
	wstring		Name;
	uint32		Version;
};

class CKademlia: public CObject
{
public:
	DECLARE_OBJECT(CKademlia)

	CKademlia(uint16 Port, bool bIPv6 = false, const CVariant& Config = CVariant(), const string& Version = "");
	~CKademlia();

	void					Process(UINT Tick);

	void					Connect();
	void					Disconnect();
	bool					IsConnected() const;
	bool					IsDisconnected() const			{return m_pKadHandler == NULL;}
	size_t					GetNodeCount() const;
	time_t					GetLastContact() const;
	int						GetUpRate();
	int						GetDownRate();

	bool					HasScript(const CVariant& CodeID) const;
	void					QueryScripts(list<SKadScriptInfo>& Scripts) const;
	bool					InstallScript(const CVariant& CodeID, const string& Source, const CVariant& Authentication = CVariant());

	CVariant				StartLookup(const CUInt128& TargetID, const CVariant& CodeID, const TCallMap& Execute, const TStoreMap& Store, CPrivateKey* pStoreKey, const TLoadMap& Load
										, int Timeout = -1, int HopLimit = -1, int JumpCount = -1, int SpreadCount = -1, bool bTrace = false, const wstring& Name = L"");
	CVariant				QueryLookup(const CVariant& LookupID, TRetMap& Results, TStoredMap& Stored, TLoadedMap& Loaded);
	void					StopLookup(const CVariant& LookupID);

	CUInt128				MakeCloseTarget(int *pDistance = NULL);

	CVariant				SetupRoute(const CUInt128& TargetID, CPrivateKey* pEntityKey = NULL, int HopLimit = -1, int JumpCount = -1, bool bTrace = false);
	CPrivateKey*			GetRouteKey(const CVariant& MyEntityID);
	CVariant				QueryRoute(const CVariant& MyEntityID);
	CVariant				OpenSession(const CVariant& MyEntityID, const CVariant& EntityID, const CUInt128& TargetID);
	bool					QueueBytes(const CVariant& MyEntityID, const CVariant& EntityID, const CVariant& SessionID, const CBuffer& Buffer, bool bStream);
	bool					QuerySessions(const CVariant& MyEntityID, list<SRouteSession>& Sessions);
	bool					PullBytes(const CVariant& MyEntityID, const CVariant& EntityID, const CVariant& SessionID, CBuffer& Buffer, bool &bStream, size_t MaxBytes);
	bool					CloseSession(const CVariant& MyEntityID, const CVariant& EntityID, const CVariant& SessionID);
	void					BreakRoute(const CVariant& MyEntityID);

	bool					InvokeScript(const CVariant& CodeID, const string& Function, const CVariant& Arguments, CVariant& Result, const CVariant& CallerID = CVariant());

	bool					Store(const CUInt128& TargetID, const string& Path, const CVariant& Data, time_t Expire = -1);
	void					List(const string& Path, vector<SKadEntryInfo>& Entrys);
	bool					List(const CUInt128& TargetID, const string& Path, vector<SKadEntryInfo>& Entrys);
	CVariant				Load(uint64 Index);
	void					Remove(uint64 Index);

	uint32					GetProtocol()						{return m_Protocol;}
	const string&			GetVersion()						{return m_Version;}

	CUInt128				GetID() const;
	CSafeAddress			GetAddress(UINT Protocol) const;
	EFWStatus				GetFWStatus(UINT Protocol) const;
	uint16					GetPort() const;
	CAddress				GetIPv4() const;
	CAddress				GetIPv6() const;

	bool					BindSockets(uint16 uPort, const CAddress& IPv4 = CAddress(CAddress::IPv4), const CAddress& IPv6 = CAddress(CAddress::IPv6));

	CKadConfig*				Cfg() const							{return m_pConfig;}

protected:
	friend class CKadHandler;
	friend class CLookupManager;
	friend class CPayloadIndex;
	friend class CRoutingBin;
	friend class CFirewallHandler;
	friend class CKadEngine;
	friend class CBinaryCache;
	friend class CJSKadScript;
	friend class CJSKademlia;
	friend class CJSKadLookup;
	friend class CKadScript;
	friend class CKadLookup;
	friend class CKadNode;
	friend class CKadPayload;
	friend class CKadRelay;
	friend class CKadRoute;
	friend class CRouteSession;
	friend class CRoutingFork;
	friend class CLookupHistory;
	friend class CFrameRelay;
	friend class CKadOperation;
	friend class CLookupProxy;
	friend class CRoutingZone;
	friend class CRoutingRoot;
	friend class CKadBridge;
	friend class CRouteStats;
	friend class CKadTask;

	CKadHandler*			Handler()						{return m_pKadHandler;}
	CRoutingRoot*			Root()							{return m_pRootZone;}
	CLookupManager*			Manager()						{return m_pLookupManager;}
	CPayloadIndex*			Index()							{return m_pPayloadIndex;}
	CFirewallHandler*		Fwh()							{return m_pFirewallHandler;}
	CKadEngine*				Engine()						{return m_pKadEngine;}

	uint16					GetPort(UINT eProtocol);

private:
	bool					LoadData();
	void					SaveData();
	uint32					LoadNodes();
	void					SaveNodes();

	uint64					m_uLastSave;

	uint32					m_Protocol;
	string					m_Version;

	CHolder<CPrivateKey>			m_pKadKey;
	CPointer<CSmartSocket>			m_pSocket;
	CPointer<CKadConfig>			m_pConfig;

	CPointer<CFirewallHandler>		m_pFirewallHandler;
	CPointer<CKadHandler>			m_pKadHandler;
	CPointer<CRoutingRoot>			m_pRootZone;

	CPointer<CPayloadIndex>			m_pPayloadIndex;
	CPointer<CLookupManager>		m_pLookupManager;

	CPointer<CKadEngine>			m_pKadEngine;

	uint16							m_Port;
	CAddress						m_IPv4;
	CAddress						m_IPv6;
};

