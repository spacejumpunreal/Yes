#include "Graphics/RenderDevice.h"
#include "Misc/Utils.h"
#include <deque>

namespace Yes
{
	RenderDevicePSODesc::RenderDevicePSODesc(VertexFormat vf, RenderDeviceResourceRef& shader, PSOStateKey stateKey, TextureFormat rts[], int rtCount)
		: Shader(shader)
		, StateKey(stateKey)
		, VF(vf)
		, RTCount(rtCount)
	{
		CheckAlways(rtCount < MaxRenderTargets);
		for (int i = 0; i < rtCount; ++i)
		{
			RTs[i] = rts[i];
		}
	}
	RenderDevicePSODesc::RenderDevicePSODesc()
		: RTCount(0)
	{
		memset(RTs, 0, sizeof(RTs));
	}
	bool operator==(const struct RenderDevicePSODesc& lhs, const struct RenderDevicePSODesc& rhs)
	{
		if (lhs.Shader != rhs.Shader || lhs.StateKey != rhs.StateKey || lhs.VF != rhs.VF || lhs.RTCount != rhs.RTCount)
			return false;
		for (int i = 0; i < MaxRenderTargets; ++i)
		{
			if (lhs.RTs[i] != rhs.RTs[i])
				return false;
		}
		return true;
	}
}