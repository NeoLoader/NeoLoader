#pragma once

#define _std_fix_

template<class _Kty,
class _Ty,
class _Pr = less<_Kty>,
class _Alloc = allocator<pair<const _Kty, _Ty> > >
class _map_fix_: public map<_Kty, _Ty, _Pr, _Alloc>
{
public:
	typedef typename map<_Kty, _Ty, _Pr, _Alloc>::iterator iterator;

	iterator erase(iterator I)
	{
		map<_Kty, _Ty, _Pr, _Alloc>::erase(I++);
		return I;
	}
};

#define map _map_fix_

template<class _Kty,
class _Ty,
class _Pr = less<_Kty>,
class _Alloc = allocator<pair<const _Kty, _Ty> > >
class _multimap_fix_: public multimap<_Kty, _Ty, _Pr, _Alloc>
{
public:
	typedef typename multimap<_Kty, _Ty, _Pr, _Alloc>::iterator iterator;

	iterator erase(iterator I)
	{
		multimap<_Kty, _Ty, _Pr, _Alloc>::erase(I++);
		return I;
	}
};

#define multimap _multimap_fix_


template<class _Ty,
class _Pr = less<_Ty>,
class _Alloc = allocator<_Ty> >
class _set_fix_: public set<_Ty, _Pr, _Alloc>
{
public:
    typedef typename set<_Ty, _Pr, _Alloc>::iterator iterator;

    iterator erase(iterator I)
    {
        set<_Ty, _Pr, _Alloc>::erase(I++);
        return I;
    }
};

#define set _set_fix_
