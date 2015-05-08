#pragma once
#include "Pointer.h"

#ifndef STR
#define STR2(X) #X
#define STR(X) STR2(X)
#endif

#define DECLARE_OBJECT(name)															\
	static char*	StaticName()  {return STR(name);}									\
	virtual char*	ClassName()  {return StaticName();}									\
	virtual bool	Inherits(char* Name) const;											\
	virtual list<string> ClassNames();													\

#define IMPLEMENT_OBJECT(name, base)													\
	bool name::Inherits(char* Name)	const {return strcmp(Name,StaticName()) == 0		\
											|| base::Inherits(Name);}					\
	list<string> name::ClassNames() 													\
	{																					\
		list<string> Names = base::ClassNames();										\
		Names.push_front(StaticName());													\
		return Names;																	\
	}																					\

#define IMPLEMENT_BASE_OBJECT(name)														\
	bool name::Inherits(char* Name)	const{return strcmp(Name,StaticName()) == 0;}		\
	list<string> name::ClassNames() 													\
	{																					\
		list<string> Names;																\
		Names.push_front(StaticName());													\
		return Names;																	\
	}																					\

class CObject: public CPointerTarget
{
public:
	DECLARE_OBJECT(CObject);

	CObject(CObject* pParent = NULL);
	virtual ~CObject();

	virtual void			SetParent(CObject* pParent);
	CObject*				GetParent() const			{return m_pParent;}
	template <class T>
	T*						GetParent() const
	{
		for(CObject* pParent = GetParent();pParent;pParent = pParent->GetParent())
		{
			if(pParent->Inherits(T::StaticName()))
				return (T*)pParent;
		}
		return NULL;
	}
	const list<CObject*>&	GetChildren() const	{return m_Children;}
	template <class T>
	T*						GetChild() const
	{
		for(list<CObject*>::const_iterator I = m_Children.begin(); I != m_Children.end(); I++)
		{
			if((*I)->Inherits(T::StaticName()))
				return (T*)(*I);
		}
		return NULL;
	}
	template <class T>
	T*						Cast()
	{
		if(this && Inherits(T::StaticName()))
			return (T*)this;
		return NULL;
	}

	virtual	void			LogLine(uint32 uFlag, const wchar_t* sLine, ...);
	//virtual	void			LogLine(uint32 uFlag, const char* sLine, ...);

protected:
	virtual void			AddChild(CObject* Child);
	virtual void			RemoveChild(CObject* Child);

	CObject*				m_pParent;
	list<CObject*>			m_Children;
};
