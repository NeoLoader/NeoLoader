#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "KadEngine.h"
#include "KadScript.h"
#include "../../Common/v8Engine/JSEngine.h"
#include "../../Common/v8Engine/JSDebug.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "../../Common/v8Engine/JSDataStore.h"
#include "../../Common/v8Engine/JSCryptoKey.h"
#include "../../Common/v8Engine/JSHashing.h"
#include "../../Common/v8Engine/JSBuffer.h"
#include "JSKadScript.h"
#include "JSKademlia.h"
#include "JSKadNode.h"
#include "JSKadRoute.h"
#include "JSRouteSession.h"
#include "JSPayloadIndex.h"
#include "JSKadLookup.h"
#include "JSKadRequest.h"
#include "JSBinaryCache.h"
#include "JSBinaryBlock.h"
#include "../Kademlia.h"
#include "../KadConfig.h"
#include "../KadID.h"
#include "JSKadID.h"
#include "../../../Framework/Cryptography/AsymmetricKey.h"
#include "../../../Framework/Strings.h"
#include "../../Common/FileIO.h"

//////////////////////////////////////////////////////
//

IMPLEMENT_OBJECT(CKadEngine, CObject)

bool CKadEngine::m_b8InitDone = false;

CKadEngine::CKadEngine(CObject* pParent)
: CObject(pParent)
{
	if(!m_b8InitDone)
	{
		m_b8InitDone = true;

		CJSEngine::RegisterObj<CJSDebug>();
		CJSEngine::RegisterObj<CJSVariant>("Variant");
		CJSEngine::RegisterObj<CJSSymKey>("SymKey");
		CJSEngine::RegisterObj<CJSPrivKey>("PrivKey");
		CJSEngine::RegisterObj<CJSPubKey>("PubKey");
		CJSEngine::RegisterObj<CJSHashing>("HashFkt");
		CJSEngine::RegisterObj<CJSBuffer>("Buffer");
		CJSEngine::RegisterObj<CJSDataStore>();

		CJSEngine::RegisterObj<CJSKadID>("KadID");
		CJSEngine::RegisterObj<CJSKadScript>();
		CJSEngine::RegisterObj<CJSKademlia>();
		CJSEngine::RegisterObj<CJSKadNode>();
		CJSEngine::RegisterObj<CJSKadRoute>();
		CJSEngine::RegisterObj<CJSRouteSession>();
		CJSEngine::RegisterObj<CJSPayloadIndex>();
		CJSEngine::RegisterObj<CJSKadLookup>();
		CJSEngine::RegisterObj<CJSKadRequest>();
		CJSEngine::RegisterObj<CJSBinaryCache>();
		CJSEngine::RegisterObj<CJSBinaryBlock>();
	}

	// instantiate binary cache is enabled
	if(!GetParent<CKademlia>()->Cfg()->GetString("BinaryCachePath").empty())
		m_BinaryCache = new CBinaryCache(this);

	wstring ScriptPath = GetParent<CKademlia>()->Cfg()->GetString("ScriptCachePath");
	if(!ScriptPath.empty())
	{
		vector<wstring> Scripts;
		ListDir(ScriptPath, Scripts, L"*.js");
		for(size_t i=0; i < Scripts.size(); i++)
		{
			string Source;
			ReadFile(Scripts[i], Source);

			map<string, string> HeaderFields = CKadScript::ReadHeader(Source);
			uint32 Version = CKadScript::GetVersion(s2w(HeaderFields["Version"]));
			wstring Name = s2w(HeaderFields["Name"]);
			if(Version == 0 || Name.empty())
				continue; // Header is mandatory

			wstring BaseName = Split2(Scripts[i], L".").first;
			CVariant CodeID = FromHex(Split2(BaseName, L"/", true).second);

			CScoped<CKadScript> pScript = new CKadScript(CodeID, this);

			pScript->LoadData(BaseName + L".dat");

			if(pScript->IsAuthenticated())
			{
				if(!Authenticate(CodeID, pScript->GetAuthentication(), Source))
					continue;
			}

			pScript->SetSource(Source, Name, Version);

			m_Scripts.insert(ScriptMap::value_type(CodeID, pScript.Detache()));
		}
	}

	m_uLastSave = GetCurTick();
}

CKadEngine::~CKadEngine()
{
	SaveData();

	// Cleanup memory
	m_BinaryCache = NULL;
	m_Scripts.clear();

	HardReset();
}

void CKadEngine::Process(UINT Tick)
{
	wstring ScriptPath = GetParent<CKademlia>()->Cfg()->GetString("ScriptCachePath");

	for(ScriptMap::iterator I = m_Scripts.begin(); I != m_Scripts.end();)
	{
		if(!I->second->Process(Tick))
		{
			if(!ScriptPath.empty())
			{
				wstring BaseName = ScriptPath + L"/" + ToHex(I->second->GetCodeID().GetData(), I->second->GetCodeID().GetSize());
				RemoveFile(BaseName + L".js");
				RemoveFile(BaseName + L".dat");
			}

			I = m_Scripts.erase(I);
		}
		else
			I++;
	}

	if(GetTotalMemory() > (size_t)GetParent<CKademlia>()->Cfg()->GetInt64("MaxTotalMemory")) 
		HardReset(); // we dont trace currently who is teh guilty, so we reset the whole engine

	if(GetCurTick() - m_uLastSave > SEC2MS(GetParent<CKademlia>()->Cfg()->GetInt("ScriptSaveInterval")))
	{
		SaveData();
		m_uLastSave = GetCurTick();
	}

	if(m_BinaryCache)
		m_BinaryCache->Process(Tick);
}

void CKadEngine::HardReset()
{
	LogLine(LOG_WARNING, L"Reseting VS Engine");

	// terminate all scripts
	for(ScriptMap::iterator I = m_Scripts.begin(); I != m_Scripts.end(); I++)
		I->second->Terminate();

	// force garbage collection
	v8::Locker Lock(v8::Isolate::GetCurrent());
	v8::Isolate::GetCurrent()->LowMemoryNotification();
	while (!v8::Isolate::GetCurrent()->IdleNotification(100)) {};

	/*extern map<string, set<void*> > g_CountMap;
	for(map<string, set<void*> >::iterator I = g_CountMap.begin(); I != g_CountMap.end(); I++)
	{
		if(!I->second.empty())
		{
			int Count = 0;
			for(set<void*>::iterator J = I->second.begin(); J != I->second.end(); J++)
			{
				CObject* pObject = (CObject*)(*J);
				Count++;
			}
			LogLine(LOG_DEBUG, L"%S: %i", I->first.c_str(), Count);
		}
	}*/
}

void CKadEngine::SaveData()
{
	wstring ScriptPath = GetParent<CKademlia>()->Cfg()->GetString("ScriptCachePath");
	if(ScriptPath.empty())
		return;
	
	for(ScriptMap::iterator I = m_Scripts.begin(); I != m_Scripts.end(); I++)
	{
		wstring BaseName = ScriptPath + L"/" + ToHex(I->second->GetCodeID().GetData(), I->second->GetCodeID().GetSize());
		I->second->SaveData(BaseName + L".dat");
	}
}

bool CKadEngine::Authenticate(const CVariant& CodeID, const CVariant& Authentication, const string& Source)
{
	CScoped<CPublicKey> pPubKey = new CPublicKey();
	if(!pPubKey->SetKey(Authentication["PK"].GetData(), Authentication["PK"].GetSize()))
	{
		LogLine(LOG_ERROR, L"Invalid Key for Script");
		return false;
	}

	CVariant TestID((byte*)NULL, CodeID.GetSize());
	UINT eHashFunkt = Authentication.Has("HK") ? CAbstractKey::Str2Algorithm(Authentication["HK"]) : CAbstractKey::eUndefined;
	CKadID::MakeID(pPubKey, TestID.GetData(), TestID.GetSize(), eHashFunkt);

	if(CodeID != TestID)
	{
		LogLine(LOG_ERROR, L"Wrong Key for Script");
		return false;
	}

	if(!pPubKey->Verify((byte*)Source.data(), Source.size(), Authentication["SIG"].GetData(), Authentication["SIG"].GetSize()))
	{
		LogLine(LOG_ERROR, L"Script with an invalid signature");
#ifndef _DEBUG
		return false;
#endif
	}

	return true;
}

string CKadEngine::Install(const CVariant& InstallReq)
{
	const CVariant& CodeID = InstallReq["CID"];
	string Source = InstallReq["SRC"].To<string>();
	const CVariant Authentication = InstallReq.Get("AUTH");
	bool bAuthenticated = false;
	if(Authentication.IsValid())
	{
		if(!Authenticate(CodeID, Authentication, Source))
		{
			LogLine(LOG_ERROR | LOG_DEBUG, L"Script Authentication Failed");
			return "InvalidSign";
		}
		bAuthenticated = true;
	}

	CKadScript* pScript = NULL;
	ScriptMap::iterator I = m_Scripts.find(CodeID);
	if(I == m_Scripts.end())
	{
		pScript = new CKadScript(CodeID, this);
		m_Scripts.insert(ScriptMap::value_type(CodeID, pScript));
	}
	else
		pScript = I->second;

	ASSERT(pScript);
	if(pScript->IsAuthenticated() && !bAuthenticated)
	{
		LogLine(LOG_ERROR | LOG_DEBUG, L"Required Script Authentication Missing");
		return "MissingSign";
	}

	int Ret = pScript->Setup(Source, bAuthenticated);
	if(!Ret)
		return "SetupFailed";

	if(Ret == 1 && bAuthenticated)
		pScript->SetAuthentication(Authentication);
	return "";
}

bool CKadEngine::Install(const CVariant& CodeID, const string& Source, const CVariant& Authentication)
{
	bool bAuthenticated = false;
	if(Authentication.IsValid())
	{
		if(!Authenticate(CodeID, Authentication, Source))
			return false;
		bAuthenticated = true;
	}

	CKadScript* pScript = NULL;
	ScriptMap::iterator I = m_Scripts.find(CodeID);
	if(I == m_Scripts.end())
	{
		pScript = new CKadScript(CodeID, this);
		m_Scripts.insert(ScriptMap::value_type(CodeID, pScript));
	}
	else
		pScript = I->second;

	int Ret = pScript->Setup(Source, bAuthenticated);
	if(Ret <= 0)
		return false;

	if(Ret == 1 && Authentication.IsValid())
		pScript->SetAuthentication(Authentication);
	return true;
}

void CKadEngine::Remove(const CVariant& CodeID)
{
	ScriptMap::iterator I = m_Scripts.find(CodeID);
	if(I != m_Scripts.end())
	{
		wstring ScriptPath = GetParent<CKademlia>()->Cfg()->GetString("ScriptCachePath");
		if(!ScriptPath.empty())
		{
			wstring BaseName = ScriptPath + L"/" + ToHex(I->second->GetCodeID().GetData(), I->second->GetCodeID().GetSize());
			RemoveFile(BaseName + L".js");
			RemoveFile(BaseName + L".dat");
		}
		m_Scripts.erase(I);
	}
}

CKadScript* CKadEngine::GetScript(const CVariant& CodeID)
{
	ScriptMap::iterator I = m_Scripts.find(CodeID);
	if(I == m_Scripts.end() || !I->second->IsValid())
		return NULL;
	return I->second;
}

size_t CKadEngine::GetTotalMemory()
{
	return CJSEngine::GetTotalMemory();
}
