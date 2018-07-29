#pragma once
#include "Yes.h"
#include "Misc/SharedObject.h"
#include "SWRSampler.h"
#include "SWRShaderUtils.h"

namespace Yes::SWR
{
	//configurations
	const size_t MaxSlots = 4;
	using SWRHandle = TRef<SharedObject>;

	enum class PixelFormat
	{
		Color1F,
		Color4F,
		PixelFormatCount,
	};

	struct BufferDesc
	{
		size_t Size;
		const void* InitialData;
	};

	struct Texture2DDesc
	{
		size_t Width;
		size_t Height;
		PixelFormat PixelFormat;
		const void* InitialData;
	};
	struct RasterizationStateDesc
	{
	};
	struct OutputMergerStateDesc
	{
		OutputMergerSignature OutputMergerFucntion;
	};

	class SWRDevice
	{
	public:
		//0 common
		virtual SWRHandle CreateBuffer(const BufferDesc* desc) = 0;
		virtual SWRHandle CreateTexture2D(const Texture2DDesc* desc) = 0;
		virtual SWRHandle CreateRenderTarget(const Texture2DDesc* desc) = 0;
		virtual SWRHandle CreateDepthStencil(const Texture2DDesc* desc) = 0;
		virtual SWRHandle CreateVertexShader(const VertexShaderDesc* desc) = 0;
		virtual SWRHandle CreatePixelShader(const PixelShaderDesc* desc) = 0;
		//1 IA
		virtual void IASetVertexBuffer(SWRHandle vb) = 0;
		//2 VS
		virtual void VSSetConstant(size_t slot, SWRHandle cb) = 0;
		virtual void VSSetTexture(size_t slot, SWRHandle tex) = 0;
		virtual void VSSetShader(SWRHandle vs) = 0;
		//3 RS
		virtual void RSSetRasterizationState(SWRHandle rsstate) = 0;
		//4 PS
		virtual void PSSetConstant(size_t slot, SWRHandle cb) = 0;
		virtual void PSSetTexture(size_t slot, SWRHandle tex) = 0;
		virtual void PSSetShader(SWRHandle ps) = 0;
		//5 OM
		virtual void OMSetRenderTarget(size_t slot, SWRHandle rt) = 0;
		virtual void OMSetDepthStencil(SWRHandle ds) = 0;
		virtual void OMSetOutputMergerState(OutputMergerSignature omstate) = 0;
		//2. commands
		virtual void Clear(float clearColor[4], float depth) = 0;
		virtual void Draw() = 0;
		virtual void Present() = 0;
		
		//-1. misc
		virtual void Test(int x) = 0;
	protected:
		SWRDevice() = default;
	};
	struct DeviceDesc
	{
		size_t BackBufferWidth;
		size_t BackBufferHeight;
		size_t NumThreads;
	};
	SWRDevice* CreateSWRDevice(const DeviceDesc* desc);
}