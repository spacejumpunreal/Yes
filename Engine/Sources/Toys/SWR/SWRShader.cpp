#include "Yes.h"
#include "SWR.h"
#include "SWRShader.h"
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

	void SWRDefaultOutputMerger(void* current, void* previous, void* next, int numRT)
	{
		auto sz = GetPSOutputSize(numRT);
		memcpy(next, current, sz);
	}
}