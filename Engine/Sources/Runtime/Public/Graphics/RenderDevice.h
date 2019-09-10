#pragma once

#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/SharedObject.h"
#include "Runtime/Public/Misc/Math.h"
#include "Runtime/Public/Graphics/ImageUtil.h"
#include <vector>

namespace Yes
{
	//forward declarations
	struct RenderDeviceDrawcall;
	class RenderDeviceTexture;
	class RenderDevice;
	class RenderDeviceDescriptorHeap;

	//enums
	enum class RenderDeviceResourceState
	{
		STATE_COMMON,
		RENDER_TARGET,
		DEPTH_WRITE,
		SHADER_RESOURCE,
		UNORDERED_ACCESS,
	};
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
	enum class TextureUsage
	{
		ShaderResource,
		RenderTarget,
		DepthStencil,
	};
#define DefineLabel(label) label
	enum class RenderCommandType
	{
#include "Runtime/Graphics/RenderCommandTypeList.inl"
		RenderCommandTypeCount,
	};
#undef DefineLabel

	static const int MaxRenderTargets = 8;
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
	//pso & shader
	class RenderDeviceShader : public SharedObject
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
	class RenderDevicePSO : public SharedObject
	{
	};
	using RenderDevicePSORef = TRef<RenderDevicePSO>;
	//drawcall resources
	class RenderDeviceDrawcallArgument : public SharedObject
	{
	public:
		virtual void Apply(size_t slot, void* ctx) = 0;
	};
	class RenderDeviceConstantBuffer : public RenderDeviceDrawcallArgument
	{
	public:
		virtual void Write(size_t offset, size_t size, void * content) = 0;
		virtual size_t GetSize() = 0;
	};
	class RenderDeviceDescriptorHeapRange : public RenderDeviceDrawcallArgument
	{
	public:
		virtual void SetRange(size_t offset, size_t length, const RenderDeviceTexture* textures[]) = 0;
		virtual void Apply(size_t slot, void* ctx) = 0;
		virtual size_t GetSize() = 0;
		virtual TRef<RenderDeviceDescriptorHeap> GetDescriptorHeap() = 0;
	};
	class RenderDeviceDescriptorHeap : public SharedObject
	{
	public:
		virtual TRef<RenderDeviceDescriptorHeapRange> CreateRange(size_t offset, size_t length) = 0;
		virtual size_t GetSize() = 0;
	};
	//heap resources
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
	//commands
	class RenderDeviceCommand
	{
	public:
		virtual void Reset() = 0;
		virtual void Prepare(void* ctx) = 0;
		virtual void Execute(void* ctx) = 0;
		virtual RenderCommandType GetCommandType() = 0;
	};
	struct RenderDeviceDrawcall : public RenderDeviceCommand
	{
	public:
		virtual void SetPSO(RenderDevicePSO* pso) = 0;
		virtual void SetMesh(RenderDeviceMesh* mesh) = 0;
		virtual void SetArgument(int slot, RenderDeviceDrawcallArgument* arg) = 0;
		virtual void SetDescriptorHeap(RenderDeviceDescriptorHeap* descriptorHeap) = 0;
		RenderCommandType GetCommandType() { return RenderCommandType::Drawcall; }
	};
	struct RenderDeviceBarrier : public RenderDeviceCommand
	{
	public:
		virtual void AddResourceBarrier(const TRef<RenderDeviceResource>& resources, RenderDeviceResourceState newState) = 0;
		RenderCommandType GetCommandType() { return RenderCommandType::Barrier; }
	};
	//pass
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
		virtual void SetArgument(int slot, RenderDeviceDrawcallArgument* arg) = 0;
		virtual RenderDeviceTextureRef GetBackbuffer() = 0;
	protected:
		std::string mName;
	};
	class RenderDevice
	{
	public:
		//Resource related
		virtual TRef<RenderDeviceConstantBuffer> CreateConstantBuffer(bool isTemp, size_t size) = 0;
		virtual TRef<RenderDeviceDescriptorHeap> CreateDescriptorHeap(bool isTemp, size_t size) = 0;
		virtual RenderDeviceMeshRef CreateMeshSimple(SharedBufferRef& vertex, SharedBufferRef& index, size_t vertexStride, size_t indexCount, size_t indexStride) = 0;
		virtual RenderDeviceTextureRef CreateTexture2D(size_t width, size_t height, TextureFormat format, TextureUsage usage, RawImage* image) = 0;
		virtual RenderDevicePSORef CreatePSOSimple(RenderDevicePSODesc& desc) = 0;
		virtual RenderDeviceShaderRef CreateShaderSimple(SharedBufferRef& textBlob, const char* registeredName) = 0;

		//Command related
		virtual void BeginFrame() = 0;
		virtual void ExecutePass(RenderDevicePass* pass) = 0;
		virtual void EndFrame() = 0;

		//allocate related
		virtual RenderDevicePass* AllocPass(size_t argsCount) = 0;

		//query
		virtual V2F GetScreenSize() = 0;
		virtual int GetFrameIndex() = 0;
	};
}
