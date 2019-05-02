#pragma once

#include "Yes.h"
#include "Public/Misc/SharedObject.h"
#include "Public/Graphics/RenderDevice.h"
#include "Public/Misc/Utils.h"
#include "Platform/DX12/DX12FrameState.h"
#include "Platform/DX12/DX12RenderDeviceModule.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"
#include <vector>
#include <deque>

namespace Yes
{
	class DX12FrameState;
	struct DX12RenderPassContext;
	class RenderDeviceCommand;
	class DX12RenderCommandPool;

	class DX12Pass : public RenderDevicePass
	{
	public:
		void Init(DX12FrameState* state, DX12RenderCommandPool* pool);
		void Reset() override;
		RenderDeviceCommand* AddCommand(RenderCommandType type) override;
		void SetOutput(size_t slot, const RenderDeviceTextureRef& renderTarget, bool needClear, const V4F& clearColor, const B3F& viewPort) override;
		void SetDepthStencil(const RenderDeviceTextureRef& depthStencil, bool clearDepth, float depth, bool clearStencil, uint8 stencil, bool setViewport, const B3F* viewport) override;
		void SetGlobalTexture(int idx, const RenderDeviceTextureRef& texture) override;
		void SetGlobalConstantBuffer(void* data, size_t size) override;

		RenderDeviceTextureRef GetBackbuffer()  override;
		void Execute(DX12RenderPassContext& context);
		DX12FrameState* GetFrameState() { return mFrameState; }
		size_t GetDescriptorHeapSize() { return mDescritptorHeapSize; }
		void CollectDescriptorHeapSize();
		D3D12_GPU_DESCRIPTOR_HANDLE GetGlobalConstantBufferGPUAddress();
		void Prepare(DX12RenderPassContext* ctx);
	private:
		std::deque<RenderDeviceCommand*>        mCommands;
		//rendertarget
		TRef<DX12Texture2D>				        mOutputTarget[8];
		V4F								        mClearColorValues[8];
		bool                                    mNeedClearColor[8];
		D3D12_VIEWPORT							mViewport[8];
		D3D12_RECT								mScissor[8];
		//depth stencil
		TRef<DX12Texture2D>				        mDepthStencil;
		float                                   mClearDepthValue;
		uint8                                   mClearStencilValue;
		bool                                    mNeedClearDepth;
		bool                                    mNeedClearStencil;
		bool									mUseDepthOnlyViewport;
		D3D12_VIEWPORT							mDepthOnlyViewport;
		D3D12_RECT								mDepthOnlyScissor;

		DX12FrameState*                         mFrameState;
		DX12GPUBufferRegion                     mConstantBuffer;
		std::vector<TRef<DX12Texture2D>>		mTextures;
		DX12RenderCommandPool*			        mCommandPool;
		size_t							        mDescritptorHeapSize;
	};
}