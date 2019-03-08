#pragma once

namespace Yes
{
	class IModule
	{
	public:
		virtual void InitializeModule() {}; //self initialization, other modules not ready
		virtual void Start() {} // other module's InitializeModule called already
	};
	template<typename ModuleType>
	ModuleType* CreateModule();
}

#define DEFINE_MODULE(ModuleName) \
	struct ModuleName##Imp : public ModuleName

#define DEFINE_MODULE_CREATOR(ModuleName) \
	IModule* Create##ModuleName()\
	{\
		return new ModuleName##Imp();\
	}

#define DECLARE_MODULE(ModuleName) IModule* Create##ModuleName(); struct ModuleName : public IModule


#define DEFINE_MODULE_CREATOR1(ModuleName) \
	IModule* Create##ModuleName()\
	{\
		return new ModuleName();\
	}
#define DECLARE_MODULE_CREATOR(ModuleName) IModule* Create##ModuleName();



#define DECLARE_MODULE_CLASS(ClassName)
	template<>\
	ClassName* CreateModule<ClassName>()

#define DEFINE_MODULE_CLASS(ClassName)\
	static ClassName* gClassInstance;\
	template<>\
	ClassName* CreateModule<ClassName>()\
	{\
		return new ClassName();\
	}
	