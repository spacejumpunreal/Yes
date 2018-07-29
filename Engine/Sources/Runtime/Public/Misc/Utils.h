#pragma once
#include "Yes.h"

#define ARRAY_COUNT(a) (sizeof(a) / sizeof(a[0]))

namespace Yes
{
	template<typename T>
	inline void ZeroFill(T& o)
	{
		char* start = (char*)&o;
		char* end = start + sizeof(T);
		std::fill(start, end, 0);
	}
	inline void FillN(void* dst, size_t n, void* pattern, size_t patternSize)
	{
		auto bdst = (uint8*)dst;
		for (int i = 0; i < n; ++i)
		{
			memcpy(bdst, pattern, patternSize);
			bdst += patternSize;
		}
	}

	inline void Copy(void* from, void* to, size_t bytes)
	{
		std::memcpy(to, from, bytes);
	}

	template<typename T>
	inline void ReleaseCOM(T*& i)
	{
		if (i != nullptr)
		{
			i->Release();
			i = nullptr;
		}
	}

	template<typename T>
	class COMRef
	{
	public:
		COMRef()
			: mPtr(nullptr)
		{}
		COMRef(T* t)
			: mPtr(t)
		{}
		T* operator->()
		{
			return mPtr;
		}
		void operator=(T* t)
		{
			_ReleaseCurrent();
			mPtr = t;
		}
		T** operator&()
		{
			return &mPtr;
		}
		T* GetPtr()
		{
			return mPtr;
		}
	protected:
		void _ReleaseCurrent()
		{
			if (mPtr)
			{
				mPtr->Release();
				mPtr = nullptr;
			}
		}
		T* mPtr;
	};
}
