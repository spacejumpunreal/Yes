#pragma once
#include "Yes.h"
#include "IModule.h"
#include "Misc/IAllocator.h"

namespace Yes
{
	using AllocatorTag = int32;
	DECLARE_MODULE(MemoryModule)
	{
		IAllocator* GetAllocator(AllocatorTag tag);
		IAllocator* CreateAllocator(AllocatorTag tag);
	};
}