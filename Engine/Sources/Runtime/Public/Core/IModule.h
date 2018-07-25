#pragma once

namespace Yes
{
	class IModule
	{
	public:
		virtual void InitializeModule() {};
		virtual void Start() {}
	};
}

#define DEFINE_MODULE(ModuleName) \
	struct ModuleName##Imp : public ModuleName

#define DEFINE_MODULE_CREATOR(ModuleName) \
	IModule* Create##ModuleName()\
	{\
		return new ModuleName##Imp();\
	}

#define DECLARE_MODULE(ModuleName) IModule* Create##ModuleName(); struct ModuleName : public IModule