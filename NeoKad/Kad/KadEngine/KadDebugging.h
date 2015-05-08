#pragma once

class CKadScript;
class CKadOperator;

class CDebugScope
{
public:
	typedef pair<void*, void*> Type;

	CDebugScope(const Type& Scope);
	CDebugScope(CKadScript* pScript, CObject* pObject = NULL);
	~CDebugScope();

	static bool			IsEmpty();
	static Type 		Scope();

protected:
	static list<Type>	m_DebugContext;
};