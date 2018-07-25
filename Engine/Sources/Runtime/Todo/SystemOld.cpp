#include "Yes.h"
#include "Core/System.h"

namespace Demo18
{
	System* GSystem = nullptr;
	struct System::SystemImp
	{
		SystemImp()
			: mFrameDuration(1.0f / 60)
		{
			ADD_MODULE(File);
			ADD_MODULE(Tick);
			ADD_MODULE(Window);
			ADD_MODULE(Input);
			ADD_MODULE(Renderer);
			ADD_MODULE(RenderDevice);
			ADD_MODULE(TestMain);
		}
		void Initialize()
		{
			for (auto m : mModules)
			{
				m->InitializeModule();
			}
			for (auto m : mModules)
			{
				m->Start();
			}
		}
		IModule* GetModule(const char* name)
		{
			return mName2Module[name];
		}
		void SetFPS(float fps) { mFrameDuration = 1.0f / fps; }
		void Loop()
		{
			auto tickModule = static_cast<TickModule*>(mName2Module["Tick"]);
			while (true)
			{
				auto t = TimeStamp::Now();
				tickModule->Execute();
				auto dt = TimeStamp::Now() - t;
				auto dtsec = dt.ToSeconds();
				auto dursec = mFrameDuration.ToSeconds();
				auto d = mFrameDuration - dt;
				auto sec = d.ToSeconds();
				if (sec > 0)
					Sleep(d);
			}
		}
	protected:
		std::vector<IModule*>							mModules;
		std::unordered_map<const char*, IModule*>		mName2Module;
		TimeDuration									mFrameDuration;
	};
	System::System()
		: mImp(new System::SystemImp())
	{
		Check(GSystem == nullptr, "initialize once");
		GSystem = this;
	}
	void System::Initialize()
	{
		mImp->Initialize();
	}
	IModule * System::GetModule(const char * name)
	{
		return mImp->GetModule(name);
	}
	void System::SetFPS(float fps)
	{
		mImp->SetFPS(fps);
	}
	void System::Loop()
	{
		mImp->Loop();
	}
}

