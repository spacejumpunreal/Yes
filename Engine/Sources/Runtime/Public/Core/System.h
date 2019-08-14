#pragma once
#include "Core/IModule.h"

#include <map>

namespace Yes
{
	using ModuleCreatorFunction = IModule* (*)();
	using ArgMap = std::multimap<std::string, std::string>;

	extern System* GSystem;

	struct ModuleDescription
	{
		ModuleDescription(const char* name, float initOrder, ModuleCreatorFunction creator, ModuleID moduleID)
			: ModuleID(moduleID)
			, InitOrder(initOrder)
			, Name(name)
			, Creator(creator)
		{}
		ModuleID ModuleID;
		float InitOrder;
		const char* Name;
		ModuleCreatorFunction Creator;
	};
	class System 
	{
	protected:
		struct SystemPrivateData;
	public:
		System();
		void Initialize(size_t argc, const char** argv);
		IModule* GetModule(ModuleID moduleID);
		//used for pre main module registry
		static void RegisterModule(const ModuleDescription& desc);
		void SetFPS(float fps);
		void Loop();
		//get parameters
		const ArgMap& GetArguments() const;
	protected:
		SystemPrivateData *mPrivate;
	};

}

#define GET_MODULE(TypeName) dynamic_cast<Yes::TypeName*>(Yes::GSystem->GetModule(Yes::TypeName::ModuleIdentifier))
#define GET_MODULE_AS(TypeName, TypeAs) dynamic_cast<Yes::TypeAs*>(Yes::GSystem->GetModule(TypeName::ModuleIdentifier))