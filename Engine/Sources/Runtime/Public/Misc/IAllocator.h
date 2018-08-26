#pragma once

#include "Yes.h"

namespace Yes
{
	class IAllocator
	{
		virtual void* Allocate(size_t sz) = 0;
		virtual void Deallocate(size_t sz, void* ptr) = 0;
		virtual size_t GetUsed() = 0;
		virtual size_t GetReserved() = 0;
		virtual ~IAllocator() {}
	};
}
