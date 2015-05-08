#include "GlobalHeader.h"
#include "Pointer.h"

CPointerTarget::CPointerTarget() 
{
}

CPointerTarget::~CPointerTarget()
{
	for(list<CPointerBase*>::iterator I = m_References.begin(); I != m_References.end(); I++)
	{
		ASSERT((*I)->IsWeak()); // Remember: if this happens it means thos object was deleted while still beind referenced by others (for example a parent object deleted its children)
		(*I)->Set(NULL);
	}
}

void CPointerTarget::AddReference(CPointerBase* Pointer)
{
	m_References.push_back(Pointer);
}

void CPointerTarget::RemoveReference(CPointerBase* Pointer)
{
	for(list<CPointerBase*>::iterator I = m_References.begin(); I != m_References.end(); I++)
	{
		if(*I == Pointer)
		{
			m_References.erase(I);
			return;
		}
	}
	ASSERT(0);
}

bool CPointerTarget::HasNoReference()
{
	for(list<CPointerBase*>::iterator I = m_References.begin(); I != m_References.end(); I++)
	{
		if(!(*I)->IsWeak())
			return false;
	}
	return true;
}

int CPointerTarget::ReferenceCount(bool bCountWeak)
{
	int Count = 0;
	for(list<CPointerBase*>::iterator I = m_References.begin(); I != m_References.end(); I++)
	{
		if(!bCountWeak && (*I)->IsWeak())
			continue;
		Count++;
	}
	return Count;
}

//////////////////////////////////////////////////////////////////////////
//

bool CPointerBase::SetWeak(bool bWeak, bool bClear)
{	
	if(m_Weak != bWeak)
	{
		m_Weak = bWeak;
		if(bClear && m_Obj->HasNoReference()) 
			delete m_Obj; 
	}
	return m_Obj != NULL; 
}

void CPointerBase::Set(const CPointerBase& Pointer)
{
	Clear(); 
	m_Weak = Pointer.IsWeak();
	Set(Pointer.Obj()); 
}

void CPointerBase::Set(const CPointerTarget* pObj)
{
	m_Obj = (CPointerTarget*)pObj; // ToDo-Now: use mutable instead
	if(m_Obj) 
		m_Obj->AddReference(this);
}

void CPointerBase::Clear()									
{	
	if(m_Obj)
	{
		m_Obj->RemoveReference(this); 
		if(!m_Weak && m_Obj->HasNoReference()) 
			delete m_Obj;
	}
}
