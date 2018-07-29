#pragma once

namespace Yes::SWR
{
	template<size_t NumRT>
	struct SWRPSOutput
	{
		float Depth;
		float Color[NumRT][4];
	};
	constexpr size_t GetPSOutputSize(size_t numRT)
	{
		return (sizeof(SWRPSOutput<2>) - sizeof(SWRPSOutput<1>) * (numRT - 1)) + sizeof(SWRPSOutput<1>);
	}

	enum class PixelFormat;
	typedef void(*VertexShaderSignature)(void* vsInput, void* vsOutput);
	typedef bool(*PixelShaderSignature)(void* psInput, void* psOutput);
	typedef void(*OutputMergerSignature)(void* current, void* previous, void* next, int numRT);

	struct VertexShaderDesc
	{
		VertexShaderSignature Function;
		size_t InputStride;
		size_t OutputStride;
	};
	struct PixelShaderDesc
	{
		PixelShaderSignature Function;
	};

	void SWRDefaultOutputMerger(void* current, void* previous, void* next, int numRT);
}