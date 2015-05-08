#include "GlobalHeader.h"
#include "KadHeader.h"
#include "Kademlia.h"
#include "KadConfig.h"
#include "KadHandler.h"
#include "FirewallHandler.h"
#include "PayloadIndex.h"
#include "LookupManager.h"
#include "KadLookup.h"
#include "KadTask.h"
#include "RoutingRoot.h"
#include "KadRouting/KadRoute.h"
#include "../Networking/SmartSocket.h"
#include "../Networking/Protocols/UTPSocketSession.h"
//#include "../Networking/Protocols/UDTSocketSession.h"
//#include "../Networking/Protocols/TCPSocketSession.h"
#include "KadEngine/KadEngine.h"
#include "KadEngine/KadScript.h"
#include "../Common/FileIO.h"
#include "../Common/v8Engine/JSVariant.h"
#include "../../Framework/Maths.h"
#include "KadEngine/KadDebugging.h"

IMPLEMENT_OBJECT(CKademlia, CObject)

CKademlia::CKademlia(uint16 Port, bool bIPv6, const CVariant& Config, const string& Version)
{
	m_Protocol = KAD_PROT_VER;

	if(!Version.empty())
		m_Version += Version;
	else
		m_Version = "v " STR(KAD_PROT_MJR) "." STR(KAD_PROT_MIN);

	m_pConfig = new CKadConfig(this);
	if(Config.IsValid())
		m_pConfig->Merge(Config);

	LoadData();

	if(m_pKadKey->GetAlgorithm() == CAbstractKey::eUndefined)
	{
		m_pKadKey->SetAlgorithm(CAbstractKey::eECP);
		//m_pKadKey->GenerateKey("secp256r1"); // NIST: P-256
		m_pKadKey->GenerateKey("brainpoolP256r1"); // dont trust NIST (NSA) curves, use brainpool
		LogLine(LOG_INFO, L"Generated new Private NodeKey");
		SaveData();
	}

	m_pSocket = new CSmartSocket(SEC2MS(Cfg()->GetInt("ConnectionTimeout")), this);

	CScoped<CPublicKey> pKey = m_pKadKey->PublicKey();

	CUInt128 ID;
	CKadID::MakeID(pKey, ID.GetData(), ID.GetSize());
	LogLine(LOG_INFO, L"Neo Kad ID: %s", ID.ToHex().c_str());

	uint64 RecvKey;
	CAbstractKey::Fold(ID.GetData(), ID.GetSize(),(byte*)&RecvKey, sizeof(RecvKey));
	m_pSocket->SetupCrypto(RecvKey, m_pKadKey);

#ifdef _DEBUG
	LogLine(LOG_INFO, L"Socket PassKey is %I64u", RecvKey);
#endif

	m_Port = 0;
	m_IPv4 = CAddress(CAddress::IPv4);
	m_IPv6 = CAddress(CAddress::IPv6);

	for(int i=Port; i < Port + 1000; i++)
	{
		CUTPSocketListner* pSocketListner = new CUTPSocketListner(m_pSocket);
		CUTPSocketListner* pSocketListnerV6 = bIPv6 ? new CUTPSocketListner(m_pSocket) : NULL;
		bool bV4 = pSocketListner->Bind(i, CAddress(CAddress::IPv4));
		bool bV6 = !pSocketListnerV6 || pSocketListnerV6->Bind(i, CAddress(CAddress::IPv6));
		if(bV4 && bV6)
		{
			m_pSocket->InstallListener(pSocketListner);
			if(pSocketListnerV6)
				m_pSocket->InstallListener(pSocketListnerV6);
			m_Port = i;
			break;
		}
		delete pSocketListner;
		delete pSocketListnerV6;
	}
}

CKademlia::~CKademlia()
{
	SaveData();
}

bool CKademlia::LoadData()
{
	m_pKadKey = new CPrivateKey(CAbstractKey::eUndefined);
	wstring ConfigPath = Cfg()->GetString("ConfigPath");
	if(!ConfigPath.empty())
	{
		CVariant Data;
		if(ReadFile(ConfigPath + L"NeoKad.dat", Data))
		{
			try
			{
				const CVariant& Info = Data["INFO"];

				string Version = Info["VER"];

				const CVariant& KeyValue = Info["PK"];
				if(!m_pKadKey->SetKey(KeyValue.GetData(), KeyValue.GetSize()))
					LogLine(LOG_ERROR, L"Loaded Invalid private Key from file");

				/*CBuffer PL = QByteArray("1234567890ABCDEF");
				CBuffer Sig;
				m_pKadKey->Sign(&PL, &Sig);
				Sig.GetSize();*/
			}
			catch(const CException&)
			{
				ASSERT(0);
				return false;
			}
		}
	}
	return true;
}

void CKademlia::SaveData()
{
	wstring ConfigPath = Cfg()->GetString("ConfigPath");
	if(!ConfigPath.empty())
	{
		CVariant Data;
		CVariant Info;
		Info["VER"] = GetVersion();
		Info["PK"] = CVariant(m_pKadKey->GetKey(), m_pKadKey->GetSize());
		Data["INFO"] = Info;

		WriteFile(ConfigPath + L"NeoKad.dat", Data);
	}
}

void CKademlia::Process(UINT Tick)
{
	m_pSocket->Process((Tick & EPerSec) != 0);

	if(!m_pKadHandler)
		return;

	if((Tick & E2PerSec) != 0) // 2 times a second
	{
		m_pFirewallHandler->Process(Tick);

		m_pKadHandler->Process(Tick);
		m_pRootZone->Process(Tick);
			
		m_pPayloadIndex->Process(Tick);
	}

#ifndef _DEBUG
	if(IsConnected())
#endif
	{
		m_pLookupManager->Process(Tick);

		if((Tick & E10PerSec) != 0) // 10 times a second
			m_pKadEngine->Process(Tick);

		if((Tick & EPer10Sec) != 0) // every 10 seconds
		{	
			if(GetCurTick() - m_uLastSave > SEC2MS(Cfg()->GetInt("NodeSaveInterval")))
			{
				SaveNodes();
				m_uLastSave = GetCurTick();
			}
		}
	}
}

void CKademlia::Connect()
{
	if(m_pKadHandler)
		return;

	m_pKadHandler = new CKadHandler(m_pSocket, this);
	m_pRootZone = new CRoutingRoot(m_pKadKey, this);
	m_pFirewallHandler = new CFirewallHandler(m_pSocket, this);
	LoadNodes();
	m_uLastSave = GetCurTick();

	m_pLookupManager = new CLookupManager(this);
	m_pPayloadIndex = new CPayloadIndex(this);

	m_pKadEngine = new CKadEngine(this);

	LogLine(LOG_INFO, L"Neo Kad Started");
}

void CKademlia::Disconnect()
{
	if(!m_pKadHandler)
		return;

	m_pKadEngine = NULL;

	m_pLookupManager = NULL;
	m_pPayloadIndex = NULL;

	m_pFirewallHandler = NULL;
	SaveNodes();
	m_pKadHandler = NULL;
	m_pRootZone = NULL;

	LogLine(LOG_INFO, L"Neo Kad Stopped");
}

bool CKademlia::IsConnected() const
{
	if(!m_pKadHandler)
		return false;
	return m_pRootZone->GetNodeCount() > 0 && (GetTime() - m_pKadHandler->GetLastContact()) < (Cfg()->GetInt64("SelfLookupInterval") * 2);
	//return !m_pFirewallHandler->AddrPool().empty();
}

size_t CKademlia::GetNodeCount() const
{
	if(m_pRootZone)
		return m_pRootZone->GetNodeCount();
	return 0;
}

time_t CKademlia::GetLastContact() const
{
	if(m_pKadHandler)
		return m_pKadHandler->GetLastContact();
	return 0;
}

int CKademlia::GetUpRate()
{
	return m_pSocket->GetUpLimit()->GetRate();
}

int CKademlia::GetDownRate()
{
	return m_pSocket->GetDownLimit()->GetRate();
}

bool CKademlia::HasScript(const CVariant& CodeID) const
{
	if(m_pKadEngine)
		return m_pKadEngine->GetScript(CodeID) != NULL;
	return false;
}

void CKademlia::QueryScripts(list<SKadScriptInfo>& Scripts) const
{
	if(!m_pKadHandler)
		return;

	const ScriptMap& ScriptMap = m_pKadEngine->GetScripts();
	for(ScriptMap::const_iterator I = ScriptMap.begin(); I != ScriptMap.end(); I++)
	{
		SKadScriptInfo KadScriptInfo;
		KadScriptInfo.CodeID = I->first;
		KadScriptInfo.Name = I->second->GetName();
		KadScriptInfo.Version = I->second->GetVersion();
		// K-ToDo: add more info
		Scripts.push_back(KadScriptInfo);
	}
}

bool CKademlia::InstallScript(const CVariant& CodeID, const string& Source, const CVariant& Authentication)
{
	if(!m_pKadHandler)
		return false;

	return m_pKadEngine->Install(CodeID, Source, Authentication);
}

CVariant CKademlia::StartLookup(const CUInt128& TargetID, const CVariant& CodeID, const TCallMap& Execute, const TStoreMap& Store, CPrivateKey* pStoreKey, const TLoadMap& Load
									, int Timeout, int HopLimit, int JumpCount, int SpreadCount, bool bTrace, const wstring& Name)
{
	if(!m_pKadHandler)
		return CVariant();

	if(!CodeID.IsValid() && Store.empty() && Load.empty())
	{
		LogLine(LOG_ERROR, L"Attempted to start an empty Lookup");
		return CVariant();
	}

	CPointer<CKadTask> pLookup = new CKadTask(TargetID, m_pLookupManager);
	pLookup->SetName(Name);

	if(Timeout != -1)
		pLookup->SetTimeOut(Timeout);

	if(HopLimit != -1)
		pLookup->SetHopLimit(HopLimit);
	if(JumpCount != -1)
		pLookup->SetJumpCount(JumpCount);
	if(SpreadCount != -1)
		pLookup->SetSpreadCount(SpreadCount);
	if(bTrace)
		pLookup->EnableTrace();

	if(pStoreKey)
		pLookup->SetStoreKey(pStoreKey);

	for(TStoreMap::const_iterator I = Store.begin(); I != Store.end(); I++)
		pLookup->Store(I->first, I->second.Path, I->second.Data);
	
	for(TLoadMap::const_iterator I = Load.begin(); I != Load.end(); I++)
		pLookup->Load(I->first, I->second.Path);

	if(CodeID.IsValid())
	{
		if(!pLookup->SetupScript(CodeID))
		{
			LogLine(LOG_ERROR, L"Attempted to start a smart lookup with an unavailable script: %s", ToHex(CodeID.GetData(), CodeID.GetSize()).c_str());
			return CVariant();
		}
	
		for(TCallMap::const_iterator I = Execute.begin(); I != Execute.end(); I++)
			pLookup->AddCall(I->second.Function, I->second.Parameters, I->first);
	}

	CVariant LookupID = m_pLookupManager->StartLookup(pLookup.Obj());

	return LookupID;
}

CVariant CKademlia::QueryLookup(const CVariant& LookupID, TRetMap& Results, TStoredMap& Stored, TLoadedMap& Loaded)
{
	if(!m_pKadHandler)
		return CVariant();

	CVariant Lookup;
	if(CKadTask* pLookup = m_pLookupManager->GetLookup(LookupID)->Cast<CKadTask>())
	{
		CVariant LookupInfo;

		pLookup->QueryStored(Stored);
		pLookup->QueryLoaded(Loaded);
		pLookup->QueryResults(Results);

		LookupInfo["Duration"] = pLookup->GetDuration();
		// K-ToDo-Now: add some info to be returned: age, bytes up bytes down, etc...

		Lookup["Info"] = LookupInfo;
		if(pLookup->IsStopped())		Lookup["Staus"] = "Finished";
		else if(pLookup->GetStartTime())Lookup["Staus"] = "Running";
		else							Lookup["Staus"] = "Pending";
	}
	else
		Lookup["Staus"] = "Finished";
	return Lookup;
}

void CKademlia::StopLookup(const CVariant& LookupID)
{
	if(!m_pKadHandler)
		return;

	if(CKadLookup* pLookup = m_pLookupManager->GetLookup(LookupID))
		m_pLookupManager->StopLookup(pLookup);
}

CUInt128 CKademlia::MakeCloseTarget(int *pDistance)
{
	if(!m_pKadHandler)
	{
		if(pDistance)
			*pDistance = -1;
		return 0;
	}

	CUInt128 uMyID = m_pRootZone->GetID();
	NodeMap Nodes;
	m_pRootZone->GetClosestNodes(uMyID, Nodes, Cfg()->GetInt("BucketSize"));
	if(Nodes.size() < 2)
	{
		if(pDistance)
			*pDistance = -1;
		return 0;
	}
	
	// Find Median distance difference between nodes closest to us
	vector<CUInt128> Diff;
	for(NodeMap::iterator np = Nodes.begin(), n = np++; np != Nodes.end(); n = np++)
		Diff.push_back(np->first - n->first);
	CUInt128 Sep = Median(Diff);

	// generate ID that is closer to us than the closest node by a few difference
	CUInt128 uDistance = Nodes.begin()->first;
	for(int i=0; i < 3 && uDistance > Sep; i++)
		uDistance = uDistance - Sep;
	CUInt128 uCloser = uMyID ^ uDistance;

	// count the matchign bits
	UINT uLevel=0;
	for(; uLevel < uMyID.GetBitSize(); uLevel++)
	{
		if(uCloser.GetBit(uLevel) != uMyID.GetBit(uLevel))
			break;
	}

	// add a few more matching bytes
	for(UINT i=0; i < 4 && uLevel < uMyID.GetBitSize() - 1; i++)
	{
		uCloser.SetBit(uLevel, uMyID.GetBit(uLevel));
		uLevel++;
	}

	if(pDistance)
		*pDistance = (int)uMyID.GetBitSize() - uLevel;

	// create a random ID that we are closest to
	CUInt128 uRandom(uCloser, uLevel);
	//wstring sTest = (uMyID ^ uRandom).ToBin();
	return uRandom;
}

CVariant CKademlia::SetupRoute(const CUInt128& TargetID, CPrivateKey* pEntityKey, int HopLimit, int JumpCount, bool bTrace)
{
	if(!m_pKadHandler)
		return CVariant();

	CPointer<CKadRoute> pRoute = new CKadRouteImpl(TargetID, pEntityKey, m_pLookupManager);

	if(HopLimit != -1)
		pRoute->SetHopLimit(HopLimit);
	if(JumpCount != -1)
		pRoute->SetJumpCount(JumpCount);
	if(bTrace)
		pRoute->EnableTrace();

	if(CKadRoute* pOldRoute = m_pLookupManager->GetRelay(pRoute->GetEntityID())->Cast<CKadRoute>())
		m_pLookupManager->StopLookup(pOldRoute);

	m_pLookupManager->StartLookup(pRoute.Obj());

	return pRoute->GetEntityID();
}

CPrivateKey* CKademlia::GetRouteKey(const CVariant& MyEntityID)
{
	if(!m_pKadHandler)
		return NULL;

	CKadRoute* pRoute = m_pLookupManager->GetRelay(MyEntityID)->Cast<CKadRoute>();
	if(pRoute)
		return pRoute->GetPrivateKey();
	return NULL;
}

CVariant CKademlia::QueryRoute(const CVariant& MyEntityID)
{
	if(!m_pKadHandler)
		return CVariant();

	CVariant Route;
	if(CKadRoute* pRoute = m_pLookupManager->GetRelay(MyEntityID)->Cast<CKadRoute>())
	{
		pRoute->Refresh();

		Route["Duration"] = pRoute->GetDuration();
		// K-ToDo-Now: add some info to be returned: age, bytes up bytes down, etc...

		Route["Status"] = "Established";
	}
	else
		Route["Status"] = "Broken";
	return Route;
}

CVariant CKademlia::OpenSession(const CVariant& MyEntityID, const CVariant& EntityID, const CUInt128& TargetID)
{
	if(!m_pKadHandler)
		return false;

	CKadRouteImpl* pRoute = m_pLookupManager->GetRelay(MyEntityID)->Cast<CKadRouteImpl>();
	if(!pRoute)
		return false;
	return pRoute->OpenSession(EntityID, TargetID)->GetSessionID();
}

bool CKademlia::QueueBytes(const CVariant& MyEntityID, const CVariant& EntityID, const CVariant& SessionID, const CBuffer& Buffer, bool bStream)
{
	if(!m_pKadHandler)
		return false;

	CKadRouteImpl* pRoute = m_pLookupManager->GetRelay(MyEntityID)->Cast<CKadRouteImpl>();
	if(!pRoute)
		return false;
	return pRoute->QueueBytes(EntityID, SessionID, Buffer, bStream);
}

bool CKademlia::QuerySessions(const CVariant& MyEntityID, list<SRouteSession>& Sessions)
{
	if(!m_pKadHandler)
		return false;

	CKadRouteImpl* pRoute = m_pLookupManager->GetRelay(MyEntityID)->Cast<CKadRouteImpl>();
	if(!pRoute)
		return false;
	pRoute->QuerySessions(Sessions);
	return true;
}

bool CKademlia::PullBytes(const CVariant& MyEntityID, const CVariant& EntityID, const CVariant& SessionID, CBuffer& Buffer, bool &bStream, size_t MaxBytes)
{
	if(!m_pKadHandler)
		return false;

	CKadRouteImpl* pRoute = m_pLookupManager->GetRelay(MyEntityID)->Cast<CKadRouteImpl>();
	if(!pRoute)
		return false;
	return pRoute->PullBytes(EntityID, SessionID, Buffer, bStream, MaxBytes);
}

bool CKademlia::CloseSession(const CVariant& MyEntityID, const CVariant& EntityID, const CVariant& SessionID)
{
	if(!m_pKadHandler)
		return false;

	CKadRouteImpl* pRoute = m_pLookupManager->GetRelay(MyEntityID)->Cast<CKadRouteImpl>();
	if(!pRoute)
		return false;
	return pRoute->CloseSession(EntityID, SessionID);
}

void CKademlia::BreakRoute(const CVariant& MyEntityID)
{
	if(!m_pKadHandler)
		return;

	if(CKadRoute* pRoute = m_pLookupManager->GetRelay(MyEntityID)->Cast<CKadRoute>())
		m_pLookupManager->StopLookup(pRoute);
}

bool CKademlia::InvokeScript(const CVariant& CodeID, const string& Function, const CVariant& Arguments, CVariant& Result, const CVariant& CallerID)
{
	CKadScript* pKadScript = m_pKadEngine->GetScript(CodeID);
	if(!pKadScript)
		return false;

	CDebugScope Debug(pKadScript);

	vector<CPointer<CObject> > Parameters;
	Parameters.push_back(new CVariantPrx(Arguments));
	if(CallerID.IsValid())
		Parameters.push_back(new CVariantPrx(CallerID));

	try
	{
		CJSScript* pScript = pKadScript->GetJSScript(true);

		CPointer<CObject> Return;
		pScript->Call(string("localAPI"), Function, Parameters, Return);
		if(CVariantPrx* pVariant = Return->Cast<CVariantPrx>())
			Result = pVariant->GetCopy();
	}
	catch(const CJSException& Exception)
	{
		pKadScript->LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
		return false;
	}
	return true;
}

// Note: for now the Index API allows only access to the public index

bool CKademlia::Store(const CUInt128& TargetID, const string& Path, const CVariant& Data, time_t Expire)
{
	return m_pPayloadIndex->Store(TargetID, Path, Data, Expire);
}

void CKademlia::List(const string& Path, vector<SKadEntryInfo>& Entrys)
{
	m_pPayloadIndex->List(0, Path, Entrys);
}

bool CKademlia::List(const CUInt128& TargetID, const string& Path, vector<SKadEntryInfo>& Entrys)
{
	return m_pPayloadIndex->List(TargetID, Path, Entrys);
}

CVariant CKademlia::Load(uint64 Index)
{
	return m_pPayloadIndex->Load(Index);
}

void CKademlia::Remove(uint64 Index)
{
	m_pPayloadIndex->Remove(Index);
}

CUInt128 CKademlia::GetID() const
{
	if(!m_pRootZone)
		return 0;
	return m_pRootZone->GetID();
}

uint16 CKademlia::GetPort() const
{
	return m_Port;
}

CAddress CKademlia::GetIPv4() const
{
	return m_IPv4;	
}

CAddress CKademlia::GetIPv6() const
{
	return m_IPv6;
}

bool CKademlia::BindSockets(uint16 uPort, const CAddress& IPv4, const CAddress& IPv6)
{
	m_Port = uPort;
	m_IPv4 = IPv4;
	m_IPv6 = IPv6;

	bool bOK = true;
	CSmartSocket::ListnerMap& Listners = m_pSocket->GetListners();
	for(CSmartSocket::ListnerMap::iterator I = Listners.begin(); I != Listners.end(); I++)
	{
		switch(I->first)
		{
			case CSafeAddress::eUTP_IP4:
				((CUTPSocketListner*)I->second)->Close();
				if(IPv4.Type() != CAddress::None)
					bOK &= ((CUTPSocketListner*)I->second)->Bind(uPort, IPv4);	
				break;
			case CSafeAddress::eUTP_IP6:
				((CUTPSocketListner*)I->second)->Close();
				if(IPv6.Type() != CAddress::None)
					bOK &= ((CUTPSocketListner*)I->second)->Bind(uPort, IPv6);	
				break;
		}
	}
	return bOK;
}

uint16 CKademlia::GetPort(UINT eProtocol)
{
	switch(eProtocol)
	{
		case CSafeAddress::eUTP_IP4:
		case CSafeAddress::eUTP_IP6:
			return GetPort();

		case CSafeAddress::eUDT_IP4:
		case CSafeAddress::eUDT_IP6:
			return 0;

		case CSafeAddress::eTCP_IP4:
		case CSafeAddress::eTCP_IP6:
			return 0;
	}
	return 0;
}

CSafeAddress CKademlia::GetAddress(UINT Protocol) const
{
	if(m_pFirewallHandler)
	{
		if(const CMyAddress* pAddress = m_pFirewallHandler->GetAddress((CSafeAddress::EProtocol)Protocol))
			return *pAddress;
	}
	return CSafeAddress();
}

EFWStatus CKademlia::GetFWStatus(UINT Protocol) const
{
	if(m_pFirewallHandler)
		return m_pFirewallHandler->GetFWStatus((CSafeAddress::EProtocol)Protocol);
	return eFWOpen;
}

uint32 CKademlia::LoadNodes()
{
	wstring ConfigPath = Cfg()->GetString("ConfigPath");
	if(ConfigPath.empty())
		return 0;

	uint32 Count = 0;
	try
	{
		CVariant NeoNodes;
		ReadFile(ConfigPath + L"NeoNodes.dat", NeoNodes);
		const CVariant& NodeList = NeoNodes["Nodes"];
		for(uint32 i=0; i < NodeList.Count(); i++)
		{
			CPointer<CKadNode> pNode = new CKadNode(m_pRootZone);

			pNode->Load(NodeList.At(i), true);
			if(pNode->GetAddresses().empty())
				continue;

			Count++;
			m_pRootZone->AddNode(pNode);
		}
		m_pKadHandler->SetLastContact(NeoNodes["LastContact"]);
	}
	catch(const CException&)
	{
		ASSERT(0);
	}
	return Count;
}

void CKademlia::SaveNodes()
{
	wstring ConfigPath = Cfg()->GetString("ConfigPath");
	if(ConfigPath.empty())
		return;

	NodeList Nodes = m_pRootZone->GetAllNodes();

	CVariant NodeList;
	for (NodeList::iterator I = Nodes.begin(); I != Nodes.end(); I++)
	{
		CKadNode* pNode = *I;
		NodeList.Append(pNode->Store(true));
	}
	CVariant NeoNodes;
	NeoNodes["Nodes"] = NodeList;
	NeoNodes["LastContact"] = m_pKadHandler->GetLastContact();
	WriteFile(ConfigPath + L"NeoNodes.dat", NeoNodes);
}
