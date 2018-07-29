#pragma once
#include "Yes.h"
#include "SWR.h"

namespace Yes::SWR
{
	struct SWRPipelineState;
	struct DeviceCore;
	class SWRFrontEndJob
	{
	public:
		SWRFrontEndJob(TRef<SWRPipelineState>& pso, DeviceCore* deviceCore)
			: mState(pso)
			, mDeviceCore(deviceCore)
		{}
		void operator()();
	private:
		TRef<SWRPipelineState> mState;
		DeviceCore* mDeviceCore;
	};
}