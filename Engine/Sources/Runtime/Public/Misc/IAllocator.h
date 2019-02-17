#pragma once

#include "Yes.h"

namespace Yes
{
	class IAllocator
	{
	public:
		virtual void* Allocate(size_t sz, size_t align) = 0;
		virtual void Deallocate(size_t sz, void* ptr) = 0;
		virtual size_t GetUsed() = 0;
		virtual size_t GetReserved() = 0;
		virtual ~IAllocator() {}
	};
}
