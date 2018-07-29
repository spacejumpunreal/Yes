#pragma once
#include "Yes.h"
#include "SWR.h"

namespace Yes::SWR
{
	struct SWRVertexShader : public SharedObject
	{
	public:
		VertexShaderSignature Function;
		size_t InputStride;
		size_t OutputStride;
	public:
		SWRVertexShader(const VertexShaderDesc * desc)
			: Function(desc->Function)
			, InputStride(desc->InputStride)
			, OutputStride(desc->OutputStride)
		{}
	};
	SWRHandle CreateSWRVertexShader(const VertexShaderDesc* desc);

	struct SWRPixelShader : public SharedObject
	{
	public:
		PixelShaderSignature Function;
		size_t InputStride;
		size_t OutputStride;
	public:
		SWRPixelShader(const PixelShaderDesc * desc)
			: Function(desc->Function)
		{}
	};
	SWRHandle CreateSWRPixelShader(const PixelShaderDesc* desc);
}
