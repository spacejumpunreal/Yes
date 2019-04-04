#pragma once

#include "Yes.h"
#include "Misc/SharedObject.h"
#include "Misc/Math.h"
#include <vector>

namespace Yes
{
	struct RenderDeviceDrawcall;

	class RenderDeviceResource : public SharedObject
	{
	public:
		virtual ~RenderDeviceResource()
		{}
		virtual bool IsReady() = 0; // check if is ready to use
	protected:

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
		R24_UNORM_X8_TYPELESS,
	};

#define DefineLabel(label) label

	enum class RenderCommandType
	{
#include "Graphics/RenderCommandTypeList.inl"
		RenderCommandTypeCount,
	};
#undef DefineLabel
	static const int MaxRenderTargets = 8;
	struct RenderDevicePSODesc
	{
		RenderDeviceResourceRef Shader;
		TextureFormat RTs[MaxRenderTargets];

		PSOStateKey StateKey;
		VertexFormat VF;
		int RTCount;
		RenderDevicePSODesc(VertexFormat vf, RenderDeviceResourceRef& shader, PSOStateKey stateKey, TextureFormat rts[], int rtCount);
		RenderDevicePSODesc();
		friend bool operator==(const struct RenderDevicePSODesc& lhs, const struct RenderDevicePSODesc& rhs);
	};
	struct RenderDeviceViewPortDesc
	{
		float TopLeftX;
		float TopLeftY;
		float Width;
		float Height;
		float MinDepth;
		float MaxDepth;
	};
	struct RenderDeviceScissorDesc
	{
		int Left;
		int Top;
		int Right;
		int Bottom;
	};
	class RenderDeviceShader : public RenderDeviceResource
	{
	};
	class RenderDeviceConstantBuffer : public RenderDeviceResource
	{
	};
	class RenderDevicePSO : public RenderDeviceResource
	{
	};
	class RenderDeviceMesh : public RenderDeviceResource
	{};
	class RenderDeviceTexture : public RenderDeviceResource
	{};
	class RenderDeviceRenderTarget : public RenderDeviceResource
	{};
	class RenderDeviceDepthStencil : public RenderDeviceResource
	{};
	class RenderDeviceCommand
	{
	public:
		virtual void Reset() = 0;
		virtual void Prepare(void* ctx) = 0;
		virtual void Execute(void* ctx) = 0;
	public:
		RenderCommandType CommandType;
	};
	class RenderDevicePass
	{
	public:
		void SetName(const char* name)
		{
			mName = name;
		}
		virtual void Reset() = 0;
		virtual RenderDeviceCommand* AddCommand(RenderCommandType cmd) = 0;
		virtual void SetOutput(TRef<RenderDeviceRenderTarget>& renderTarget, int idx) = 0;
		virtual void SetClearColor(const V4F& clearColor, bool needed, int idx) = 0;
		virtual void SetDepthStencil(TRef<RenderDeviceDepthStencil>& depthStencil) = 0;
		virtual void SetClearDepth(float depth, uint8 stencil, bool neededDepth, bool needStencil, int idx) = 0;
		virtual void SetGlobalConstantBuffer(void* data, size_t size) = 0;
	protected:
		std::string mName;
	};
	struct RenderDeviceDrawcall : public RenderDeviceCommand
	{
	public:
		virtual void SetMesh(RenderDeviceMesh* mesh) = 0;
		virtual void SetTextures(int idx, RenderDeviceTexture* texture) = 0;
		virtual void SetConstantBuffer(void* data, size_t size, RenderDevicePass* pass) = 0;
		virtual void SetPSO(RenderDevicePSO* pso) = 0;
	};
	class RenderDevice
	{
	public:
		//Resource related
		virtual RenderDeviceResourceRef CreateConstantBufferSimple(size_t size) = 0;
		virtual RenderDeviceResourceRef CreateMeshSimple(SharedBufferRef& vertex, SharedBufferRef& index, size_t vertexStride, size_t indexCount, size_t indexStride) = 0;
		virtual RenderDeviceResourceRef CreatePSOSimple(RenderDevicePSODesc& desc) = 0;
		virtual RenderDeviceResourceRef CreateShaderSimple(SharedBufferRef& textBlob, const char* registeredName) = 0;
		virtual RenderDeviceResourceRef CreateRenderTarget() = 0;
		virtual RenderDeviceResourceRef CreteTextureSimple() = 0;

		//Command related
		virtual void BeginFrame() = 0;
		virtual void ExecutePass(RenderDevicePass* pass) = 0;
		virtual void EndFrame() = 0;

		//allocate related
		virtual RenderDevicePass* AllocPass() = 0;
	};
}
