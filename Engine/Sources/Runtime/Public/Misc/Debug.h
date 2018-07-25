#pragma once

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

