#pragma once
#include "Yes.h"

#include <mutex>
#include <deque>
#include <condition_variable>

namespace Yes
{
/**
	a task queue:
	you can put task in from front or back
*/
	template<typename T>
	class MultiThreadQueue
	{
	public:
		template<typename Iterator>
		void PushMove(bool atBack, Iterator begin, Iterator end)
		{
			{
				std::lock_guard<std::mutex> lk(mLock);
				if (atBack)
				{
					for (auto i = begin; i != end; ++i)
					{
						mData.emplace_back(std::move(*i));
					}
				}
				else
				{
					for (auto i = begin; i != end; ++i)
					{
						mData.emplace_front(std::move(*i));
					}
				}
			}
			mCondition.notify_all();
		}
		template<typename... Args>
		void PushBack(Args&&... args)
		{
			mData.emplace_back(std::forward<Args>(args)...);
			mCondition.notify_one();
		}
		template<typename Container>
		void Pop(size_t popLimit, Container& container)
		{
			std::unique_lock<std::mutex> lk(mLock);
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
		std::mutex					mLock;
		std::condition_variable		mCondition;
	};
}