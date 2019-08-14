#pragma once
#include "Yes.h"
#include "Public/Core/IModule.h"
#include "Public/Concurrency/JobUtils.h"

namespace Yes
{
	class ConcurrencyTestDriverModule : public IModule
	{

		DECLARE_MODULE_IN_CLASS(ConcurrencyTestDriverModule);
	};
}