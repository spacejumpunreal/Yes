#pragma once

#include "Yes.h"
#include "Core/IModule.h"

namespace Yes
{
	DECLARE_MODULE(WindowsWindowModule)
	{
	public:
		virtual void* GetWindowHandle() = 0;
		virtual void GetWindowRect(int& width, int& height) = 0;
	};
}