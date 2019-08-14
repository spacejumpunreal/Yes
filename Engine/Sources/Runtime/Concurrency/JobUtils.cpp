#include "Public/Concurrency/JobUtils.h"
#include "Public/Misc/Debug.h"

namespace Yes
{
	JobSyncPoint::JobSyncPoint(size_t initialCount, ThreadFunctionPrototype function, void* arg)
		: mCount(initialCount)
		, mJobData{ function, arg }
	{
		CheckDebug(initialCount > 0);
	}
	void JobSyncPoint::Sync()
	{
		size_t old = mCount.fetch_sub(1, std::memory_order_release);
		if (old != 1)
			return;
		mJobData.Function(mJobData.Context);
	}

	JobSemaphore::JobSemaphore(size_t count)
		: mCount(count)
	{
	}

	void JobSemaphore::Increase()
	{
		mCount.fetch_add(1, std::memory_order_release);
	}

	void JobSemaphore::Decrease()
	{
		while (true)
		{
			size_t v = mCount.load(std::memory_order_acquire);
			if (v > 0)
			{
				size_t nv = v - 1;
				if (mCount.compare_exchange_strong(v, nv, std::memory_order_acq_rel))
				{
					break;
				}
			}
		}
	}

	bool JobSemaphore::TryDecrease()
	{
		size_t v = mCount.load(std::memory_order_acquire);
		if (v > 0)
		{
			size_t nv = v - 1;
			if (mCount.compare_exchange_strong(v, nv, std::memory_order_acq_rel))
			{
				return true;
			}
		}
		return false;
	}

}