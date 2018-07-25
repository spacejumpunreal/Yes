#pragma once
#include "Demo18.h"
#include "IModule.h"

namespace Demo18
{
	struct RendererPass;
	struct RenderDeviceModule : public IModule
	{
		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;
		virtual void RenderPass(RendererPass* pass) = 0;
	};
	extern RenderDeviceModule* CreateRenderDeviceModule();
}