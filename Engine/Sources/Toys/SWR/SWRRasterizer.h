#pragma once
#include "Runtime/Public/Yes.h"
#include "Toys/SWR/Public/SWR.h"

namespace Yes::SWR
{
	class SWRJobSystem;

	class SWRRasterizer
	{
	public:
		
	};

	SWRRasterizer* CreateSWRRasterizer(const DeviceDesc* desc, SWRJobSystem* jobSystem);
}
