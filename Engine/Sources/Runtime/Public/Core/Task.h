#pragma once

#include <mutex>
#include <deque>
#include <functional>

class TaskQueue
{
public:
	template<typename Functor, typename... Args>
	void Put(Functor&& f, Args&&... args)
	{
		std::lock_guard<std::mutex> lk(mLock);
		mTasks.emplace_back(std::bind(std::forward<Functor>(f), std::forward<Args>(args)...));
	}
	void ExecuteAll()
	{
		ExecuteN(std::numeric_limits<size_t>::max());
	}
	void ExecuteN(size_t N)
	{
		decltype(mTasks) tasks;
		{
			std::lock_guard<std::mutex> lk(mLock);
			if (mTasks.size() <= N)
			{
				tasks.swap(mTasks);
			}
			else
			{
				for (int i = 0; i < N; i++)
				{
					tasks.emplace_back(mTasks.front());
					mTasks.pop_front();
				}
			}
		}
		while (tasks.size() > 0)
		{
			tasks.front()();
			tasks.pop_front();
		}
	}
protected:
	std::deque<std::function<void()>>		mTasks;
private:
	std::mutex								mLock;
};