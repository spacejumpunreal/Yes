#pragma once
#include "Runtime/Public/Concurrency/Concurrency.h"
#include "Runtime/Public/Concurrency/Lock.h"
#include "Runtime/Public/Core/System.h"
#include <atomic>
#include <list>

namespace Yes
{
	class ConcurrencyModule;
	class Fiber;

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
		JobDataBatch(ConcurrencyModule* module)
			: mCount(0)
			, mModule(module)
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
			if (mCount > 0)
			{
				mModule->AddJobs(mJobDatas, mCount);
				mCount = 0;
			}
		}
		~JobDataBatch()
		{
			Flush();
		}
	private:
		JobData mJobDatas[BatchSize];
		size_t mCount;
		ConcurrencyModule* mModule;
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

	class JobWaitingList
	{
	public:
		void Append(Fiber* fiber);
		void Pop(size_t n);
	private:
		JobLock ListLock;
		std::list<Fiber*> Entries;
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
	private:
		size_t mCount;
		JobLock mLock;
		JobWaitingList mWaitList;
	};
}