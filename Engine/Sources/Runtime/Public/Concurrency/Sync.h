#pragma once
#include "Misc/Debug.h"
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace Yes
{
	template<typename LockType = std::mutex>
	class Semaphore
	{
	private:
		LockType mLock;
		std::condition_variable mCondition;
		std::atomic<size_t> mCount;
	public:
		Semaphore(size_t count = 0)
			: mCount(count)
		{}
		void Increase()
		{
			mCount.fetch_add(1, std::memory_order_acq_rel);
			mCondition.notify_one();
		}
		void Decrease()
		{
			std::unique_lock<LockType> lock(mLock);
			while (true)
			{
				size_t a = mCount.load(std::memory_order_acquire);
				if (a == 0)
				{
					mCondition.wait(lock);
				}
				else
				{
					size_t aa = a - 1;
					bool success = mCount.compare_exchange_weak(a, aa, std::memory_order_acq_rel);
					if (success)
					{
						break;
					}
					else
					{
						mCondition.notify_one();
					}
				}
			}
		}
		bool TryDecrease()
		{
			size_t a = mCount.load(std::memory_order_relaxed);
			if (a == 0)
				return false;
			size_t aa = a - 1;
			if (!mCount.compare_exchange_weak(aa, a, std::memory_order_acq_rel))
			{
				return false;
			}
			return true;
		}
	};
	template<typename ElemType, typename LockType = std::mutex>
	class RingBuffer
	{
	private:
		LockType mLock;
		std::condition_variable mReadCond;
		std::condition_variable mWriteCond;
		size_t mMaxSize;
		size_t mSize;
		ElemType* mData;
		ElemType* mWritePtr;
		ElemType* mReadPtr;
	public:
		RingBuffer(size_t maxSize = 0)
			: mMaxSize(maxSize)
			, mData(new ElemType[maxSize])
			, mWritePtr(mData)
			, mReadPtr(mData)
			, mSize(0)
		{}
		size_t MaxSize() { return mMaxSize; };
		size_t Size() { return mSize; }
		ElemType Get()
		{
			std::unique_lock<LockType> lock(mLock);
			while (mSize == 0)
			{
				mReadCond.wait(lock);
			}
			auto full = mSize == mMaxSize;
			auto rv = *mReadPtr++;
			--mSize;
			if (mReadPtr == mData + mMaxSize)
				mReadPtr = mData;
			if (full)
				mWriteCond.notify_one();
			return rv;
		}
		void Put(const ElemType& e)
		{
			std::unique_lock<LockType> lock(mLock);
			while (mSize == mMaxSize)
			{
				mWriteCond.wait(lock);
			}
			auto empty = mSize == 0;
			(*mWritePtr++) = e;
			++mSize;
			if (mWritePtr == mData + mMaxSize)
				mWritePtr = mData;
			if (empty)
				mReadCond.notify_one();
		}
	};
	
	class SyncPoint
	{
	public:
		SyncPoint(size_t n)
			: mToWait(n)
		{
			CheckDebug(n > 0);
		}
		void Sync()
		{
			std::unique_lock<std::mutex> lk(mLock);
			CheckDebug(mToWait >= 1);
			if (mToWait > 1)
			{
				--mToWait;
				mCondition.wait(lk);
				CheckDebug(mToWait == 0);
				lk.unlock();
			}
			else
			{
				mToWait = 0;
				lk.unlock();
				mCondition.notify_all();
			}
		}

	protected:
		std::mutex mLock;
		std::condition_variable mCondition;
		size_t mToWait;
	};

}