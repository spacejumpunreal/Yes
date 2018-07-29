#pragma once
#include "Yes.h"
#include "SWR.h"

namespace Yes::SWR
{
	class SWRTextureSampler : public SharedObject
	{
		virtual void* At(float x, float y) = 0;
		virtual void* At(int32 ix, int32 iy) = 0;
	};
}