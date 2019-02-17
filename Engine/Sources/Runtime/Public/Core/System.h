#pragma once

#include "Core/ModuleRegistry.h"

namespace Yes
{
	const size_t MaxModuleCount = 128;
	class IModule;
	enum class EModuleRegistry;
	class System;

	extern System* GSystem;

	class System 
	{
	protected:
		struct SystemPrivateData;
		System(SystemPrivateData* pdata);
	public:
		System();
		void Initialize();
		IModule* GetModule(EModuleRegistry name);
		void RegisterModule(EModuleRegistry name, IModule* module);
		void SetFPS(float fps);
		void Loop();
	protected:
		SystemPrivateData *mPrivate;
	};
}
#define ADD_MODULE(TYPE) Yes::GSystem->RegisterModule(Yes::EModuleRegistry::E##TYPE, Yes::Create##TYPE())
#define GET_MODULE(TYPE) (dynamic_cast<Yes::TYPE*>(Yes::GSystem->GetModule(Yes::EModuleRegistry::E##TYPE)))
