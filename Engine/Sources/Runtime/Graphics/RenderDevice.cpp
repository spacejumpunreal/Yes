#include "Runtime/Public/Graphics/RenderDevice.h"
#include "Runtime/Public/Misc/Utils.h"
#include <deque>

namespace Yes
{
	//RenderDeviceResource
	void RenderDeviceResource::SetState(RenderDeviceResourceState state)
	{
		CheckAlways(false, "not supposed to be called");
	}

	RenderDeviceResourceState RenderDeviceResource::GetState()
	{
		CheckAlways(false, "not supposed to be called");
		return RenderDeviceResourceState::STATE_COMMON;
	}
	void* RenderDeviceResource::GetTransitionTarget()
	{
		CheckAlways(false, "not supposed to be called");
		return nullptr;
	}

	//RenderDevicePSODesc
	RenderDevicePSODesc::RenderDevicePSODesc(VertexFormat vf, RenderDeviceShaderRef& shader, PSOStateKey stateKey, TextureFormat rts[], int rtCount)
		: Shader(shader)
		, StateKey(stateKey)
		, VF(vf)
		, RTCount(rtCount)
	{
		CheckAlways(rtCount < MaxRenderTargets);
		for (int i = 0; i < rtCount; ++i)
		{
			RTFormats[i] = rts[i];
		}
	}
	RenderDevicePSODesc::RenderDevicePSODesc()
		: RTCount(0)
	{
		memset(RTFormats, 0, sizeof(RTFormats));
	}
	bool operator==(const struct RenderDevicePSODesc& lhs, const struct RenderDevicePSODesc& rhs)
	{
		if (lhs.Shader != rhs.Shader || lhs.StateKey != rhs.StateKey || lhs.VF != rhs.VF || lhs.RTCount != rhs.RTCount)
			return false;
		for (int i = 0; i < MaxRenderTargets; ++i)
		{
			if (lhs.RTFormats[i] != rhs.RTFormats[i])
				return false;
		}
		return true;
	}
	B3F RenderDeviceTexture::GetDefaultViewport()
	{
		V3I sz = GetSize();  
		return B3F::Init2Points(V3F(0, 0, 0), V3F((float)sz.x, (float)sz.y, 1.0f));
	}
}