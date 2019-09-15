#pragma once
#include "Runtime/Public/Yes.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>

#if YES_WINDOWS
#include <Windows.h>
#endif

namespace Yes
{
	template<typename T>
	inline void Check(const T& v, const char* fmt, ...)
	{
		if (!v)
		{
			va_list args;
			va_start(args, fmt);
			vprintf(fmt, args);
			va_end(args);
			fflush(0);
			assert(0);
		}

	}
	template<typename T>
	inline void Check(const T& v, ...)
	{
		if (!v)
		{
			assert(0);
		}
	}
}

#define CheckAlways(cond, ...) Check(cond, __VA_ARGS__)
#define CheckSucceeded(cond, ...) Check(SUCCEEDED(cond), __VA_ARGS__)
#ifdef NDEBUG
#define CheckDebug(...) 
#else
#define CheckDebug(cond, ...) Check(cond, __VA_ARGS__)
#endif
