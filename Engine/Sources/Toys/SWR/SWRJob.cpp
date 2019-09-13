#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Concurrency/Sync.h"
#include "Toys/SWR/SWRJob.h"

#include <thread>
#include <deque>

namespace Yes::SWR
{
	struct SWRJobSystemImp;
	void _WorkerThread(size_t idx, SWRJobSystemImp* jobSystem);

	struct SWRJobItem
	{
	public:
		std::function<void()> Function;
		bool IsSync;
	public:
		SWRJobItem(std::function<void()>&& f)
			: Function(f)
			, IsSync(false)
		{}

		SWRJobItem()
			: IsSync(true)
		{}
	};

	struct SWRJobSystemImp : public SWRJobSystem
	{
	public:
		SWRJobSystemImp(const DeviceDesc* desc)
			: mThreads(desc->NumThreads)
			, mWaitCount(0)
		{
			for (size_t i = 0; i < desc->NumThreads; ++i)
			{
				mThreads[i] = std::thread(_WorkerThread, i, this);
			}
		}
		void _PutBack(std::function<void()>&& f) override
		{
			{
				std::lock_guard<std::mutex> lk(mLock);
				mTasks.emplace_back(std::move(f));
			}
			mCondition.notify_one();
		}
		void _PutFront(std::function<void()>&& f) override
		{
			{
				std::lock_guard<std::mutex> lk(mLock);
				mTasks.emplace_front(std::move(f));
			}
			mCondition.notify_one();
		}
		void PutSyncPoint()
		{
			{
				std::lock_guard<std::mutex> lk(mLock);
				mTasks.emplace_back();
			}
			mCondition.notify_one();
		}
		void Execute()
		{
			while (true) //TODO should check a quit flag
			{
				std::unique_lock<std::mutex> lk(mLock);
				if (mTasks.size() == 0)
				{
					++mWaitCount;
					mCondition.wait(lk);
					--mWaitCount;
				}
				if (mTasks.size() > 0)
				{
					if (mTasks[0].IsSync)
					{
						if (mWaitCount + 1 == mThreads.size())
						{//all synced
							mTasks.pop_front();
							lk.unlock();
							mCondition.notify_all();
						}
						else
						{// can't just continue the loop, it won't stop there
							++mWaitCount;
							mCondition.wait(lk);
							--mWaitCount;
						}
					}
					else
					{//there is a real job
						auto f = std::move(mTasks.front().Function);
						mTasks.pop_front();
						lk.unlock();
						f();
					}
				}
			}
		}
		void Test(int)
		{
		}
	protected:
		std::mutex								mLock;
		std::vector<std::thread>				mThreads;
		std::deque<SWRJobItem>					mTasks;
		std::condition_variable					mCondition;
		int32									mWaitCount;
		friend void _WorkerThread(size_t, SWRJobSystemImp* jobSystem)
		{
			jobSystem->Execute();
		}
	};

	SWRJobSystem* CreateSWRJobSystem(const DeviceDesc* desc)
	{
		return new SWRJobSystemImp(desc);
	}
}
