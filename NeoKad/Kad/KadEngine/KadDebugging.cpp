#include "GlobalHeader.h"
#include "../KadHeader.h"
#include "KadDebugging.h"
#include "KadScript.h"
#include "KadOperator.h"
#include "../../Common/MT/Mutex.h"

list<CDebugScope::Type> g_DebugContext;
CMutex g_DebugMutex;

CDebugScope::CDebugScope(const Type& Scope)
{
	g_DebugMutex.Lock();
	g_DebugContext.push_back(Scope);
	g_DebugMutex.Unlock();
}

CDebugScope::CDebugScope(CKadScript* pScript, CObject* pObject)
{
	ASSERT(pObject == NULL || pObject->Inherits("CKadOperation") || pObject->Inherits("CRouteSession") || pObject->Inherits("CKadRoute"));

	g_DebugMutex.Lock();
	g_DebugContext.push_back(make_pair(pScript, pObject));
	g_DebugMutex.Unlock();
}

CDebugScope::~CDebugScope()
{
	g_DebugMutex.Lock();
	g_DebugContext.pop_back();
	g_DebugMutex.Unlock();
}

bool CDebugScope::IsEmpty()	
{
	g_DebugMutex.Lock();
	bool ret = g_DebugContext.empty();
	g_DebugMutex.Unlock();
	return ret;
}

CDebugScope::Type CDebugScope::Scope()
{
	g_DebugMutex.Lock();
	Type ret;
	if(!g_DebugContext.empty())
		ret = g_DebugContext.back();
	g_DebugMutex.Unlock();
	return ret;
}