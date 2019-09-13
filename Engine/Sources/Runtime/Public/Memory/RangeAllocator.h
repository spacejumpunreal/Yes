#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Memory/SizeUtils.h"
#include "Runtime/Public/Misc/Debug.h"

#include <map>
#include <unordered_map>
#include <list>

namespace Yes
{
	template<typename RangeKey, typename RangePtr>
	struct RangeData
	{
		static const RangePtr UsedBitPos = sizeof(RangePtr) * 8 - 1;
		static const RangePtr UsedBitMask = ((RangePtr)1) << UsedBitPos;

		RangeKey Key;
		RangePtr Start;
		RangePtr Size;
		RangePtr GetSize() { return Size & ~UsedBitMask; }
		void MarkUsed() { Size |= UsedBitMask; }
		void MarkUnused() { Size &= ~UsedBitMask; }
		bool IsUsed() { return Size & UsedBitMask; }
	};

	struct AllocatorStats
	{
	protected:
		AllocatorStats()
			: mCount(0)
			, mUsed(0)
			, mReserved(0)
		{}
	public:
		void Stats(size_t& count, size_t& used, size_t& reserved)
		{
			count = mCount;
			used = mUsed;
			reserved = mReserved;
		}
	protected:
		size_t mCount, mUsed, mReserved;
	};
	struct MallocIntAllocateAPI
	{
		using RangeKey = int*;
		using RangePtr = size_t;
	protected:
		RangeKey AllocRange(RangePtr requestedSize)
		{
			return (RangeKey)malloc(requestedSize);
		}
		void FreeRange(RangeKey key)
		{
			free((void*)key);
		}
	public:
		void AdjustSomething()
		{}
	};

	template<typename _RangeKey, typename _RangePtr, typename AllocateAPI>
	struct LinearBlockRangeAllocator : public AllocatorStats, public AllocateAPI
	{
		using RangeKey = _RangeKey;
		using RangePtr = _RangePtr;
		struct RangeInfo
		{
			RangeKey Key;
			RangePtr Size;
			size_t RefCount;
		};

		LinearBlockRangeAllocator(size_t minBlockSize, size_t maxReserveSize)
			: mCurrentRange(nullptr)
			, mMinBlockSize(minBlockSize)
			, mMaxReservation(maxReserveSize)
		{
		}
		void Allocate(RangeKey& allocatedKey, RangePtr& allocatedStart, size_t size, size_t align = 1)
		{
			align = align == 0 ? 1 : align;
			++mCount;
			bool currentOK = false;
			if (mCurrentRange != nullptr)
			{
				RangePtr alignedPtr = GetAlignedPtr(mAvailableOffset, align);
				RangePtr newPtr = alignedPtr + size;
				if (mCurrentRange->Size >= newPtr)
				{
					currentOK = true;
				}
			}
			if (!currentOK)
			{
				
				size_t newSize = size > mMinBlockSize ? size: mMinBlockSize;
				bool useCache = false;
				for (auto it = mFreeRanges.begin(); it != mFreeRanges.end(); ++it)
				{
					if (it->Size >= size)
					{
						auto iret = mUsedRanges.insert(std::make_pair(it->Key, RangeInfo{ it->Key, it->Size, 0 }));
						mUsed += it->Size;
						CheckAlways(iret.second);
						mFreeRanges.erase(it);
						useCache = true;
						mCurrentRange = &iret.first->second;
						break;
					}
				}
				if (!useCache)
				{
					RangeKey newKey = (*this).AllocateAPI::AllocRange(newSize);
					auto iret = mUsedRanges.insert(std::make_pair(newKey, RangeInfo{ newKey, newSize, 0 }));
					CheckAlways(iret.second);
					mUsed += newSize;
					mReserved += newSize;
					mCurrentRange = &iret.first->second;
				}
				mAvailableOffset = 0;
			}
			allocatedKey = mCurrentRange->Key;
			allocatedStart = GetAlignedPtr(mAvailableOffset, align);
			RangePtr nextAvailableOffset = allocatedStart + size;
			mAvailableOffset = nextAvailableOffset;
			++mCurrentRange->RefCount;
		}
		void Free(RangeKey key, RangePtr /*ptr*/)
		{
			--mCount;
			auto kit = mUsedRanges.find(key);
			CheckAlways(kit != mUsedRanges.end());
			RangeInfo& range = kit->second;
			--range.RefCount;
			if (range.RefCount == 0)
			{
				if (key == mCurrentRange->Key)
				{
					mCurrentRange = nullptr;
				}
				mUsed -= range.Size;
				if (CanFreeRange(range.Size))
				{
					(*this).AllocateAPI::FreeRange(range.Key);
					mReserved -= range.Size;
				}
				else
				{
					mFreeRanges.push_back(range);
				}
				mUsedRanges.erase(kit);

			}
		}
		void Reset()
		{
			mUsed = 0;
			mCount = 0;
			for (auto& p : mUsedRanges)
			{
				auto& range = p.second;
				range.RefCount = 0;
				if (CanFreeRange(range.Size))
				{
					(*this).AllocateAPI::FreeRange(range.Key);
					mReserved -= range.Size;
				}
				else
				{
					mFreeRanges.push_back(range);
				}
				
			}
			mUsedRanges.clear();
			mCurrentRange = nullptr;
		}
	private:
		bool CanFreeRange(size_t /*total*/)
		{
			return mReserved > mMaxReservation;
		}
		void AddNewRange(size_t size)
		{
			//free list 
			RangePtr newSize = mMinBlockSize > size ? mMinBlockSize : size;
			RangeKey k = (*this).AllocateAPI::AllocRange(newSize);
			mUsedRanges.insert(std::make_pair(k, RangeInfo{ k, newSize, 1 }));
			mAvailableOffset = 0;
		}
		std::unordered_map<RangeKey, RangeInfo> mUsedRanges;
		std::list<RangeInfo> mFreeRanges;
		RangeInfo* mCurrentRange;
		RangePtr mAvailableOffset;
		RangePtr mMinBlockSize;
		size_t mMaxReservation;
	};

	template<typename _RangeKey, typename _RangePtr, typename AllocateAPI>
	struct BestFitRangeAllocator : public AllocatorStats, public AllocateAPI
	{
		using RangeKey = _RangeKey;
		using RangePtr = _RangePtr;
		using RangeData = RangeData<RangeKey, RangePtr>;
		static_assert(std::is_same_v<_RangeKey, AllocateAPI::RangeKey>, "RangeKey should match");
		static_assert(std::is_same_v<_RangePtr, AllocateAPI::RangePtr>, "RangePtr should match");

		struct KeyAndStart
		{
			RangeKey Key;
			RangePtr Start;
			friend static bool operator==(const KeyAndStart& l, const KeyAndStart& r)
			{
				return l.Key == r.Key && l.Start == r.Start;
			}
		};
		struct KeyAndStartHasher
		{
			std::size_t operator()(const KeyAndStart& key) const
			{
				return ((size_t)key.Key) ^ ((size_t)key.Start);
			}
		};
		struct KeyAndStartEqual
		{
			bool operator() (const KeyAndStart& l, const KeyAndStart& r) const
			{
				return l == r;
			}
		};
		typedef std::list<RangeData> RangeList;
		typedef std::unordered_map<RangeKey, RangeList> Key2RangeList;
		typedef std::multimap<RangePtr, typename RangeList::iterator> Size2Ranges;
		typedef std::unordered_map<KeyAndStart, typename RangeList::iterator, KeyAndStartHasher, KeyAndStartEqual> KeyAndStart2Range;

	public:
		BestFitRangeAllocator(size_t minBlockSize, size_t maxReservation)
			: mMinBlockSize(minBlockSize)
			, mMaxReservation(maxReservation)
		{}
		void Allocate(RangeKey& allocatedKey, RangePtr& allocatedStart, size_t size, size_t align = 1)
		{
			++mCount;
			align = align == 0 ? 1 : align;
			auto it = mSize2Range.lower_bound(size);
			KeyAndStart kas;
			while (it != mSize2Range.end())
			{
				typename RangeList::iterator sizeMatchedRangeItr = it->second;
				RangePtr alignedPtr = GetAlignedPtr(sizeMatchedRangeItr->Start, align);
				RangePtr newStart = alignedPtr + size;
				if (newStart > sizeMatchedRangeItr->Start + sizeMatchedRangeItr->GetSize())
				{//failed due to align, try next
					++it;
					continue;
				}
				mUsed += newStart - sizeMatchedRangeItr->Start;
				mSize2Range.erase(it);
				RangePtr leftSize = sizeMatchedRangeItr->Start + sizeMatchedRangeItr->GetSize() - newStart;
				if (leftSize != 0)
				{//add new
					auto afterIt = sizeMatchedRangeItr;
					++afterIt;
					typename RangeList::iterator inserted = mRanges[sizeMatchedRangeItr->Key].insert(
						afterIt,
						RangeData{ sizeMatchedRangeItr->Key, newStart, leftSize });
					sizeMatchedRangeItr->Size = newStart - sizeMatchedRangeItr->Start;
					sizeMatchedRangeItr->MarkUsed();
					mSize2Range.insert(std::make_pair(leftSize, inserted));
				}
				sizeMatchedRangeItr->MarkUsed();
				allocatedKey = sizeMatchedRangeItr->Key;
				allocatedStart = alignedPtr;
				//add to allocated hashtable
				kas = { allocatedKey , allocatedStart };
				auto ret = mAllocated.insert(std::make_pair(kas, sizeMatchedRangeItr));
				CheckAlways(ret.second == true);
				return;
			}
			//need to add new block
			RangePtr newRangeSize = size > mMinBlockSize ? size : mMinBlockSize;
			RangeKey newRangeKey = (*this).AllocateAPI::AllocRange(newRangeSize);
			CheckAlways(mRanges.count(newRangeKey) == 0);
			//build linked list anyway
			RangeList& newList = mRanges[newRangeKey];
			RangeData& rd = newList.emplace_back();
			rd.Key = newRangeKey;
			rd.Start = 0;
			rd.Size = newRangeSize;
			if (newRangeSize > size)
			{
				rd.Size = size;
				RangeData& rd2 = newList.emplace_back();
				rd2.Key = newRangeKey;
				rd2.Start = size;
				rd2.Size = newRangeSize - (RangePtr)size;
				mSize2Range.insert(std::make_pair(rd2.Size, --newList.end()));
			}
			mUsed += size;
			mReserved += newRangeSize;
			rd.MarkUsed();
			allocatedKey = newRangeKey;
			allocatedStart = 0;
			kas = KeyAndStart{ allocatedKey, allocatedStart };
			auto ret = mAllocated.insert(std::make_pair(kas, newList.begin()));
			CheckAlways(ret.second == true);
		}
		void Free(RangeKey key, RangePtr ptr)
		{
			--mCount;
			KeyAndStart kas{ key, ptr };
			auto it = mAllocated.find(kas);
			CheckAlways(it != mAllocated.end());
			//clean from mAllocated
			auto freedRangeIt = it->second;
			CheckDebug(freedRangeIt->IsUsed());
			mUsed -= freedRangeIt->GetSize();
			freedRangeIt->MarkUnused();
			mAllocated.erase(it);
			//merge ranges in mRanges
			RangeList& rl = mRanges[key];
			{
				auto forwardIt = freedRangeIt;
				++forwardIt;
				while (forwardIt != rl.end())
				{
					if ((!forwardIt->IsUsed()) && forwardIt->Start == freedRangeIt->Start + freedRangeIt->Size)
					{
						freedRangeIt->Size += forwardIt->Size;
						RemoveFromSize2Range(forwardIt);
						forwardIt = rl.erase(forwardIt);
					}
					else
					{
						break;
					}
				}
			}
			while (freedRangeIt != rl.begin())
			{
				auto backwardIt = freedRangeIt;
				--backwardIt;
				if ((!backwardIt->IsUsed()) && backwardIt->Start + backwardIt->Size == freedRangeIt->Start)
				{
					freedRangeIt->Start = backwardIt->Start;
					freedRangeIt->Size += backwardIt->Size;
					RemoveFromSize2Range(backwardIt);
					rl.erase(backwardIt);
				}
				{
					break;
				}
			}
			if (rl.size() == 1 && CanFreeRange(freedRangeIt->Size))
			{//back to 1 piece now
				AllocateAPI::FreeRange(key);
				mReserved -= freedRangeIt->Size;
				mRanges.erase(key);
			}
			else
			{
				mSize2Range.insert(std::make_pair(freedRangeIt->Size, freedRangeIt));
			}
		}
		void Reset()
		{
			mCount = mUsed = 0;
			mAllocated.clear();
			mSize2Range.clear();
			for (auto it = mRanges.begin(); it != mRanges.end();)
			{
				RangeKey key = it->first;
				RangeList& lst = it->second;
				RangePtr total = 0;
				for (auto& v : lst)
				{
					total += v.GetSize();
				}
				lst.clear();
				bool deleted;
				if (CanFreeRange(total))
				{
					deleted = true;
					AllocateAPI::FreeRange(key);
				}
				else
				{
					deleted = false;
				}
				if (!deleted)
				{
					lst.push_back(RangeData{ key, 0, total });
					++it;
				}
				else
				{
					mReserved -= total;
					it = mRanges.erase(it);
				}
			}
		}
	private:
		bool CanFreeRange(size_t /*total*/)
		{
			return mReserved > mMaxReservation;
		}
		void RemoveFromSize2Range(typename RangeList::iterator valueInSize2Range)
		{
			size_t size = valueInSize2Range->Size;
			auto lb = mSize2Range.lower_bound(size);
			while ((&*lb->second) != (&*valueInSize2Range))
			{
				++lb;
			}
			mSize2Range.erase(lb);
		}
		Key2RangeList mRanges;
		Size2Ranges mSize2Range;
		KeyAndStart2Range mAllocated;
		size_t mMinBlockSize;
		size_t mMaxReservation;
	};
}
