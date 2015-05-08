#include "GlobalHeader.h"
#include "STLAllocator.h"
#include "MicroAllocator.h"
using namespace MICRO_ALLOCATOR;

HeapManager* g_stdm;
struct SSTDAllocator
{
	SSTDAllocator() { g_stdm = createHeapManager(); }
	~SSTDAllocator() { releaseHeapManager(g_stdm);}
} g_STDAllocator;

void* std_malloc(size_t size)
{
	return heap_malloc(g_stdm, size);
}

void std_free(void* ptr)
{
	heap_free(g_stdm, ptr);
}
