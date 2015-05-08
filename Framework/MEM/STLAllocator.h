#pragma once

#include<memory>
#include<cstdlib>

void* std_malloc(size_t size);
void std_free(void* ptr);

template<typename T>
class CSTDAllocator{
public :
    //    typedefs

    typedef T value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

public :
    //    convert an allocator<T> to allocator<U>

    template<typename U>
    struct rebind {
        typedef CSTDAllocator<U> other;
    };

public :
    inline explicit CSTDAllocator() {}
    inline ~CSTDAllocator() {}
    inline explicit CSTDAllocator(CSTDAllocator const&) {}
    template<typename U>
    inline explicit CSTDAllocator(CSTDAllocator<U> const&) {}

    //    address

    inline pointer address(reference r) { return &r; }
    inline const_pointer address(const_reference r) { return &r; }

    //    memory allocation

    inline pointer allocate(size_type cnt, typename std::allocator<void>::const_pointer = 0) {
	  return (T*) std_malloc(cnt * sizeof (T));
    }
    inline void deallocate(pointer p, size_type n) {
        return std_free(p);
    }
    //    size
    inline size_type max_size() const {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    //    construction/destruction

    inline void construct(pointer p, const T& t) {
      new(p) T(t);
    }
    inline void destroy(pointer p) {
      p->~T();
    }

    inline bool operator==(CSTDAllocator const&) { return true; }
    inline bool operator!=(CSTDAllocator const& a) { return !operator==(a); }
};