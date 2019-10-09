#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Core/TickModule.h"
#include "Runtime/Public/Core/FileModule.h"
#include "Runtime/Public/Misc/Time.h"
#include "Runtime/Public/Memory/RangeAllocator.h"

#include <map>
#include <list>
#include <cstdio>

class TestTick : public Yes::ITickable
{
	void Tick()
	{
		//printf("ticking");
	}
};
int cnt = 10;
Yes::TimeStamp start;
void func()
{
	printf("fuck:%d\n", cnt);
	--cnt;
	if (cnt > 0)
	{
		printf("dt:%lf\n", (Yes::TimeStamp::Now() - start).ToSeconds());
		Yes::TickModule* tickModule = GET_MODULE(TickModule);
		tickModule->AddCallback(1, &func);
	}
}

#define DEMO 0

namespace Yes
{
	void TestRangeAllocators();
	void TestMath();
}

namespace Yes
{
	int Main(int, const char**)
	{
		TestMath();
		std::vector<const char*> args
		{
			"NumberOfJobsThreads=4",
			R"(FileModuleBasePath=C:\checkout\Yes\Resources\)",
			"module=FrameLogicModule",
			"module=WindowsWindowModule",
	#if DEMO
			"module=DX12DemoModule",
	#else
			"module=DX12RenderDeviceModule",
			"module=RenderDeviceTestDriverModule",
			"module=FrameLogicTestDriverModule",
	#endif	
		};
		auto sys = new Yes::System();
		sys->Initialize(args.size(), args.data());
		Yes::TickModule* tickModule = GET_MODULE(TickModule);
		tickModule->AddTickable(new TestTick());
		start = Yes::TimeStamp::Now();
		sys->Loop();

		return 0;
	}
}
namespace Yes
{
	int TestFunctional();
}
int main(int argc, const char** argv)
{
	//return Yes::TestFunctional();
	return Yes::Main(argc, argv);
}

