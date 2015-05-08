#pragma once

template <class T>
class CScoped
{
public:
	CScoped(T* Val = NULL)			{m_Val = Val;}
	~CScoped()						{delete m_Val;}

	CScoped<T>& operator=(const CScoped<T>& Scoped)	{ASSERT(0); return *this;} // copying is explicitly forbidden
	CScoped<T>& operator=(T* Val)	{ASSERT(!m_Val); m_Val = Val; return *this;}

	inline T* Val() const			{return m_Val;}
	inline T* &Val()				{return m_Val;}
	inline T* Detache()				{T* Val = m_Val; m_Val = NULL; return Val;}
    inline T* operator->() const	{return m_Val;}
    inline T& operator*() const     {return *m_Val;}
    inline operator T*() const		{return m_Val;}

private:
	T*	m_Val;
};

template <class T>
class CIScoped // intrusive scoped pointer
{
public:
	CIScoped()						{m_Val = NULL;}
	CIScoped(T* Val)				{m_Val = NULL; Set(Val);}
	~CIScoped()						{Clr();}

	CIScoped<T>& operator=(const CIScoped<T>& Val)	{Set(Val.m_Val); return *this;}
	CIScoped<T>& operator=(T* Val)	{Set(Val); return *this;}

	inline T* Val() const			{return m_Val;}
	inline T* &Val()				{return m_Val;}
	inline T* Detache()				{T* Val = m_Val; m_Val = NULL; return Val;}
    inline T* operator->() const	{return m_Val;}
    inline T& operator*() const     {return *m_Val;}
    inline operator T*() const		{return m_Val;}

private:
	inline void Set(T* Val)			{Clr(); m_Val = Val; m_Val->Ref++;}
	inline void Clr()				{if(m_Val && (--m_Val->Ref) == 0) delete m_Val;}

	T*	m_Val;
};
