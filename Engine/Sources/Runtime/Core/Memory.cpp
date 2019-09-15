#if 0

#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/Memory.h"

namespace Yes
{
	void * operator new(size_t sz, BaseAllocator * al)
	{
		return nullptr;
	}
	void * operator new[](size_t, BaseAllocator * al)
	{
		return nullptr;
	}
		void operator delete(void * ptr, size_t sz, BaseAllocator * al)
	{
	}
	void operator delete[](void * ptr, size_t sz, BaseAllocator * al)
	{
	}
}

#endif