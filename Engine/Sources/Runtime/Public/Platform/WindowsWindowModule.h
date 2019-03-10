#pragma once

#include "Yes.h"
#include "Core/IModule.h"

namespace Yes
{
	class WindowsWindowModule : public IModule
	{
	public:
		virtual void* GetWindowHandle() = 0;
		virtual void GetWindowRect(int& width, int& height) = 0;

		DECLARE_MODULE_IN_CLASS(WindowsWindowModule);
	};
}