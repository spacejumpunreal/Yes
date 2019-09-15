#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/Debug.h"

#include <atomic>
#include <mutex>

namespace Yes
{
	class LockGuard
	{
	public:
		void Lock()
		{
			CheckAlways(mLock.try_lock());
		}
		void Unlock()
		{
			mLock.unlock();
		}
	private:
		std::mutex mLock;
	};

	class SimpleSpinLock
	{//note: I thought about acquire/release, it will allow critical sections to overlap, so I decide to use sequential consistency
	public:
		SimpleSpinLock()
		{
			mFlag.clear();
		}
		void Lock()
		{
			while (mFlag.test_and_set())
			{
			}
		}
		bool TryLock()
		{
			return mFlag.test_and_set();
		}
		void Unlock()
		{
			mFlag.clear();
		}
	private:
		std::atomic_flag mFlag;
	};

	template<typename TLock>
	class AutoLock
	{
	public:
		AutoLock(TLock& lk)
			: mLock(&lk)
		{
			lk.Lock();
		}
		~AutoLock()
		{
			mLock->Unlock();
		}
	private:
		TLock* mLock;
	};
}