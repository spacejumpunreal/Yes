#pragma once

#include "Yes.h"
#include "Misc/SharedObject.h"

namespace Yes
{
	class RenderDeviceResource : public SharedObject
	{
	public:
	protected:
		virtual ~RenderDeviceResource()
		{}
		virtual bool IsReady() = 0; // check if is ready to use
	};
	using RenderDeviceResourceRef = TRef<RenderDeviceResource>;
	typedef enum class VertexFormat : byte
	{
		VF_P3F_T2F,
	} VertexFormat;
	enum class PSOStateKey : byte
	{
		Default,
	};
	enum class TextureFormat
	{
		R8G8B8A8_UNORM,
	};
	static const int MaxRenderTargets = 4;
	struct RenderDevicePSODesc
	{
		RenderDeviceResourceRef Shader;
		TextureFormat RTs[MaxRenderTargets];
		PSOStateKey StateKey;
		VertexFormat VF;
		RenderDevicePSODesc(VertexFormat vf, RenderDeviceResourceRef& shader, PSOStateKey stateKey, TextureFormat rts[]);
		RenderDevicePSODesc();
		friend bool operator==(const struct RenderDevicePSODesc& lhs, const struct RenderDevicePSODesc& rhs);
	};
	class RenderDevicePass
	{

	};
	class RenderDeviceShader : public RenderDeviceResource
	{

	};
	class RenderDevicePSO : public RenderDeviceResource
	{
	};
	class RenderDeviceCommand
	{
		virtual void Reset() = 0;
		virtual void Execute() = 0;
	};
	class RenderDeviceBarrier : public RenderDeviceCommand
	{};
	class  RenderDeviceDrawcall : public RenderDeviceCommand
	{
		RenderDeviceResourceRef Mesh;
		std::vector<RenderDeviceResourceRef> Textures;
		std::vector<RenderDeviceResourceRef> ConstantBuffers;
		RenderDeviceResourceRef PSO;
	};
	class RenderDevice
	{
	public:
		//Resource related
		virtual RenderDeviceResourceRef CreateMeshSimple(SharedBufferRef& meshBlob) = 0;
		virtual RenderDeviceResourceRef CreatePSOSimple(RenderDevicePSODesc& desc) = 0;
		virtual RenderDeviceResourceRef CreateShaderSimple(SharedBufferRef& textBlob, const char* registeredName) = 0;
		virtual RenderDeviceResourceRef CreateRenderTarget() = 0;
		virtual RenderDeviceResourceRef CreteTextureSimple() = 0;

		//Command related
		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;
		virtual RenderDevicePass* AllocPass() = 0;
		virtual RenderDeviceDrawcall* AllocDrawcall() = 0;
		virtual RenderDeviceBarrier* AllocBarrier() = 0;
	};
}
