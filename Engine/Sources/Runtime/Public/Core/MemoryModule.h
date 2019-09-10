#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Misc/IAllocator.h"

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