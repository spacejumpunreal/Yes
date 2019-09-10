#pragma once

#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Graphics/RenderDevice.h"

namespace Yes
{
	class DX12RenderDeviceModule : public IModule, public RenderDevice
	{

	DECLARE_MODULE_IN_CLASS(DX12RenderDeviceModule);
	};
}
