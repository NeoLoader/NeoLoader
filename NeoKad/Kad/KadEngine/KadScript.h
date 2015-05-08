#pragma once

#include "../../Common/v8Engine/JSScript.h"
class CDebugObj;
class CKadRouteObj;
class CDataStoreObj;
class CAbstractKey;
class CKadOperation;
class CJSKadLookup;
class CKadRequest;
class CComChannel;
#include "../../../Framework/Cryptography/AsymmetricKey.h"
#include "../KadOperation.h"
#include "../KadEngine/KadOperator.h"

class CReportLogger: public CObject
{
public:
	virtual void			LogReport(UINT Flags, const wstring& ErrorMessage, const string& Error = "") = 0;
};

class CKadScript: public CObject
{
public:
	DECLARE_OBJECT(CKadScript);

	CKadScript(const CVariant& CodeID, CObject* pParent = NULL);
	virtual ~CKadScript();

	virtual bool			Process(UINT Tick);

	virtual const CVariant&	GetCodeID() const						{return m_CodeID;}
	virtual wstring			GetName() const							{return m_Name;}
	virtual uint32			GetVersion() const						{return m_Version;}
	virtual const string&	GetSource() const						{return m_Source;}
	virtual bool			IsValid() const;

	virtual void			SetSource(const string& Source, const wstring& Name, uint32 Version);
	virtual int				Setup(const string& Source, bool bAuthenticated);

	virtual void			SetAuthentication(const CVariant& Authentication);
	virtual const CVariant&	GetAuthentication()						{return m_Authentication;}
	virtual bool			IsAuthenticated()						{return m_Authentication.IsValid();}

	virtual void			SetSecretKey(CAbstractKey* pSecretKey);
	virtual CAbstractKey*	GetSecretKey()							{return m_SecretKey;}

	virtual void			SaveData();
	virtual void			SaveData(const wstring& Path);
	virtual bool			LoadData(const wstring& Path);

	virtual CKadRouteObj*	SetupRoute(CAbstractKey* pKey = NULL);
	virtual CKadRouteObj*	GetRoute() {return m_pRoute;}

	virtual void			KeepAlive();

	virtual void			Terminate();

	virtual CJSScript*		GetJSScript(bool bInstantiate = false);
	virtual CDataStoreObj*	GetData()								{return m_Data;}

	virtual CVariant		Execute(const CVariant& Requests, const CUInt128& TargetID, CReportLogger* pLogger);
	virtual CVariant		Execute(const map<CVariant, CKadOperator::SKadRequest>& Requests, const CUInt128& TargetID, CReportLogger* pLogger);

	static string			WriteHeader(const map<string, string>& HeaderFields);
	static map<string, string> ReadHeader(const string& Source);

	static wstring			GetVersion(uint32 Version);
	static uint32			GetVersion(const wstring& sVersion);

	virtual void			LogReport(UINT Flags, const wstring& ErrorMessage, const string& Error = "");

protected:
	virtual CVariant		Call(const string& Name, const CVariant& Arguments, const CUInt128& TargetID, const CVariant& XID);

	CPointer<CJSScript>		MakeScript(const string& Source, const wstring& Name, uint32 Version);

	static void				LineLogger(UINT Flags, const wstring& Line);

	wstring					m_Name;
	uint32					m_Version;
	string					m_Source;
	CVariant				m_SourceHash;
	CPointer<CJSScript>		m_pScript;
	CVariant				m_CodeID;
	CPointer<CKadRouteObj>	m_pRoute;
	time_t					m_LastUsed;
	CPointer<CDataStoreObj> m_Data;
	CVariant				m_Authentication;
	CHolder<CAbstractKey>	m_SecretKey;

	CPointer<CReportLogger>	m_pLogger;
};
