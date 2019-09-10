#pragma once
#include "Runtime/Public/Yes.h"
#include "Toys/SWR/Public/SWR.h"

namespace Yes::SWR
{
	class SWRTextureSampler : public SharedObject
	{
		virtual void* At(float x, float y) = 0;
		virtual void* At(int32 ix, int32 iy) = 0;
	};
}