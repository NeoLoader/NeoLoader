#pragma once

#include "../../Common/v8Engine/JSScript.h"
class CKadScript;
class CBinaryCache;

typedef map<CVariant, CPointer<CKadScript> > ScriptMap;

class CKadEngine: public CObject
{
public:
	DECLARE_OBJECT(CKadEngine);

	CKadEngine(CObject* pParent = NULL);
	virtual	~CKadEngine();

	void				Process(UINT Tick);
	void				HardReset();

	string				Install(const CVariant& InstallReq);
	bool				Install(const CVariant& CodeID, const string& Source, const CVariant& Authentication = CVariant());
	void				Remove(const CVariant& CodeID);
	CKadScript*			GetScript(const CVariant& CodeID);
	const ScriptMap&	GetScripts()	{return m_Scripts;}

	CBinaryCache*		GetCache()		{return m_BinaryCache;}

	size_t				GetTotalMemory();

protected:
	bool				Authenticate(const CVariant& CodeID, const CVariant& Authentication, const string& Source);

	static	bool		m_b8InitDone;

	void				SaveData();

	ScriptMap			m_Scripts;

	CPointer<CBinaryCache> m_BinaryCache;
	uint64				m_uLastSave;
};

