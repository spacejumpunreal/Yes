#include "Runtime/Public/Concurrency/JobUtils.h"
#include "Runtime/Public/Concurrency/Fiber.h"
#include "Runtime/Public/Core/ConcurrencyModule.h"
#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Misc/Debug.h"
#include <vector>

namespace Yes
{
	JobUnitePoint::JobUnitePoint(size_t initialCount, ThreadFunctionPrototype function, void* arg)
		: mCount(initialCount)
		, mJobData{ function, arg }
	{
		CheckDebug(initialCount > 0);
	}
	void JobUnitePoint::Unite()
	{
		size_t old = mCount.fetch_sub(1, std::memory_order_relaxed);
		if (old != 1)
			return;
		mJobData.Function(mJobData.Context);
	}

	void JobWaitingList::Append(Fiber* fiber)
	{
		ListLock.Lock();
		Entries.push_back(fiber);
		ListLock.Unlock();
	}
	void JobWaitingList::Pop(size_t n)
	{
		std::vector<Fiber*> fibers;
		ListLock.Lock();
		for (size_t i = 0; i < n && Entries.size() > 0; ++i)
		{
			fibers.emplace_back(Entries.front());
			Entries.pop_front();
		}
		ListLock.Unlock();
		if (fibers.size() > 0)
			GET_MODULE(ConcurrencyModule)->RescheduleFibers(fibers.data(), fibers.size());
	}

	void JobConditionVariable::Wait(JobLock* lock)
	{
		//lock should be already acquired by current fiber
		mWaitList.Append(Fiber::GetCurrentFiber());
		auto module = GET_MODULE(ConcurrencyModule);
		module->SwitchOutAndReleaseLock(lock);
		//after switch back in, need to get the lock again
		lock->Lock();
	}

	void JobConditionVariable::NotifyOne()
	{
		mWaitList.Pop(1);
	}

	void JobConditionVariable::NotifyAll()
	{
		mWaitList.Pop(std::numeric_limits<size_t>::max());
	}

	JobSemaphore::JobSemaphore(size_t count)
		: mCount(count)
	{
	}

	void JobSemaphore::Increase()
	{
		mLock.Lock();
		++mCount;
		mLock.Unlock();
		mWaitList.Pop(1);
	}

	void JobSemaphore::Decrease()
	{
		while (true)
		{
			mLock.Lock();
			if (mCount > 0)
			{
				--mCount;
				mLock.Unlock();
				return;
			}
			mWaitList.Append(Fiber::GetCurrentFiber());
			auto module = GET_MODULE(ConcurrencyModule);
			module->SwitchOutAndReleaseLock(&mLock);
		}
		
	}
}