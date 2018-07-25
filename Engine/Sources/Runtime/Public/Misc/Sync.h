#pragma once
#include <mutex>

namespace Yes
{
	template<typename LockType = std::mutex>
	class Semaphore
	{
	private:
		LockType mLock;
		std::condition_variable mCondition;
		int mCount;
	public:
		Semaphore(int count = 0)
			: mCount(count)
		{}
		void Notify()
		{
			std::unique_lock<LockType> lock(mLock);
			++mCount;
			mCondition.notify_one();
		}
		void Wait()
		{
			std::unique_lock<LockType> lock(mLock);
			while (!mCount)
				mCondition.wait(lock);
			--mCount;

		}
		bool TryWait()
		{
			std::unique_lock<LockType> lock(mLock);
			if (mCount)
			{
				--mCount;
				return true;
			}
			else
				return false;
		}
	};
	using Signal = Semaphore<>;
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

}