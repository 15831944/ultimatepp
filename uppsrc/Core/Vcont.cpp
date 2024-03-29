#include "Core.h"

NAMESPACE_UPP

struct Vector_ {
	void *vector;
	int   items;
	int   alloc;
};

#ifdef _DEBUG

void *break_when_picked;

void BreakWhenPicked(void *ptr)
{
	if(ptr == break_when_picked)
		__BREAK__;
}

void BREAK_WHEN_PICKED__(void *ptr)
{
	break_when_picked = ptr;
}

#endif

void VectorReAlloc_(void *v_, int newalloc, int sizeofT)
{
	Vector_ *v = (Vector_*)v_;
	ASSERT(newalloc >= v->items);
	ASSERT(v->items >= 0); // Pick semantics broken
	size_t sz0 = newalloc * sizeofT;
	size_t sz = sz0;
	void *newvector = newalloc ? MemoryAllocSz(sz) : NULL;
//	v->alloc = /*(sz - sz0) / sizeofT + */newalloc; //Benchmark...
	v->alloc = (int)((sz - sz0) / sizeofT + newalloc); //Benchmark...
	if(v->vector)
		memcpy(newvector, v->vector, v->items * sizeofT);
	v->vector = newvector;
}

void VectorReAllocF_(void *v_, int newalloc, int sizeofT)
{
	Vector_ *v = (Vector_*)v_;
	void *prev = v->vector;
	VectorReAlloc_(v, newalloc, sizeofT);
	MemoryFree(prev);
}

void VectorGrow_(void *v_, int sizeofT)
{
	Vector_ *v = (Vector_*)v_;
#ifdef _DEBUG
	VectorReAlloc_(v, max(v->alloc + 1, 2 * v->alloc), sizeofT);
#else
	VectorReAlloc_(v, max(v->alloc + 1, int(v->alloc + ((unsigned)v->alloc >> 1))), sizeofT);
//	VectorReAlloc_(v, max(v->alloc + 1, max(2 * v->alloc, 16 / sizeofT)), sizeofT);
//	VectorReAlloc_(v, max(v->alloc + 1, 2 * v->alloc), sizeofT);
#endif
}

void VectorGrowF_(void *v_, int sizeofT)
{
	Vector_ *v = (Vector_*)v_;
	void *prev = v->vector;
	VectorGrow_(v, sizeofT);
	MemoryFree(prev);
}


int64 NewInVectorSerial()
{
	static int64 x;
	INTERLOCKED {
		++x;
	}
	return x;
}

thread__ int64 invector_cache_serial_;
thread__ int   invector_cache_blki_;
thread__ int   invector_cache_offset_;
thread__ int   invector_cache_end_;

END_UPP_NAMESPACE
