#pragma once
#include "Public/Concurrency/Concurrency.h"
#include "Public/Concurrency/Lock.h"
#include <atomic>
#include <list>

namespace Yes
{
	class ConcurrencyModule;
	class Fiber;
	using JobWaitingList = std::list<Fiber*>;

	struct JobData
	{
		ThreadFunctionPrototype Function;
		void* Context;
	};

	template<size_t BatchSize = 32>
	class JobDataBatch
	{
		static_assert(BatchSize > 0, "BatchSize should be larger than 0");
	public:
		JobDataBatch(ConcurrencyModule* module, uint32 dispatchPolicy = 0)
			: mCount(0)
			, mModule(module)
			, mPolicy(dispatchPolicy)
		{
		}
		void PutJobData(ThreadFunctionPrototype func, void* context)
		{
			mJobDatas[mCount].Function = func;
			mJobDatas[mCount].Context = context;
			++mCount;
			if (mCount == BatchSize)
			{
				Flush();
			}
		}
		void Flush()
		{
			mModule->AddJobs(mJobDatas, mCount);
			mCount = 0;
		}
	private:
		JobData mJobDatas[BatchSize];
		size_t mCount;
		ConcurrencyModule* mModule;
		uint32 mPolicy;
	};

	class JobUnitePoint
	{
	private:
		std::atomic<size_t> mCount;
		JobData mJobData;
	public:
		JobUnitePoint(size_t initialCount, ThreadFunctionPrototype function, void* arg);
		void Unite();
	};

	class JobLock : public SimpleSpinLock
	{
	};

	class JobConditionVariable
	{//a waiting list
	public:
		void Wait(JobLock* lock);
		void NotifyOne();
		void NotifyAll();
	private:
		JobWaitingList mWaitList;
	};



	class JobSemaphore
	{
	public:
		JobSemaphore(size_t count);
		void Increase();
		void Decrease();
		bool TryDecrease();
	private:
		std::atomic<size_t> mCount;
	};
}