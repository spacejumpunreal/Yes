#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Misc/Name.h"
#include "Runtime/Public/Misc/Debug.h"

#include <map>
#include <vector>
#include <list>
#include <cstdio>

namespace Yes
{
	int Main(int, const char**)
	{
		std::vector<const char*> args
		{
			"NumberOfJobsThreads=4",
			R"(FileModuleBasePath=C:\checkout\Yes\Resources\)",
			"module=WindowsWindowModule",
		};
		auto sys = new Yes::System();
		sys->Initialize(args.size(), args.data());

		const Name aName("SomeName1");
		Name bName("SomeName1");
		CheckAlways(aName.GetHash() == bName.GetHash());
		printf("sizeof(Name) == %zd", sizeof(Name));

		return 0;
	}
}
int main(int argc, const char** argv)
{
	return Yes::Main(argc, argv);
}

