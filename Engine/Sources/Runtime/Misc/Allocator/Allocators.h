#pragma once
#include "Yes.h"
#include "Misc/IAllocator.h"

namespace Yes
{
	class FrameAllocator : public IAllocator
	{
	public:
		FrameAllocator(size_t reservedSize)
		{

		}
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
			auto p = (AllocationHeader*)_aligned_malloc(sizeof(AllocationHeader) + sz);
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
	private:
		std::atomic<size_t> mUsed;
	};
}