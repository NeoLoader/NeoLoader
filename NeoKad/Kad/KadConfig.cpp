#include "GlobalHeader.h"
#include "KadHeader.h"
#include "KadConfig.h"

IMPLEMENT_OBJECT(CKadConfig, CObject)

CKadConfig::CKadConfig(CObject* pParent)
: CObject(pParent)
{
	m_Config["AddressTimeout"] = 3;				// timeout for address test packets
	m_Config["AddressChecks"] = 3;					// address tests to start
	m_Config["AddressRatio"] = 60;
	m_Config["AddressConRatio"] = 30;
	m_Config["AddressInterval"] = MIN2S(60);		// interval to recheck he current IP address, to detect reconnects/changes
	m_Config["MaxTunnels"] = 10;

	m_Config["ConnectionTimeout"] = 60;

	m_Config["BucketSize"] = 10;					// K
	m_Config["ZoneLimit"] = 5;						// KK
	m_Config["LevelLimit"] = 4;						// KBASE

	m_Config["NodeLookupInterval"] = MIN2S(45);		// lookup random node from a bucket in this invervals
	m_Config["LookupTimeout"] = 25;
	m_Config["MaxNodeLookups"] = 3;					// 

	m_Config["NodeSaveInterval"] = MIN2S(15);
	m_Config["IndexSaveInterval"] = MIN2S(15);

	m_Config["Age1stClass"] = MIN2S(120);			// age fo a node after successfull conenct to considder it 1nd class
	m_Config["Age2ndClass"] = MIN2S(60);			// age fo a node after successfull conenct to considder it 2nd class
	m_Config["RetentionTime"] = MIN2S(30);			// how long to keep cached nodes in the tree
	m_Config["KeepZeroClass"] = MIN2S(120);			// 3 times or more
	m_Config["Keep1ndClass"] = MIN2S(90);			// connected twise
	m_Config["Keep2ndClass"] = MIN2S(60);			// connected once
	m_Config["KeepDefaultClass"] = MIN2S(5);		// unconnected node class

	m_Config["NodeReqCount"] = 10;					// amount of nodes to request

	m_Config["CheckoutInterval"] = 3;
	m_Config["ConsolidateInterval"] = MIN2S(30);	// interval for Zone consolidation
	m_Config["SelfLookupInterval"] = MIN2S(45);		// lookup own node in this invervals
	m_Config["HelloInterval"] = MIN2S(10);			// minimum interval in which it is alowed to send a hello packet to a node

	m_Config["MaxFrameTTL"] = 3000;					// time a frame is alowed to live, after which the route resends the frame from the begin, and relays drop it
	m_Config["MinFrameTTL"] = 500;					
	m_Config["MaxResend"]	= 3;					// max amount of resends befoure considdering a route broken
	m_Config["MaxRelayTimeout"] = 1000;				// timeout for first frame in ms
	m_Config["MinRelayTimeout"] = 250;				
	m_Config["SegmentSize"] = KB2B(4);				// frame size for a stream segment
	m_Config["MaxWindowSize"] = 64;					// maximal count of frames that can be in transit between two stations at one time
	m_Config["RouteTimeout"] = MIN2S(1);			// time after which a route is droped if it wasnt refreshed, a refresh is issued at the half of this time
	//m_Config["MaxPendingFrames"] = 10;				// amount of frames that can be on route end to end for each session

	m_Config["RequestTimeOut"] = 5;					// delay to force a jumpstart sonce last incomming results
	m_Config["BrancheCount"] = 2;
	m_Config["SpreadCount"] = 10;

	m_Config["MaxLookupTimeout"] = MIN2S(5);					// delay to force a jumpstart sonce last incomming results
	m_Config["MaxBrancheCount"] = 5;
	m_Config["MaxSpreadCount"] = 25;
	m_Config["MaxHopLimit"] = 25;
	m_Config["MaxJumpCount"] = 5;

	m_Config["KeepTrace"] = 10;
	m_Config["TraceTimeOut"] = MIN2S(15);

	m_Config["ConfigPath"] = L"";
	m_Config["IndexCleanupInterval"] = MIN2S(60);	// interval for paylaod index cleanup
	m_Config["MaxPayloadExpiration"] = DAY2S(90);
	m_Config["MaxPayloadReturn"] = 1000;
	m_Config["MaxScriptExpiration"] = DAY2S(90);
	m_Config["MaxScriptIdleTime"] = MIN2S(5);
	m_Config["BinaryCachePath"] = L"";
	m_Config["ScriptCachePath"] = L"";
	m_Config["ScriptSaveInterval"] = MIN2S(15);
	m_Config["CacheCleanupInterval"] = MIN2S(15);
	m_Config["CacheRetentionTime"] = DAY2S(10);
	// K-ToDo-Now: script and payload size hard limit
	m_Config["LoadInterval"] = HR2S(3);
	m_Config["MaxTotalMemory"] = MB2B(512); // Note: V8 is by default limited to 1GB of memory


	m_Config["DebugFW"] = true;
	m_Config["DebugTL"] = false; // transport Lyer
	m_Config["DebugRT"] = false; // routing tree maintenence
	m_Config["DebugLU"] = true;	// lookup prozedures
	m_Config["DebugRU"] = true; // tunel routing
	m_Config["DebugRE"] = false; // data realying through tunnels
}

CVariant CKadConfig::GetSetting(const char* Name) const
{
	if(m_Config.Has(Name))
		return m_Config[Name];
	ASSERT(0);
	return CVariant();
}

void CKadConfig::SetSetting(const char* Name, const CVariant& Value)
{
	if(m_Config.Has(Name))
		m_Config[Name] = Value;
	else{
		ASSERT(0);
	}
}

void CKadConfig::Merge(const CVariant& Config)
{
	for(uint32 i=0; i < Config.Count(); i++)
		SetSetting(Config.Key(i).c_str(), Config.At(i));
}