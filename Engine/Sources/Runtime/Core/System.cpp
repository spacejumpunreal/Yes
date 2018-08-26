#include "Yes.h"
#include "Core/System.h"
#include "Misc/Utils.h"
#include "Misc/Debug.h"
#include "Misc/Time.h"

#include "Core/ModuleRegistry.h"
#include "Core/MemoryModule.h"
#include "Core/FileModule.h"
#include "Core/TickModule.h"

namespace Yes
{
	struct System::SystemPrivateData
	{
	public:
		IModule* mModules[MaxModuleCount];
		TimeDuration mFrameDuration;
	public:
		SystemPrivateData()
			: mFrameDuration(1.0 / 60)
		{
			ZeroFill(mModules);
			Check((int)EModuleRegistry::EIntrisicModuleCount < (int)MaxModuleCount);
		}
	};

	System* GSystem = nullptr;
	System::System(SystemPrivateData* pData)
		: mPrivate(pData)
	{
		GSystem = this;
		ADD_MODULE(MemoryModule);
		ADD_MODULE(FileModule);
		ADD_MODULE(TickModule);
	}
	System::System()
		: System(new SystemPrivateData())
	{}

	void System::Initialize()
	{
		for (int i = 0; i < ARRAY_COUNT(mPrivate->mModules); ++i)
		{
			if (mPrivate->mModules[i])
			{
				mPrivate->mModules[i]->InitializeModule();
			}
		}
		for (int i = 0; i < ARRAY_COUNT(mPrivate->mModules); ++i)
		{
			if (mPrivate->mModules[i])
			{
				mPrivate->mModules[i]->Start();
			}
		}
	}

	IModule* System::GetModule(EModuleRegistry name)
	{
		int i = (int)name;
		return mPrivate->mModules[i];
	}

	void System::RegisterModule(EModuleRegistry name, IModule* module)
	{
		int idx = (int)name;
		mPrivate->mModules[idx] = module;
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
			auto dtsec = dt.ToSeconds();
			auto dursec = mPrivate->mFrameDuration.ToSeconds();
			auto d = mPrivate->mFrameDuration - dt;
			auto sec = d.ToSeconds();
			if (sec > 0)
				Sleep(d);
		}
	}
}

