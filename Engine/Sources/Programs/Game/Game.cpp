#include "Core/System.h"
#include "Core/ModuleRegistry.h"
#include "Core/TickModule.h"

#include <cstdio>

class TestTick : public Yes::ITickable
{
	void Tick()
	{
		printf("ticking");
	}
};
int main()
{
	auto sys = new Yes::System();
	Yes::TickModule* tickModule = GET_MODULE(TickModule);
	tickModule->AddTickable(new TestTick());
	sys->Loop();
	
	return 0;
}