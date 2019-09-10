#pragma once
#include "Runtime/Public/Yes.h"
#include "Toys/SWR/Public/SWR.h"

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