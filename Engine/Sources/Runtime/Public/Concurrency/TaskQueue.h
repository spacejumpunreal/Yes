#pragma once

#include <mutex>
#include <deque>
#include <functional>
#include <condition_variable>

using TaskFunction = void(*)();
struct TaskItem
{
	TaskFunction Entry;
	void* Context;
};

class TaskQueue2
{
public:
	void Put(bool atBack, std::function<void()>&& f)
	{
		{
			std::lock_guard<std::mutex> lk(mLock);
			if (atBack)
				mTasks.emplace_back(std::move(f));
			else
				mTasks.emplace_front(std::move(f));
		}
		mCondition.notify_one();
	}
	template<typename Iterator>
	void Put(bool atBack, Iterator begin, Iterator end)
	{
		{
			std::lock_guard<std::mutex> lk(mLock);
			if (atBack)
			{
				for (auto i = begin; i != end; ++i)
				{
					mTasks.emplace_back(std::move(*i));
				}
			}
			else
			{
				for (auto i = begin; i != end; ++i)
				{
					mTasks.emplace_front(std::move(*i));
				}
			}
		}
		mCondition.notify_all();
	}
	void Execute()
	{
		while (true)
		{
			std::unique_lock<std::mutex> lk(mLock);
			mCondition.wait(lk, [this] { return mTasks.size() > 0; });
			auto task = std::move(mTasks.front());
			mTasks.pop_front();
			lk.unlock();
			task();
		}
	}
protected:
	std::deque<std::function<void()>>		mTasks;
	std::mutex								mLock;
	std::condition_variable					mCondition;
};