#pragma once

#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Core/TickModule.h"

namespace Yes
{
	struct InputStateDatum;
	class IWindowModule : public IModule
	{
	public:
		virtual void* GetWindowHandle() = 0;
		virtual void GetWindowRect(int& width, int& height) = 0;
	};
}