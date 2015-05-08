#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "KadScript.h"
#include "JSKadScript.h"
#include "KadEngine.h"
#include "../Kademlia.h"
#include "../KadID.h"
#include "../KadConfig.h"
#include "../KadHandler.h"
#include "../PayloadIndex.h"
#include "../LookupManager.h"
#include "../KadOperation.h"
#include "../RoutingRoot.h"
#include "JSKademlia.h"
#include "JSKadRoute.h"
#include "JSKadID.h"
#include "JSBinaryCache.h"
#include "JSKadLookup.h"
#include "../../Common/v8Engine/JSScript.h"
#include "../../Common/v8Engine/JSDebug.h"
#include "../../Common/v8Engine/JSVariant.h"
#include "../../Common/v8Engine/JSDataStore.h"
#include "../../../Framework/Strings.h"
#include "../../Common/FileIO.h"
#include "KadDebugging.h"
#include "KadOperator.h"

IMPLEMENT_OBJECT(CKadScript, CObject)

CKadScript::CKadScript(const CVariant& CodeID, CObject* pParent)
: CObject(pParent)
{
	m_CodeID = CodeID;
	m_Version = 0;
	m_LastUsed = GetTime();

	m_Data = new CDataStoreObj();
}

CKadScript::~CKadScript()
{
	Terminate();
}

bool CKadScript::IsValid() const
{
	return !m_Source.empty() && m_Version;
}

bool CKadScript::Process(UINT Tick)
{
	time_t uIdleTime = GetTime() - m_LastUsed;
	bool bExpired = uIdleTime > GetParent<CKademlia>()->Cfg()->GetInt("MaxScriptExpiration");
	if(m_pScript)
	{
		CJSKadScript* pJSKadScript = (CJSKadScript*)m_pScript->GetJSObject(this);
		// We take this cast on faith, the way object constructros are registered should ensure that we wil get the right object
		ASSERT(pJSKadScript); 

		CDebugScope Debug(this);

		try
		{
			pJSKadScript->RunTimers(m_pScript);
		}
		catch(const CJSException& Exception)
		{
			LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());
		}

		if(m_pRoute) // refresh the route as long as the script is alive
			m_pRoute->Refresh();

		if(bExpired || uIdleTime > GetParent<CKademlia>()->Cfg()->GetInt("MaxScriptIdleTime"))
			Terminate();
	}
	return !bExpired;
}

void CKadScript::LineLogger(UINT Flags, const wstring& Line)
{
	CKadScript* pScript = (CKadScript*)CDebugScope::Scope().first;
	CObject* pObject = (CObject*)CDebugScope::Scope().second;
	if(CKadOperation* pOperation = pObject->Cast<CKadOperation>())
		pOperation->LogReport(Flags, Line);
	else
		pScript->LogReport(Flags, Line);
}

void CKadScript::LogReport(UINT Flags, const wstring& ErrorMessage, const string& Error)
{
	wstring Operation = L"Kad Script";
	Operation += L" (" + GetName() + L" v" + CKadScript::GetVersion(GetVersion()) + L")";

	if(!Error.empty())
		LogLine(LOG_DEBUG | Flags, L"%s caused an Error: %s", Operation.c_str(), ErrorMessage.c_str());
	else
		LogLine(LOG_DEBUG | Flags, L"%s Reports: %s", Operation.c_str(), ErrorMessage.c_str());

	if(m_pLogger)
		m_pLogger->LogReport(Flags, ErrorMessage, Error);
}

CPointer<CJSScript> CKadScript::MakeScript(const string& Source, const wstring& Name, uint32 Version)
{
	CPointer<CJSScript> pScript = new CJSScript(this);

	CDebugScope Debug(this); // this may origined with a operation but is a global event

	wstring FileName = ToHex(m_CodeID.GetData(), m_CodeID.GetSize()) + L"/" + Name;

	map<string, CObject*> Objects;
	Objects["debug"] = new CDebugObj(FileName + L"-v" + GetVersion(Version), LineLogger, this);
	Objects["kademlia"] = GetParent<CKademlia>();
	Objects["script"] = this;
	if(CBinaryCache* pCache = GetParent<CKademlia>()->Engine()->GetCache())
		Objects["cache"] = pCache;
	wstring wSource;
	Utf8ToWStr(wSource, Source);
	pScript->Initialize(wSource, FileName, Objects); // this can fron a CException
	return pScript;
}

void CKadScript::SetSource(const string& Source, const wstring& Name, uint32 Version)
{
	m_Name = Name;
	m_Version = Version;
	m_Source = Source;
}

string CKadScript::WriteHeader(const map<string, string>& HeaderFields)
{
	string Header;
	Header = "/*\r\n";
	for(map<string, string>::const_iterator I = HeaderFields.begin(); I != HeaderFields.end(); I++)
		Header += "\t" + I->first + ": " + I->second + "\r\n";
	Header += "*/";
	return Header;
}

map<string, string> CKadScript::ReadHeader(const string& Source)
{
	map<string, string> HeaderFields;
	string::size_type HeaderBegin = Source.find("/*");
	string::size_type HeaderEnd = Source.find("*/");
	if(HeaderBegin == -1 || HeaderEnd == -1)
		return HeaderFields;
		
	vector<string> HeaderLines = SplitStr(Source.substr(HeaderBegin + 2, HeaderEnd - (HeaderBegin + 2)), "\n");
	for(size_t i=0; i < HeaderLines.size(); i++)
	{
		string Line = Trimm(HeaderLines.at(i));
		if(Line.substr(0,1) == "*")
		{
			Line.erase(0,1);
			Line = Trimm(Line);
		}
		
		pair<string,string> Field = Split2(Line, ":");
		if(Field.first.empty() || Field.first.find(" ") != string::npos)
			continue;

		HeaderFields[Field.first] = Trimm(Field.second);
	}
	return HeaderFields;

}

int CKadScript::Setup(const string& Source, bool bAuthenticated)
{
	ASSERT(!Source.empty());

	wstring FileName = ToHex(m_CodeID.GetData(), m_CodeID.GetSize());
	
	map<string, string> HeaderFields = ReadHeader(Source);
	uint32 Version = CKadScript::GetVersion(s2w(HeaderFields["Version"]));
	wstring Name = s2w(HeaderFields["Name"]);
	if(Version == 0 || Name.empty())
		return 0;

	if(bAuthenticated && !IsAuthenticated()) // we always update an unauthenticated script with an authentic one
		LogLine(LOG_DEBUG | LOG_WARNING, L"Installing authenticated script %s v%s over present v%s", FileName.c_str(), GetVersion(Version).c_str(), GetVersion(m_Version).c_str());
	else
	{
		if(Version < m_Version)
		{
			LogLine(LOG_DEBUG | LOG_WARNING, L"Atempted to install an older v%s script %s than present v%s", GetVersion(Version).c_str(), FileName.c_str(), GetVersion(m_Version).c_str());
			return -2; // report presence of newer script as success
		}

		if(Version == m_Version && Source == m_Source) // Note: for debuging we may want to update without version increment
			return -1; // nothing to do
	}

	LogLine(LOG_DEBUG | LOG_SUCCESS, L"Install script %s version v%s: ", FileName.c_str(), GetVersion(Version).c_str());
	SetSource(Source, Name, Version);
	
	wstring ScriptPath = GetParent<CKademlia>()->Cfg()->GetString("ScriptCachePath");
	if(!ScriptPath.empty())
	{
		WriteFile(ScriptPath + L"/" + FileName + L".js", m_Source);
		SaveData(ScriptPath + L"/" + FileName + L".dat");
	}
	return 1;
}

void CKadScript::SetAuthentication(const CVariant& Authentication)
{
	m_Authentication.Clear();
	m_Authentication = Authentication;

	SaveData();
}

CKadRouteObj* CKadScript::SetupRoute(CAbstractKey* pKey)
{
	if(m_pRoute == NULL)
	{
		CPrivateKey* pEntityKey = NULL;
		if(pKey)
		{
			pEntityKey = new CPrivateKey(pKey->GetAlgorithm());
			pEntityKey->SetKey(pKey);
		}
		m_pRoute = new CKadRouteObj(GetParent<CKademlia>()->Root()->GetID(), pEntityKey, this);

		GetParent<CKademlia>()->Manager()->StartLookup(m_pRoute.Obj());
	}
	return m_pRoute;
}

CJSScript* CKadScript::GetJSScript(bool bInstantiate)
{
	ASSERT(m_pScript || bInstantiate);
	if(!m_pScript && bInstantiate && IsValid())
		m_pScript = MakeScript(m_Source, m_Name, m_Version);
	return m_pScript;
}

void CKadScript::KeepAlive()
{
	m_LastUsed = GetTime();
}

void CKadScript::Terminate()
{
	if(m_pRoute)
	{
		GetParent<CKademlia>()->Manager()->StopLookup(m_pRoute.Obj());
		m_pRoute = NULL;
	}

	if(m_pScript)
	{
		m_pScript = NULL;
	}
}

void CKadScript::SaveData()
{
	wstring FileName = ToHex(m_CodeID.GetData(), m_CodeID.GetSize());

	wstring ScriptPath = GetParent<CKademlia>()->Cfg()->GetString("ScriptCachePath");
	if(!ScriptPath.empty())
		SaveData(ScriptPath + L"/" + FileName + L".dat");
}

void CKadScript::SaveData(const wstring& Path)
{
	CVariant Data;

	Data["LastUsed"] = m_LastUsed;

	Data["Data"] = m_Data->Get();

	if(m_SecretKey)
	{
		Data["KEY"] = CVariant(m_SecretKey->GetKey(), m_SecretKey->GetSize());
		Data["KEY"].Encrypt(GetParent<CKademlia>()->Root()->GetID().GetKey());
	}

	if(m_Authentication.IsValid())
		Data["AUTH"] = m_Authentication;

	WriteFile(Path, Data);
}

bool CKadScript::LoadData(const wstring& Path)
{
	CVariant Data;
	if(!ReadFile(Path, Data))
		return false;

	try
	{
		m_LastUsed = Data["LastUsed"].To<uint64>();
		if(!m_LastUsed)
			m_LastUsed = GetTime();

		m_Data->Set(Data["Data"]);

		if(Data.Has("KEY"))
		{
			CVariant SecretKey = Data["KEY"];
			if(SecretKey.Decrypt(GetParent<CKademlia>()->Root()->GetID().GetPrivateKey()))
			{
				m_SecretKey = new CAbstractKey;
				if(!m_SecretKey->SetKey(SecretKey.GetData(), SecretKey.GetSize()))
					m_SecretKey = NULL;
			}
		}

		m_Authentication = Data.Get("AUTH");
	}
	catch(const CException&)
	{
		return false;
	}
	return true;
}

void CKadScript::SetSecretKey(CAbstractKey* pSecretKey)
{
	m_SecretKey = new CAbstractKey;
	m_SecretKey->SetKey(pSecretKey);
}

CVariant CKadScript::Execute(const CVariant& Requests, const CUInt128& TargetID, CReportLogger* pLogger)
{
	KeepAlive();

	m_pLogger = CPointer<CReportLogger>(pLogger, true); // this is a week pointer and it gets cleared when the caller exits

	CVariant Results(CVariant::EList);
	for(uint32 i=0; i < Requests.Count(); i++)
	{
		const CVariant& Request = Requests.At(i);
		Results.Append(Call(Request["FX"], Request["ARG"], TargetID, Request["XID"]));
	}
	return Results;
}

CVariant CKadScript::Execute(const map<CVariant, CKadOperator::SKadRequest>& Requests, const CUInt128& TargetID, CReportLogger* pLogger)
{
	KeepAlive();

	m_pLogger = CPointer<CReportLogger>(pLogger, true); // this is a week pointer and it gets cleared when the caller exits

	CVariant Results(CVariant::EList);
	for(map<CVariant, CKadOperator::SKadRequest>::const_iterator I = Requests.begin(); I != Requests.end(); I++)
	{
		CKadRequest* pRequest = I->second.pRequest;
		Results.Append(Call(pRequest->GetName(), pRequest->GetArguments(), TargetID, pRequest->GetXID()));
	}
	return Results;
}

CVariant CKadScript::Call(const string& Name, const CVariant& Arguments, const CUInt128& TargetID, const CVariant& XID)
{
	CVariant Result;
	Result["XID"] = XID;

	CDebugScope Debug(this);

	try
	{
		CJSScript* pScript = GetJSScript(true);

		vector<CPointer<CObject> > Parameters;
		Parameters.push_back(new CVariantPrx(Arguments));
		Parameters.push_back(new CKadIDObj(TargetID));
		CPointer<CObject> Return;
		pScript->Call(string("remoteAPI"), Name, Parameters, Return);
		if(CVariantPrx* pVariant = Return->Cast<CVariantPrx>())
			Result["RET"] = pVariant->GetCopy();
		else
			throw CJSException("InvalidResult", L"Call returned an invalid result");
	}
	catch(const CJSException& Exception)
	{
		LogReport(Exception.GetFlag(), Exception.GetLine(), Exception.GetError());

		Result["ERR"] = Exception.GetError();
	}

	return Result;
}

wstring CKadScript::GetVersion(uint32 Version)
{
	wstring sVersion;
	for(int i=sizeof(uint32)-1; i >= 0; i--)
	{
		byte Val = ((byte*)&Version)[sizeof(uint32)-(i+1)];
		if(!sVersion.empty() || Val != 0)
		{
			if(!sVersion.empty())
				sVersion.insert(0, L".");
			sVersion.insert(0, int2wstring(Val));
		}
	}
	return sVersion;
}

uint32 CKadScript::GetVersion(const wstring& sVersion)
{
	uint32 Version = 0;
	vector<wstring> Ver = SplitStr(sVersion,L".");
	for(size_t i = 0; i < Min(sizeof(uint32), Ver.size()); i++)
		((byte*)&Version)[sizeof(uint32)-(i+1)] = wstring2int(Ver[i]);
	return Version;
}
