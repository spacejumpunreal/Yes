#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Core/ITickable.h"

namespace Yes
{
	class FrameContext
	{
	public:
	};

	class FrameLogicModule : public IModule
	{
	public:
		virtual void StartFrame() = 0;
		virtual void RegisterDatum() = 0;
	};
}