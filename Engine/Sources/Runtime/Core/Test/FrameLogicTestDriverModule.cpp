#include "Runtime/Core/Test/FrameLogicTestDriverModule.h"
#include "Runtime/Public/Core/FrameLogicModule.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Core/IDatum.h"
#include "Runtime/Profile/Benchmarks.h"
#include <vector>
#include <algorithm>

namespace Yes
{
	struct FrameLogicTestDriverModuleImp : public FrameLogicTestDriverModule
	{
	public:
		virtual void Start(System*);
	DEFINE_MODULE_IN_CLASS(FrameLogicTestDriverModule, FrameLogicTestDriverModuleImp);
	};
	DEFINE_MODULE_REGISTRY(FrameLogicTestDriverModule, FrameLogicTestDriverModuleImp, 0);

	struct TestTask00Datum : public IDatum
	{
		int Counts[64];
		std::atomic<int> GCount;
		static TestTask00Datum* Task(IFrameContext*)
		{
			TestTask00Datum* ret = new TestTask00Datum();
			RUN_REF_BENCHMARK(BubbleSortLinkedList<0>, 1);
			memset(ret->Counts, 0, sizeof(int[64]));
			return ret;
		}
	};

	struct TestTask10Datum : public IDatum
	{
		static TestTask10Datum* Task(IFrameContext*, TestTask00Datum* source)
		{
			TestTask10Datum* ret = new TestTask10Datum();
			RUN_REF_BENCHMARK(BubbleSortLinkedList<1>, 1);
			source->Counts[1] = source->GCount.fetch_add(1, std::memory_order_relaxed);
			return ret;
		}
	};

	struct TestTask11Datum : public IDatum
	{
		static TestTask11Datum* Task(IFrameContext*, TestTask00Datum* source)
		{
			TestTask11Datum* ret = new TestTask11Datum();
			RUN_REF_BENCHMARK(FixedPointSqrt2<1>, 2);
			source->Counts[2] = source->GCount.fetch_add(1, std::memory_order_relaxed);
			return ret;
		}
	};

	struct TestTask12Datum : public IDatum
	{
		static TestTask12Datum* Task(IFrameContext*, TestTask00Datum* root)
		{
			TestTask12Datum* ret = new TestTask12Datum();
			RUN_REF_BENCHMARK(FixedPointSqrt2<1>, 1);
			root->Counts[3] = root->GCount.fetch_add(1, std::memory_order_relaxed);
			return ret;
		}
	};

	struct TestTask20Datum : public IDatum
	{
		static TestTask20Datum* Task(IFrameContext*, TestTask00Datum* source, TestTask10Datum*, TestTask11Datum*)
		{
			TestTask20Datum* ret = new TestTask20Datum();
			source->Counts[4] = source->GCount.fetch_add(1, std::memory_order_relaxed);
			return ret;
		}
	};

	struct TestTask21Datum : public IDatum
	{
		static TestTask21Datum* Task(IFrameContext*, TestTask00Datum* source, TestTask10Datum*, TestTask11Datum*)
		{
			TestTask21Datum* ret = new TestTask21Datum();
			source->Counts[5] = source->GCount.fetch_add(1);
			return ret;
		}
	};

	struct FrameLogicTestDriverDatum : public IDatum
	{
		static FrameLogicTestDriverDatum* Task(IFrameContext*, TestTask00Datum* source, TestTask20Datum*, TestTask21Datum*)
		{
			FrameLogicTestDriverDatum* ret = new FrameLogicTestDriverDatum();
			source->Counts[6] = source->GCount.fetch_add(1, std::memory_order_relaxed);
			return ret;
		}
	};
	
	void FrameLogicTestDriverModuleImp::Start(System*)
	{
		FrameLogicModule* fm = GET_MODULE(FrameLogicModule);
		fm->RegisterTask(TestTask00Datum::Task);
		fm->RegisterTask(TestTask10Datum::Task);
		fm->RegisterTask(TestTask11Datum::Task);
		fm->RegisterTask(TestTask12Datum::Task);
		fm->RegisterTask(TestTask20Datum::Task);
		fm->RegisterTask(TestTask21Datum::Task);
		fm->RegisterTask(FrameLogicTestDriverDatum::Task);
	}
}