#pragma once

#include "Yes.h"
#include "SWR.h"

namespace Yes::SWR
{
	struct SWRPipelineState : public SharedObject
	{
		SWRPipelineState()
			: VSShader(nullptr)
			, PSShader(nullptr)
			, OMShader(nullptr)
		{
		}
		SWRPipelineState(SWRPipelineState& o)
			: VertexBuffer(o.VertexBuffer)
			, VSShader(o.VSShader)
			, PSShader(o.PSShader)
			, OMShader(o.OMShader)
			, OMDepthStencil(o.OMDepthStencil)
		{
			for (int i = 0; i < MaxSlots; ++i)
			{
				VSTextures[i] = o.VSTextures[i];
				VSConstants[i] = o.VSConstants[i];
				PSTextures[i] = o.PSTextures[i];
				PSConstants[i] = o.PSConstants[i];
				OMRenderTargets[i] = o.OMRenderTargets[i];
				OMDepthStencil = o.OMDepthStencil;
			}
		}
	public:
		//IA
		TRef<ISharedBuffer> VertexBuffer;
		//VS
		SWRHandle VSShader;
		TRef<SWRTextureSampler> VSTextures[MaxSlots];
		TRef<ISharedBuffer> VSConstants[MaxSlots];
		//RS
		//PS
		SWRHandle PSShader;
		TRef<SWRTextureSampler> PSTextures[MaxSlots];
		TRef<ISharedBuffer> PSConstants[MaxSlots];
		//OM
		OutputMergerSignature OMShader;
		TRef<SWRTextureSampler> OMRenderTargets[MaxSlots];
		TRef<SWRTextureSampler> OMDepthStencil;
		//Misc
	};
}