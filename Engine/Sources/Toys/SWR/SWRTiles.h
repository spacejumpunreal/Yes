#pragma once
#include "Runtime/Public/Yes.h"
#include "Toys/SWR/Public/SWR.h"

namespace Yes::SWR
{
	struct SWRPipelineState;
	class SWRJobSystem;

	struct BinnedTriangles
	{
		TRef<ISharedBuffer> IntermediateBuffer;
		TRef<SWRPipelineState> PipelineState;
		TRef<ISharedBuffer> TriangleIndexes;
		int32 Stride;
		BinnedTriangles(TRef<ISharedBuffer>& imb, TRef<SWRPipelineState>& ps, ISharedBuffer* idx, int32 stride)
			: IntermediateBuffer(imb)
			, PipelineState(ps)
			, TriangleIndexes(idx)
			, Stride(stride)
		{}
	};

	class SWRTileBuffer
	{
	public:
		virtual void BinTriangles(TRef<ISharedBuffer>& imb, TRef<SWRPipelineState>& ps, size_t stride) = 0;
		virtual ~SWRTileBuffer() {};
	};
	SWRTileBuffer* CreateSWRTileBuffer(const DeviceDesc* desc, SWRJobSystem* jobSystem);
}