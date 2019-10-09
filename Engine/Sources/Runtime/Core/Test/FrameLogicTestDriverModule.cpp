#include "Runtime/Core/Test/FrameLogicTestDriverModule.h"
#include "Runtime/Public/Core/FrameLogicModule.h"
#include <vector>
#include <algorithm>

namespace Yes
{
	struct FrameLogicTestDriverModuleImp : public FrameLogicTestDriverModule
	{
		virtual void Start(System*);
	};

	struct TestTask00Datum : public IDatum
	{
		std::vector<float> Data;
		static TestTask00Datum* GreatJobFunction(IFrameContext*)
		{
			auto x = new TestTask00Datum;
			int N = 1024 * 1024;
			x->Data.resize(1024 * 1024);
			for (int i = 0; i < N; ++i)
			{
				x->Data[i] = (float)-i;
			}
			return x;
		}
	};

	struct TestTask10Datum : public IDatum
	{
		std::vector<float>* Data;
		static IDatum* GreatJobFunction(IFrameContext*, TestTask00Datum* source)
		{
			int N2 = (int)source->Data.size() >> 1;
			std::sort(source->Data.data(), source->Data.data() + N2);
			TestTask10Datum* ret = new TestTask10Datum();
			ret->Data = &source->Data;
			return ret;
		}
	};

	struct TestTask11Datum : public IDatum
	{
		std::vector<float>* Data;
		static IDatum* GreatJobFunction(IFrameContext*, TestTask00Datum* source)
		{
			int N2 = (int)source->Data.size() >> 1;
			std::sort(source->Data.data() + N2, source->Data.data() + source->Data.size());
			auto ret = new TestTask11Datum;
			ret->Data = &source->Data;
			return ret;
		}
	};

	struct TestTask20Datum : public IDatum
	{
		int Nothing;
		static TestTask20Datum* GreatJobFunction(IFrameContext*, TestTask10Datum*, TestTask11Datum*)
		{
			auto x = new TestTask20Datum;
			x->Nothing = 1234;
			return x;
		}
	};
	
	void FrameLogicTestDriverModuleImp::Start(System*)
	{
		FrameLogicModule* fm = GET_MODULE(FrameLogicModule);
		fm->RegisterTask(TestTask00Datum::GreatJobFunction);
		fm->RegisterTask(TestTask10Datum::GreatJobFunction);
		fm->RegisterTask(TestTask11Datum::GreatJobFunction);
		fm->RegisterTask(TestTask20Datum::GreatJobFunction);
		fm->SetConclusionTask<TestTask00Datum>();
	}
}