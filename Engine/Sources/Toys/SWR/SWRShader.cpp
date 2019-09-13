#include "Runtime/Public/Yes.h"
#include "Toys/SWR/Public/SWR.h"
#include "Toys/SWR/SWRShader.h"
namespace Yes::SWR
{
	SWRHandle CreateSWRVertexShader(const VertexShaderDesc * desc)
	{
		return new SWRVertexShader(desc);
	}

	SWRHandle CreateSWRPixelShader(const PixelShaderDesc * desc)
	{
		return new SWRPixelShader(desc);
	}

	void SWRDefaultOutputMerger(void* current, void*, void* next, int numRT)
	{
		auto sz = GetPSOutputSize(numRT);
		memcpy(next, current, sz);
	}
}