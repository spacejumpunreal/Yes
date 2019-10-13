#pragma once

#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Core/IDatum.h"

namespace Yes
{
	class RenderDeviceTestDriverDatum;
	class RenderDeviceTestDriverModule : public IModule
	{
		DECLARE_MODULE_IN_CLASS(RenderDeviceTestDriverModule);
	};
}

