#pragma once

#include "Yes.h"
#include "Misc/SharedObject.h"
#include "Misc/Math.h"
#include "Graphics/ImageUtil.h"
#include <vector>

namespace Yes
{
	struct RenderDeviceDrawcall;

	enum class RenderDeviceResourceState
	{
		STATE_COMMON,
		RENDER_TARGET,
		DEPTH_WRITE,
		SHADER_RESOURCE,
		UNORDERED_ACCESS,
	};
	class RenderDeviceResource : public SharedObject
	{
	public:
		virtual ~RenderDeviceResource()
		{}
		virtual void SetName(wchar_t* name) {}
		virtual bool IsReady() = 0; // check if is ready to use
		virtual void SetState(RenderDeviceResourceState state);
		virtual RenderDeviceResourceState GetState();
		virtual void* GetTransitionTarget();
	};
	using RenderDeviceResourceRef = TRef<RenderDeviceResource>;
	typedef enum class VertexFormat : byte
	{
		VF_P3F_T2F,
	} VertexFormat;
	enum class PSOStateKey : byte
	{
		Default,
		Normal,
	};
	enum class TextureFormat
	{
		R8G8B8A8_UNORM,
		D24_UNORM_S8_UINT,
		D32_UNORM_S8_UINT,
	};

#define DefineLabel(label) label

	enum class RenderCommandType
	{
#include "Graphics/RenderCommandTypeList.inl"
		RenderCommandTypeCount,
	};
#undef DefineLabel
	static const int MaxRenderTargets = 8;
	class RenderDeviceShader : public RenderDeviceResource
	{
	};
	using RenderDeviceShaderRef = TRef<RenderDeviceShader>;
	struct RenderDevicePSODesc
	{
		RenderDeviceShaderRef Shader;
		TextureFormat RTFormats[MaxRenderTargets];
		TextureFormat DSFormat;

		PSOStateKey StateKey;
		VertexFormat VF;
		int RTCount;
		RenderDevicePSODesc(VertexFormat vf, RenderDeviceShaderRef& shader, PSOStateKey stateKey, TextureFormat rts[], int rtCount);
		RenderDevicePSODesc();
		friend bool operator==(const struct RenderDevicePSODesc& lhs, const struct RenderDevicePSODesc& rhs);
	};
	class RenderDeviceConstantBuffer : public RenderDeviceResource
	{
	};
	using RenderDeviceConstantBufferRef = TRef<RenderDeviceConstantBuffer>;
	class RenderDevicePSO : public RenderDeviceResource
	{
	};
	using RenderDevicePSORef = TRef<RenderDevicePSO>;
	class RenderDeviceMesh : public RenderDeviceResource
	{};
	using RenderDeviceMeshRef = TRef<RenderDeviceMesh>;
	class RenderDeviceTexture : public RenderDeviceResource
	{
	public:
		virtual V3I GetSize() = 0;
		B3F GetDefaultViewport();
	};
	using RenderDeviceTextureRef = TRef<RenderDeviceTexture>;
	class RenderDeviceCommand
	{
	public:
		virtual void Reset() = 0;
		virtual void Prepare(void* ctx) = 0;
		virtual void Execute(void* ctx) = 0;
		virtual size_t GetDescriptorHeapSlotCount() = 0;
		virtual RenderCommandType GetCommandType() = 0;
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
		virtual void SetOutput(size_t slot, const RenderDeviceTextureRef& renderTarget, bool needClear, const V4F& clearColor, const B3F& viewPort) = 0;
		virtual void SetDepthStencil(const RenderDeviceTextureRef& depthStencil, bool clearDepth, float depth, bool clearStencil, uint8 stencil, bool setViewport, const B3F* viewport) = 0;
		virtual void SetGlobalTexture(int idx, const RenderDeviceTextureRef& texture) = 0;
		virtual void SetGlobalConstantBuffer(void* data, size_t size) = 0;
		virtual RenderDeviceTextureRef GetBackbuffer() = 0;
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
		RenderCommandType GetCommandType() { return RenderCommandType::Drawcall; }
	};
	struct RenderDeviceBarrier : public RenderDeviceCommand
	{
	public:
		virtual void AddResourceBarrier(const TRef<RenderDeviceResource>& resources, RenderDeviceResourceState newState) = 0;
		RenderCommandType GetCommandType() { return RenderCommandType::Barrier; }
	};
	enum class TextureUsage
	{
		ShaderResource,
		RenderTarget,
		DepthStencil,
	};
	class RenderDevice
	{
	public:
		//Resource related
		virtual RenderDeviceConstantBufferRef CreateConstantBufferSimple(size_t size) = 0;
		virtual RenderDeviceMeshRef CreateMeshSimple(SharedBufferRef& vertex, SharedBufferRef& index, size_t vertexStride, size_t indexCount, size_t indexStride) = 0;
		virtual RenderDeviceTextureRef CreateTexture2D(size_t width, size_t height, TextureFormat format, TextureUsage usage, RawImage* image) = 0;
		virtual RenderDevicePSORef CreatePSOSimple(RenderDevicePSODesc& desc) = 0;
		virtual RenderDeviceShaderRef CreateShaderSimple(SharedBufferRef& textBlob, const char* registeredName) = 0;

		//Command related
		virtual void BeginFrame() = 0;
		virtual void ExecutePass(RenderDevicePass* pass) = 0;
		virtual void EndFrame() = 0;

		//allocate related
		virtual RenderDevicePass* AllocPass() = 0;

		//query
		virtual V2F GetScreenSize() = 0;
		virtual int GetFrameIndex() = 0;
	};
}
