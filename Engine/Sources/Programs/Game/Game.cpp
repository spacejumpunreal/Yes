#include "Core/System.h"
#include "Core/ModuleRegistry.h"
#include "Core/TickModule.h"
#include "Misc/Time.h"

#include "SWR.h"

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
int main()
{

	auto sys = new Yes::System();

	sys->Initialize();

	Yes::TickModule* tickModule = GET_MODULE(TickModule);
	tickModule->AddTickable(new TestTick());
	start = Yes::TimeStamp::Now();
	func();

	sys->Loop();
	
	return 0;
}