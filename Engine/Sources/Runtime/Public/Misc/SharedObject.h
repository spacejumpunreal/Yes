#pragma once
#include "Yes.h"
#include "Debug.h"

#include <atomic>

namespace Yes
{
	class SharedObject
	{
	public:
		void Acquire()
		{
			mRefCount.fetch_add(1);
		}
		void Release()
		{
			auto old = mRefCount.fetch_sub(1);
			CheckDebug(old >= 1, "Release called on non positive referenced object, how strange");
			if (old == 1)
			{
				Destroy();
			}
		}
	protected:
		SharedObject()
			: mRefCount(0)
		{}
		virtual ~SharedObject()
		{}
		virtual void Destroy()
		{
			delete this;
		}
	private:
		std::atomic<int32> mRefCount;
	};
	
	//ref to subclass of SharedObject, difference between shared_ptr and TRef:
	//TRef can only reference those that are subclass of SharedObject
	//NOTE: TRef itself is not thread safe(copy and modify at the same time may cause problem)
	template<typename T>
	class TRef
	{
	public:
		TRef()
		{}
		TRef(const TRef& other)
			: mPtr(other.mPtr)
		{
			if (other.mPtr)
			{
				other.mPtr->Acquire();
			}
		}
		template<typename F>
		TRef(F* rawPtr)
		{
			if constexpr (std::is_convertible_v<F, T>)
			{
				mPtr = static_cast<T*>(rawPtr);
			}
			else
			{
				mPtr = dynamic_cast<T*>(rawPtr);
			}
			if (mPtr)
			{
				mPtr->Acquire();
			}
		}
		TRef(T* rawPtr)
			: mPtr(rawPtr)
		{
			if (mPtr)
			{
				mPtr->Acquire();
			}
		}
		TRef(TRef&& other)
		{
			std::swap(other.mPtr, mPtr);
		}
		~TRef()
		{
			if (mPtr)
			{
				mPtr->Release();
				mPtr = nullptr;
			}
		}
		template<typename C>
		TRef& operator=(C* rawPtr)
		{
			if (mPtr)
			{
				mPtr->Release();
			}
			mPtr = static_cast<T*>(rawPtr);
			if (mPtr)
			{
				mPtr->Acquire();
			}
			return *this;
		}
		TRef& operator=(const TRef& other)
		{
			if (mPtr)
			{
				mPtr->Release();
			}
			mPtr = other.mPtr;
			if (mPtr)
			{
				mPtr->Acquire();
			}
			return *this;
		}
		TRef& operator=(TRef&& other)
		{
			std::swap(other.mPtr, mPtr);
			return *this;
		}
		friend bool operator==(const TRef& lhs, const TRef& rhs)
		{
			return lhs.mPtr == rhs.mPtr;
		}
		friend bool operator!=(const TRef& lhs, const TRef& rhs)
		{
			return lhs.mPtr != rhs.mPtr;
		}
		T* operator->()
		{
			return mPtr;
		}
		T* GetPtr()
		{
			return mPtr;
		}

	private:
		T* mPtr = nullptr;
	};

	class ISharedBuffer : public SharedObject
	{
	public:
		virtual size_t GetSize() = 0;
		virtual void* GetData() = 0;
	};

	using SharedBufferRef = TRef<ISharedBuffer>;
}