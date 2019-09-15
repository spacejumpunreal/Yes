#if 0
#pragma once

#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/IAllocator.h"

namespace Yes
{
	class FrameAllocator : public IAllocator
	{
	public:
		FrameAllocator(size_t reservedSize)
		{

		}
		virtual ~FrameAllocator()
		{}
	protected:
		uint64* mBuffer;
	};

	struct AllocationHeader
	{
		size_t Size;
	};
	//all allocations are tracked and sent to malloc and free
	class LowLevelAllocator : public IAllocator
	{
	public:
		virtual void* Allocate(size_t sz)
		{
			mUsed += sz;
			auto p = (AllocationHeader*)_aligned_malloc(sizeof(AllocationHeader) + sz, );
			p->Size = sz;
			return p + 1;
		}
		virtual void Deallocate(void* ptr)
		{
			auto p = ((AllocationHeader*)ptr) - 1;
			size_t sz = p->Size;
			_aligned_free(p);
		}
		virtual size_t GetUsed()
		{
			return mUsed;
		}
		virtual size_t GetReserved()
		{
			return mUsed;
		}
		virtual ~LowLevelAllocator()
		{}
	private:
		std::atomic<size_t> mUsed;
	};
}
#endif