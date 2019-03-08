#pragma once

namespace Yes
{
	class System;
	using ModuleCreatorFunction = IModule* (*)(System* system);
	struct ModuleRegistry
	{
		const char* Name;
		ModuleCreatorFunction CreatorFunction;
	};
}
