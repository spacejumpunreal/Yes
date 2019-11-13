#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Misc/Time.h"
#include "Runtime/Public/Core/FrameLogicModule.h"
#include "Runtime/Public/Graphics/Test/RenderDeviceTestDriverModule.h"
#include "Runtime/Core/Test/FrameLogicTestDriverModule.h"
#include "Runtime/Public/Core/IDatum.h"
#include <map>
#include <list>
#include <cstdio>
#include <vector>


namespace Yes
{
	struct GameFinalTask : public IDatum
	{
		static GameFinalTask* Task(IFrameContext*, RenderDeviceTestDriverDatum*, FrameLogicTestDriverDatum*)
		//static GameFinalTask* Task(IFrameContext*, FrameLogicTestDriverDatum*)
		{
			return new GameFinalTask();
		}
	};
}

namespace Yes
{
	int Main(int, const char**)
	{
		std::vector<const char*> args
		{
			"NumberOfJobsThreads=4",
			R"(FileModuleBasePath=Resources)",
			"module=WindowsWindowModule",
			"module=DX12RenderDeviceModule",
			"module=RenderDeviceTestDriverModule",
			"module=FrameLogicTestDriverModule",
		};
		auto sys = new Yes::System();
		sys->Initialize(args.size(), args.data());
		GET_MODULE(FrameLogicModule)->RegisterTask(GameFinalTask::Task);
		GET_MODULE(FrameLogicModule)->SetConclusionTask<GameFinalTask>();
		sys->Loop();

		return 0;
	}
}

int main(int argc, const char** argv)
{
	return Yes::Main(argc, argv);
}

