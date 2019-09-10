#pragma once
#include "Runtime/Public/Yes.h"

namespace Yes
{
	template<typename RangePtr>
	RangePtr GetAlignedPtr(RangePtr start, size_t align)
	{
		size_t alignMask = align - 1;
		size_t invAlignMask = ~alignMask;
		size_t startInt = (size_t)start;
		RangePtr ret = (RangePtr)((startInt + alignMask) & invAlignMask);
		return ret;
	}
	inline size_t AlignSize(size_t size, size_t align)
	{
		size_t alignMask = align - 1;
		return (size + alignMask) & ~alignMask;
	}
}