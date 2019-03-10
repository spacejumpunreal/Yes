#pragma once
#include "Misc/StringUtils.h"

namespace Yes
{
	class System;
	using ModuleID = size_t;

	class IModule
	{
	public:
		virtual void InitializeModule(System* system) {}; // self initialization, other modules not ready
		virtual void Start(System* system) {} // other module's InitializeModule called already
	};
}

#define DECLARE_MODULE_IN_CLASS(ModuleName) public: static const ModuleID ModuleIdentifier = Yes::HornerHashCompileTime(#ModuleName);
#define DEFINE_MODULE_IN_CLASS(ModuleName, ImpClassName) public: static IModule* Create() { return new ImpClassName(); }
#define DEFINE_MODULE_REGISTRY(TypeName, ImpTypeName, InitOrder) void Register##TypeName()\
{\
	ModuleDescription desc(#TypeName, InitOrder, ImpTypeName::Create, TypeName::ModuleIdentifier); \
	System::RegisterModule(desc);\
}
