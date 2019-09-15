#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Misc/Utils.h"
#include "Runtime/Public/Misc/Debug.h"
#include "Runtime/Public/Misc/Time.h"
#include "Runtime/Public/Concurrency/Thread.h"
#include "Runtime/Public/Core/ModuleRegistry.h"
#include "Runtime/Public/Core/MemoryModule.h"
#include "Runtime/Public/Core/FileModule.h"
#include "Runtime/Public/Core/TickModule.h"
#include <unordered_map>
#include <vector>
#include <set>
#include <unordered_set>

namespace Yes
{
	struct ModuleDescriptionCmp;

	using ModuleMap = std::unordered_map<ModuleID, IModule*>;
	


	System* GSystem = nullptr;

	struct ModuleDescriptionCmp
	{
		bool operator() (const ModuleDescription& lhs, const ModuleDescription& rhs) const
		{
			return lhs.InitOrder < rhs.InitOrder;
		}
	};
	using ModuleDescriptionSet = std::multiset<ModuleDescription, ModuleDescriptionCmp>;
	struct System::SystemPrivateData
	{
		ModuleDescriptionSet mRegisteredModules;
		ModuleMap mModules;
		ArgMap mArgs;
		TimeDuration mFrameDuration;

		SystemPrivateData()
			: mFrameDuration(1.0 / 60)
		{
		}
		void ParseArgs(size_t argc, const char** argv)
		{
			for (int i = 0; i < argc; ++i)
			{
				const char* p = argv[i];
				const char* s = p;
				while (*p != '=' && *p != 0)
				{
					++p;
				}
				const char* m = p;
				CheckAlways(*p != 0);
				while (*++p)
				{
				}
				mArgs.insert(std::make_pair(std::string(s, m), std::string(m + 1, p)));
			}
		}
	};
	
	System::System()
		: mPrivate(new SystemPrivateData())
	{
		CheckAlways(GSystem == nullptr);
		GSystem = this;
		CollectAllModules();
	}

	const ArgMap& System::GetArguments() const
	{
		return mPrivate->mArgs;
	}
	void System::Initialize(size_t argc, const char** argv)
	{
		mPrivate->ParseArgs(argc, argv);
		new Thread(InitAsMainThread{});
		mPrivate->mArgs.insert(std::make_pair("module", "ConcurrencyModule"));
		mPrivate->mArgs.insert(std::make_pair("module", "TickModule"));
		mPrivate->mArgs.insert(std::make_pair("module", "FileModule"));
		//find need to initialize modules from args
		ArgMap::iterator s, e;
		std::tie(s, e) = mPrivate->mArgs.equal_range("module");
		std::unordered_set<std::string> modulesToInitialize;
		std::unordered_map<ModuleID, const ModuleDescription*> initializedDescriptions;
		while (s != e)
		{
			modulesToInitialize.insert(s->second);
			++s;
		}
		//enumerate available modules and create
		for (auto& i : mPrivate->mRegisteredModules)
		{
			std::string n = i.Name;
			if (!modulesToInitialize.count(n))
				continue;
			if (mPrivate->mModules.count(i.ModuleID) > 0)
			{
				const char* usedName = initializedDescriptions[i.ModuleID]->Name;
				CheckAlways(false, usedName);
			}
			IModule* mod = mPrivate->mModules[i.ModuleID] = i.Creator();
			initializedDescriptions[i.ModuleID] = &i;
			mod->InitializeModule(this);
		}
		//initialize
		for (auto& i : mPrivate->mRegisteredModules)
		{
			std::string n = i.Name;
			if (!modulesToInitialize.count(n))
				continue;
			mPrivate->mModules[i.ModuleID]->Start(this);
		}
	}

	IModule* System::GetModule(ModuleID moduleID)
	{
		auto it = mPrivate->mModules.find(moduleID);
		if (it == mPrivate->mModules.end())
		{
			return nullptr;
		}
		else
		{
			return it->second;
		}
	}

	void System::RegisterModule(const ModuleDescription& desc)
	{
		GSystem->mPrivate->mRegisteredModules.insert(desc);
	}
	void System::SetFPS(float fps)
	{
		mPrivate->mFrameDuration = 1.0f / fps;
	}
	void System::Loop()
	{
		TickModule* tickModule = GET_MODULE(TickModule);
		while (true)
		{
			auto t = TimeStamp::Now();
			tickModule->Tick();
			auto dt = TimeStamp::Now() - t;
			//auto dtsec = dt.ToSeconds();
			//auto dursec = mPrivate->mFrameDuration.ToSeconds();
			auto d = mPrivate->mFrameDuration - dt;
			auto sec = d.ToSeconds();
			if (sec > 0)
				Sleep(d);
		}
	}
}

