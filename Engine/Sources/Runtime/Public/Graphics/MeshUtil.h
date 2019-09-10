#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/Math.h"
#include "Runtime/Public/Misc/SharedObject.h"

namespace Yes
{
	void CreateUnitSphereMesh(SharedBufferRef& vb, SharedBufferRef& ib, int logitudeCount, int latitudeCount);
	//void CreateUnitBoxMesh(SharedBufferRef& vb, SharedBufferRef& ib);
}