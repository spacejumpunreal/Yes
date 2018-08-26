#pragma once
#include "Yes.h"
#include "SWR.h"

namespace Yes::SWR
{
	class SWRJobSystem;

	class SWRRasterizer
	{
	public:
		
	};

	SWRRasterizer* CreateSWRRasterizer(const DeviceDesc* desc, SWRJobSystem* jobSystem);
}
