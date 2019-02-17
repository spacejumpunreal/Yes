#pragma once
#include "Yes.h"
#include "Debug.h"

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
		for (size_t i = 0; i < n; ++i)
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
		{
			if (mPtr)
			{
				mPtr->AddRef();
			}
		}
		COMRef(COMRef& comRef)
			: mPtr(comRef.GetPtr())
		{
			if (mPtr)
			{
				mPtr->AddRef();
			}
		}
		~COMRef()
		{
			_ReleaseCurrent();
		}
		T* operator->()
		{
			return mPtr;
		}
		COMRef<T>& operator=(T* t)
		{
			_ReleaseCurrent();
			mPtr = t;
			return *this;
		}
		COMRef<T>& operator=(COMRef<T>& other)
		{
			_ReleaseCurrent();
			mPtr = other.mPtr;
			return *this;
		}
		COMRef<T>& operator=(COMRef<T>&& other)
		{
			_ReleaseCurrent();
			mPtr = other.mPtr;
			other.mPtr = nullptr;
			return *this;
		}
		T** operator&()
		{
			CheckAlways(mPtr == nullptr);
			return &mPtr;
		}
		T* GetPtr()
		{
			return mPtr;
		}
		T* Detach()
		{
			T* tmp = mPtr;
			mPtr = nullptr;
			return tmp;
		}
		template<typename U>
		HRESULT As(COMRef<U>& other)
		{
			return mPtr->QueryInterface(__uuidof(U), (void**)&other);
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
