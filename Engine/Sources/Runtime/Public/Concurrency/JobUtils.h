#pragma once
#include "Public/Concurrency/Concurrency.h"
#include <atomic>

namespace Yes
{
	struct JobData
	{
		ThreadFunctionPrototype Function;
		void* Context;
	};

	class JobSyncPoint
	{
	private:
		std::atomic<size_t> mCount;
		JobData mJobData;
	public:
		JobSyncPoint(size_t initialCount, ThreadFunctionPrototype function, void* arg);
		void Sync();
	};

	class JobLock
	{};

	class JobSemaphore
	{};
}