#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/Debug.h"
#include <type_traits>

namespace Yes
{
	template<typename T>
	class ObjectPool
	{
	public:
		template<typename... Args>
		T* Allocate(Args&&... args)
		{
			if (mFreeList == nullptr)
			{
				T* head = (T*)malloc(sizeof(T) * mGrowStep);
				for (int i = 0; i < mGrowStep - 1; ++i)
				{
					T** pptr = (T**)&*(head + i);
					*pptr = head + i + 1;
				}
				*(T**)(head + mGrowStep - 1) = nullptr;
				mFreeList = head;
				mTotalCount += mGrowStep;
			}
			auto p = mFreeList;
			mFreeList = *((T**)p);
			CheckAlways(mFreeList != nullptr);
			new (p) T(std::forward<Args>(args)...);
			++mUsedCount;
			return p;
		}
		void Deallocate(T* p)
		{
			p->~T();
			*(T**)p = mFreeList;
			mFreeList = p;
			CheckAlways(mFreeList != nullptr);
			--mUsedCount;
		}
		void GetStatistics(size_t& usedCount, size_t& totalCount)
		{
			mUsedCount = usedCount;
			mTotalCount = totalCount;
		}
		ObjectPool(size_t growStep)
			: mFreeList(nullptr)
			, mUsedCount(0)
			, mTotalCount(0)
			, mGrowStep(growStep)
		{
		}
		static_assert(sizeof(T) > sizeof(T*), "size of T must be able to contain a pointer");
	private:
		T* mFreeList;
		size_t mUsedCount;
		size_t mTotalCount;
		size_t mGrowStep;
	};
}