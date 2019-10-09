#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/StringUtils.h"
#include <cstring>

namespace Yes
{
	static const size_t NameHashPrime = 31;
	struct Name
	{
	public:
		template<size_t N>
		constexpr Name(const char(&str)[N])
			: mHash(HornerHashCompileTime(str))
			, mSize(N - 1)
#if YES_DEBUG
			, mString(str)
#endif
		{}
		Name(const char* str = "")
			: mHash(HornerHashRunTime(str, NameHashPrime))
			, mSize(strlen(str))
#if YES_DEBUG
			, mString(str)
#endif
		{}
		size_t GetHash() const { return mHash; }
		size_t GetSize() const { return mSize; }
		friend bool operator==(const Name& l, const Name& r)
		{
			return l.mSize == r.mSize && l.mHash == r.mHash;
		}
	private:
		size_t			mHash;
		size_t			mSize;
#if YES_DEBUG
	public:
		const char* mString;
#endif
	};
}

namespace std
{
	template<>
	struct hash<Yes::Name>
	{
		size_t operator()(const Yes::Name& n) const
		{
			return n.GetHash();
		}
	};
}
