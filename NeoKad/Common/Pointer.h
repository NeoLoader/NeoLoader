#pragma once

class CPointerBase;

class CPointerTarget
{
public:
	CPointerTarget();
	virtual ~CPointerTarget();

protected:
	friend class CPointerBase;
	void					AddReference(CPointerBase* Pointer);
	void					RemoveReference(CPointerBase* Pointer);
	bool					HasNoReference();
	int						ReferenceCount(bool bCountWeak = false);

	list<CPointerBase*>		m_References;
};

//////////////////////////////////////////////////////////////////////////
//

class CPointerBase
{
public:
	virtual ~CPointerBase()								{Clear();}

	bool IsWeak() const									{return m_Weak;}
	bool SetWeak(bool bWeak = true, bool bClear = true);
	bool IsLast() const									{return m_Obj ? m_Obj->ReferenceCount() <= 1 : false;}

	void Set(const CPointerBase& Pointer);
	inline CPointerTarget* Obj() const					{return m_Obj;}

protected:
	friend class CPointerTarget;
	void Set(const CPointerTarget* pObj);
	void Clear();

	CPointerTarget*	m_Obj;
	bool			m_Weak;
};

//////////////////////////////////////////////////////////////////////////
//

template <class T>
class CPointer: public CPointerBase
{
public:
	CPointer(T* pObj = NULL, bool Weak = false)			{m_Weak = Weak;		Set(pObj);}
	CPointer(const CPointer& Pointer)					{m_Obj = NULL;		Set(Pointer);}

	CPointer<T>& operator=(const CPointer<T>& Pointer)	{Set(Pointer); return *this;}
	CPointer& operator=(const CPointerBase& Pointer)	{Set(Pointer); return *this;}

	inline T* Obj() const								{return (T*)m_Obj;}
    inline T* operator->() const						{return (T*)m_Obj;}
    inline T& operator*() const							{return *((T*)m_Obj);}
    inline operator T*() const							{return (T*)m_Obj;}
};

//////////////////////////////////////////////////////////////////////////
//

template <class T>
class CPointerHolder: public CPointerTarget
{
public:
	CPointerHolder(T* pPtr = NULL)						{m_Ptr = pPtr;}
	virtual ~CPointerHolder()							{delete m_Ptr;}

	inline T* Ptr() const								{return m_Ptr;}

protected:
	T*				m_Ptr;
};

//////////////////////////////////////////////////////////////////////////
//

template <class T, class H = CPointerHolder<T> >
class CHolder: public CPointerBase
{
public:
	CHolder(T* pPtr = NULL, bool Weak = false)			{m_Weak = Weak;		Set(new H(pPtr));}
	CHolder(const CHolder& Holder)						{m_Obj = NULL;		Set(Holder);}

	CHolder<T>& operator=(const CHolder<T>& Holder)		{Set(Holder); return *this;}
	CHolder& operator=(const CPointerBase& Holder)		{Set(Holder); return *this;}

	inline T* Obj() const								{return ((H*)m_Obj)->Ptr();}
    inline T* operator->() const						{return ((H*)m_Obj)->Ptr();}
    inline T& operator*() const							{return *((H*)m_Obj)->Ptr();}
    inline operator T*() const							{return ((H*)m_Obj)->Ptr();}
};
