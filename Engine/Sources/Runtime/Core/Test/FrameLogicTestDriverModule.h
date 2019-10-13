#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"

namespace Yes
{
	struct FrameLogicTestDriverDatum;
	class FrameLogicTestDriverModule : public IModule
	{
		DECLARE_MODULE_IN_CLASS(FrameLogicTestDriverModule);
	};
}