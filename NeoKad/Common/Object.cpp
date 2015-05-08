#include "GlobalHeader.h"
#include "Object.h"

IMPLEMENT_BASE_OBJECT(CObject)

CObject::CObject(CObject* pParent)
{
	m_pParent = NULL;
	if(pParent)
		pParent->AddChild(this);
}

CObject::~CObject()
{
	for(list<CObject*>::iterator I = m_Children.begin();I != m_Children.end();)
	{
		CObject* pChild = *I++; // Note: this is tricky, the child when deleted somes back to remove itself form this m_Children 
								//		so we must at this point already obtained the next list position

		bool bReferenced = false;
		for(list<CPointerBase*>::iterator J = pChild->m_References.begin(); J != pChild->m_References.end(); J++)
		{
			if(!(*J)->IsWeak()) // Note: children with strong refrences are not deleted, thay are kept untill the last reference is released
				bReferenced = true;
		}
		if(!bReferenced) // do not delete referenced children
			delete pChild;
		else
			pChild->m_pParent = NULL;
	}

	if(m_pParent)
		m_pParent->RemoveChild(this);
}

void CObject::SetParent(CObject* pParent)
{
	if(m_pParent)
		m_pParent->RemoveChild(this);

	if(pParent)
		pParent->AddChild(this);
}

void CObject::AddChild(CObject* Child)
{
	ASSERT(Child->m_pParent == NULL);
	Child->m_pParent = this;

	m_Children.push_back(Child);
}

void CObject::RemoveChild(CObject* Child)
{
	ASSERT(Child->m_pParent == this);
	Child->m_pParent = NULL;

	for(list<CObject*>::iterator I = m_Children.begin(); I != m_Children.end(); I++)
	{
		if(*I == Child)
		{
			m_Children.erase(I);
			return;
		}
	}
	ASSERT(0);
}

void CObject::LogLine(uint32 uFlag, const wchar_t* sLine, ...)
{
	ASSERT(sLine != NULL);

	const size_t bufferSize = 10241;
	wchar_t bufferline[bufferSize];

	va_list argptr;
	va_start(argptr, sLine);
#ifndef WIN32
	if (vswprintf_l(bufferline, bufferSize, sLine, argptr) == -1)
#else
	if (vswprintf(bufferline, bufferSize, sLine, argptr) == -1)
#endif
		bufferline[bufferSize - 1] = L'\0';
	va_end(argptr);

	::LogLine(uFlag, L"%s", bufferline);
}

/*void CObject::LogLine(uint32 uFlag, const char* sLine, ...)
{
	ASSERT(sLine != NULL);

	const size_t bufferSize = 10241;
	char bufferline[bufferSize];

	va_list argptr;
	va_start(argptr, sLine);
	if (vsprintf(bufferline, sLine, argptr) == -1)
		bufferline[bufferSize - 1] = L'\0';
	va_end(argptr);

	::LogLine(uFlag, L"%S", bufferline);
}*/