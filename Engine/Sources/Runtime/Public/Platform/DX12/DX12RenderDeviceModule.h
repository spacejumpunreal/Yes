#pragma once

#include "Yes.h"
#include "Core/IModule.h"
#include "Graphics/RenderDevice.h"

namespace Yes
{
	class DX12RenderDeviceModule : public IModule, public RenderDevice
	{

	DECLARE_MODULE_IN_CLASS(DX12RenderDeviceModule);
	};
}
