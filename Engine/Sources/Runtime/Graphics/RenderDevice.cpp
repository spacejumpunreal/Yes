#include "Graphics/RenderDevice.h"
#include "Misc/Utils.h"
#include <deque>

namespace Yes
{
	RenderDevicePSODesc::RenderDevicePSODesc(VertexFormat vf, RenderDeviceResourceRef& shader, PSOStateKey stateKey, TextureFormat rts[])
		: Shader(shader)
		, StateKey(stateKey)
		, VF(vf)
	{
		memcpy(RTs, rts, sizeof(RTs));
	}
	RenderDevicePSODesc::RenderDevicePSODesc()
	{
		memset(RTs, 0, sizeof(RTs));
	}
	bool operator==(const struct RenderDevicePSODesc& lhs, const struct RenderDevicePSODesc& rhs)
	{
		if (lhs.Shader != rhs.Shader)
			return false;
		for (int i = 0; i < MaxRenderTargets; ++i)
		{
			if (lhs.RTs[i] != rhs.RTs[i])
				return false;
		}
		if (lhs.StateKey != rhs.StateKey || lhs.VF != rhs.VF)
			return false;
		return true;
	}
}