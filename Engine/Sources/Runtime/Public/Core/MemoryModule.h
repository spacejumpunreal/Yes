#pragma once
#include "Yes.h"
#include "IModule.h"
#include "Misc/IAllocator.h"

namespace Yes
{
	using AllocatorTag = int32;
	class MemoryModule : public IModule
	{
		virtual IAllocator* GetAllocator(AllocatorTag tag) = 0;
		virtual IAllocator* CreateAllocator(AllocatorTag tag) = 0;
		DECLARE_MODULE_IN_CLASS(MemoryModule);
	};
}