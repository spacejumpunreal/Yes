#pragma once
#include "Runtime/Public/Yes.h"

#include <condition_variable>
#include <deque>
#include <mutex>

namespace Yes
{
/**
	a task queue:
	you can put task in from front or back
*/
	template<typename T, typename LockType = std::mutex>
	class MultiThreadQueue
	{
	public:
		template<typename... Args>
		void PushBack(Args&&... args)
		{
			std::unique_lock<LockType> lk(mLock);
			mData.emplace_back(std::forward<Args>(args)...);
			mCondition.notify_one();
		}
		template<typename Container>
		void Pop(size_t popLimit, Container& container)
		{
			std::unique_lock<LockType> lk(mLock);
			mCondition.wait(lk, [this] { return mData.size() > 0; });
			while (popLimit > 0 && mData.size() > 0)
			{
				container.push_back(std::move(mData.front()));
				mData.pop_front();
				--popLimit;
			}
		}
	protected:
		std::deque<T>				mData;
		LockType					mLock;
		std::condition_variable		mCondition;
	};
}