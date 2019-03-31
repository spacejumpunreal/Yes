#include "Memory/RangeAllocator.h"
#include "Misc/Utils.h"
#include <cstdlib>
#include <random>
#include <map>

namespace Yes
{
	template<typename TestedAllocator>
	void TestRangeAllocator()
	{
		const int C = 2;
		const int N = 8;
		const int M = 16;
		const size_t ReserveSize = 32;
		TestedAllocator mallocator(M * C, ReserveSize);
		mallocator.AdjustSomething();
		if (true)
		{//basic aligned cases
			int* bases[N];
			size_t start[N];
			for (int i = 0; i < N; ++i)
			{
				mallocator.Allocate(bases[i], start[i], M);
				bases[i][start[i]] = i;
			}
			for (int i = 0; i < N / C; ++i)
			{
				for (int j = 0; j < C; ++j)
				{
					CheckAlways(bases[i * C] == bases[i * C + j]);
				}
			}
			{
				size_t count, used, reserved;
				mallocator.Stats(count, used, reserved);
				CheckAlways(count == N);
				CheckAlways(used == reserved && used == N * M);
			}
			
			for (int i = 0; i < N; ++i)
			{
				mallocator.Free(bases[i], start[i]);
			}
			{
				size_t count, used, reserved;
				mallocator.Stats(count, used, reserved);
				CheckAlways(count == 0);
				CheckAlways(used == 0);
				CheckAlways(reserved <= ReserveSize);
			}
		}
		if (true)
		{//non aligned
			size_t aligns[] = {1, 2, 4, 8, 16};
			for (int alignIdx = 0; alignIdx < ARRAY_COUNT(aligns); ++alignIdx)
			{
				size_t alignUsed = aligns[alignIdx];
				int* bases[N];
				size_t start[N];
				for (int i = 0; i < N; ++i)
				{
					mallocator.Allocate(bases[i], start[i], M + 3, alignUsed);
					int* addr = bases[i] + start[i];
					uint64 iaddr = (uint64)start;
					CheckAlways((iaddr & (alignUsed - 1)) == 0U);
					bases[i][start[i]] = i;
				}
				{
					size_t count, used, reserved;
					mallocator.Stats(count, used, reserved);
					CheckAlways(count == N);
				}
				for (int i = 0; i < N; ++i)
				{
					mallocator.Free(bases[i], start[i]);
				}
				{
					size_t count, used, reserved;
					mallocator.Stats(count, used, reserved);
					CheckAlways(count == 0);
					CheckAlways(used == 0);
					CheckAlways(reserved <= ReserveSize);
				}
			}
		}
		if (true)
		{//reset
			for (int i = 0; i < N; ++i)
			{
				int* base;
				size_t offset;
				mallocator.Allocate(base, offset, M);
			}
			mallocator.Reset();
			size_t count, used, reserved;
			mallocator.Stats(count, used, reserved);
			CheckAlways(count == 0 && used == 0 && reserved <= ReserveSize);
		}
		{//random
			bool log = true;
			struct Allocated
			{
				int* Base;
				size_t Offset;
			};
			const int RandomN = 1024 * 8;
			mallocator.Reset();
			std::map<int, Allocated> time2Allocated;
			int time = 0;
			size_t maxOptionalSize = 64;
			mallocator = TestedAllocator(maxOptionalSize * 2, maxOptionalSize * 2);
			size_t optionalAligns[] = { 1, 2, 4, 8, 16 };
			std::srand(888);
			int allocated = 0;
			for (int i = 0; i < RandomN * 2; ++i)
			{
				int rr = std::rand();
				if (time2Allocated.size() == 0 || 
					(allocated < RandomN && (rr & 1) == 0))
				{
					++allocated;
					Allocated& allocated = time2Allocated[time++];
					int r = std::rand();
					size_t allocationSize = (r % maxOptionalSize) + 1;
					size_t align = optionalAligns[r % ARRAY_COUNT(optionalAligns)];
					mallocator.Allocate(allocated.Base, allocated.Offset, allocationSize, align);
					if (log)
						printf("alloc\t%p,%zd\tsize=%zd,align=%zd\n", allocated.Base, allocated.Offset, allocationSize, align);
				}
				else
				{
					auto startK = time2Allocated.begin()->first;
					auto endK = time2Allocated.cbegin()->first;
					int range = endK - startK + 1;
					int k = std::rand() % range + startK;
					auto ii = time2Allocated.lower_bound(k);
					if (log)
						printf("free\t%p,%zd\n", ii->second.Base, ii->second.Offset);
					mallocator.Free(ii->second.Base, ii->second.Offset);
					time2Allocated.erase(ii);
					
				}
			}
			size_t count, used, reserved;
			mallocator.Stats(count, used, reserved);
			CheckAlways(count == 0 && used == 0 && reserved <= maxOptionalSize * 2);
		}
	}
	void TestRangeAllocators()
	{
		//TestRangeAllocator<BestFitRangeAllocator<int*, size_t, MallocIntAllocateAPI>>();
		TestRangeAllocator<LinearBlockRangeAllocator<int*, size_t, MallocIntAllocateAPI>>();
	}
}