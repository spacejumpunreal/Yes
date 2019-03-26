#include "Core/System.h"
#include "Core/TickModule.h"
#include "Core/FileModule.h"
#include "Misc/Time.h"

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

#define DEMO 1

int main(int argc, const char** argv)
{
	std::vector<const char*> args
	{
		"module=TickModule",
		"module=FileModule",
		R"(FileModuleBasePath=C:\checkout\Yes\Resources\)",
		"module=WindowsWindowModule",
#if DEMO
		"module=DX12DemoModule",
#else
		"module=DX12RenderDeviceModule",
		"module=RenderDeviceTestDriverModule",
#endif	
	};
	auto sys = new Yes::System((int)args.size(), args.data());
	sys->Initialize();
	Yes::TickModule* tickModule = GET_MODULE(TickModule);
	tickModule->AddTickable(new TestTick());
	start = Yes::TimeStamp::Now();
	sys->Loop();
	
	return 0;
}