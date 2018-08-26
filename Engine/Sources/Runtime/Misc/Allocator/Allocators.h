#pragma once
#include "Yes.h"
#include "Misc/IAllocator.h"

namespace Yes
{
	class FrameAllocator : public IAllocator
	{

	};

	//all allocations are tracked and sent to malloc and free
	class LowLevelAllocator : public IAllocator
	{
	public:
		virtual void* Allocate(size_t sz)
		{
			mUsed += sz;
			return malloc(sz);
		}
		virtual void Deallocate(size_t sz, void* ptr)
		{
			mUsed -= sz;
			free(ptr);
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
		std::atomic<int32> mUsed;
	};
}