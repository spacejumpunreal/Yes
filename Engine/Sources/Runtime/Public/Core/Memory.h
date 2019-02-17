#if 0

#pragma once
#include "Yes.h"
#include "Core/System.h"

namespace Yes
{
	class BaseAllocator;
	void* operator new(size_t sz, BaseAllocator* al);
	void operator delete(void* ptr, size_t sz, BaseAllocator* al);
	template<typename T>
	void _InternalDelete(T* ptr, BaseAllocator* al);
}

#define GET_ALLOCATOR(Tag) GET_MODULE(MemoryModule)->GetAllocator(Tag)
#define YES_MALLOC(allocator, size) (allocator->Allocate(size))
#define YES_FREE(allocator, ptr) (allocator->Deallocate(ptr))
#define YES_NEW(allocator, type, ...) new(allocator) type(__VA_ARGS__)
#define YES_DELETE(allocator, ptr) { _InternalDelete(ptr, allocator); ptr = nullptr; }

namespace Yes
{
	template<typename T>
	void _InternalDelete(T* ptr, BaseAllocator* al)
	{
		ptr->~T();
		YES_FREE(al, ptr);
	}
}

#endif