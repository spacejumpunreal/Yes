#pragma once
#include "Demo18.h"

#define VERTEX_FORMAT_BEGIN(name, idx)\
	struct name\
	{\
		static const uint32 Enum = idx;

#define VERTEX_FORMAT_END };

namespace Demo18
{
	VERTEX_FORMAT_BEGIN(VF_P3F_N3F_T2F, 0)
		float Position[3];
		float Normal[3];
		float Texcoord[2];
	VERTEX_FORMAT_END

	VERTEX_FORMAT_BEGIN(VF_P3F_T2F, 1)
		float Position[3];
		float Texcoord[2];
	VERTEX_FORMAT_END

#undef VERTEX_FORMAT_BEGIN
#undef VERTEX_FORMAT_END

	struct DeviceVertexInfo;
	struct VertexFormatInfo
	{
		int32 Enum;
		int VertexSize;
		const DeviceVertexInfo* DeviceDetail;
		VertexFormatInfo()
			: Enum(-1)
			, VertexSize(-1)
			, DeviceDetail(nullptr)
		{}
	};
	template<typename RVF>
	void RegisterVertexFormat(const DeviceVertexInfo* info)
	{
		extern void _RegisterVertexFormat(int idx, int size, const DeviceVertexInfo* info);
		_RegisterVertexFormat(RVF::Enum, sizeof(RVF), info);
	}
	const VertexFormatInfo* GetVertexFormatInfo(int e);
}


