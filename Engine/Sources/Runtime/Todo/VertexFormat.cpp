#include "VertexFormat.h"
#include "Debug.h"

/*
need:
1. enum-> size, declaration
2. easy registration
3. typename -> enum
*/
namespace Demo18
{
	static int GVFMax = 0;
	static const size_t MAX_VF_INDEX = 31;

	static VertexFormatInfo GVFInfos[MAX_VF_INDEX + 1];

	void _RegisterVertexFormat(int idx, int size, const DeviceVertexInfo* info)
	{
		Check(idx <= MAX_VF_INDEX, "too many vertex format");
		GVFMax = idx > GVFMax ? idx : GVFMax;
		Check(GVFInfos[idx].DeviceDetail == nullptr, "should not re-register a VF");
		GVFInfos[idx].Enum = idx;
		GVFInfos[idx].VertexSize = size;
		GVFInfos[idx].DeviceDetail = info;
	}
	const VertexFormatInfo* GetVertexFormatInfo(int e)
	{
		if (e > GVFMax || e < 0 || GVFInfos[e].DeviceDetail == nullptr)
			return nullptr;
		return &GVFInfos[e];
	}
}