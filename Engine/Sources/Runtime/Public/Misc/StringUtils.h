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
}