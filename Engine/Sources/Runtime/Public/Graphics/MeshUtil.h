#pragma once
#include "Yes.h"
#include "Misc/Math.h"
#include "Misc/SharedObject.h"

namespace Yes
{
	void CreateUnitSphereMesh(SharedBufferRef& vb, SharedBufferRef& ib, int logitudeCount, int latitudeCount);
	//void CreateUnitBoxMesh(SharedBufferRef& vb, SharedBufferRef& ib);
}