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
}