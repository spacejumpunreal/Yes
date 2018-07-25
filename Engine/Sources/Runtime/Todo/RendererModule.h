#pragma once
#include "Demo18.h"
#include "IModule.h"
#include "Container.h"
#include "Camera.h"

namespace Demo18
{
	struct RendererScene
	{
		RendererCamera Camera;
	};

	struct RendererModule : public IModule
	{
		virtual void RunRenderer(RendererScene* scene) = 0;
	};

	extern RendererModule* CreateRendererModule();
}