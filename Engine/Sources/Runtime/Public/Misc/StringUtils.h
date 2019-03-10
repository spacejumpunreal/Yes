#pragma once

#include "Yes.h"

namespace Yes
{
	inline std::string FormatString(const char* fmt, ...)
	{
		const int max = 4096;
		std::string ret;
		ret.resize(max);
		va_list args;
		va_start(args, fmt);
		int bytes = vsnprintf((char*)ret.data(), max, fmt, args);
		if (bytes < 0)
			*((int*)nullptr) = 0;
		va_end(args);
		ret.resize(bytes);
		return ret;
	}
	template<size_t N>
	constexpr size_t HornerHashCompileTime(const char(&str)[N], size_t prime = 31, size_t len = N - 1)
	{
		return (len <= 1) ? str[0] : (prime * HornerHashCompileTime(str, prime, len - 1) + (size_t)str[len - 1]);
	}
	inline size_t HornerHashRunTime(const char* str, size_t prime)
	{
		size_t acc = 0;
		while (*str != 0)
		{
			acc *= prime;
			acc += *str;
			++str;
		}
		return acc;
	}
}