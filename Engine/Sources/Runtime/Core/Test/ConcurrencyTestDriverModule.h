#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Concurrency/JobUtils.h"

namespace Yes
{
	class ConcurrencyTestDriverModule : public IModule
	{

		DECLARE_MODULE_IN_CLASS(ConcurrencyTestDriverModule);
	};
}