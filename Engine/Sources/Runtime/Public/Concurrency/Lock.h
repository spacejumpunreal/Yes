#pragma once
#include "Yes.h"

#include "Public/Misc/Debug.h"

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
}