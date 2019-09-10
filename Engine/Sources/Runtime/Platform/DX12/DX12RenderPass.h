#pragma once

#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/SharedObject.h"
#include "Runtime/Public/Graphics/RenderDevice.h"
#include "Runtime/Public/Misc/Utils.h"
#include "Runtime/Platform/DX12/DX12FrameState.h"
#include "Runtime/Public/Platform/DX12/DX12RenderDeviceModule.h"
#include "Runtime/Platform/DX12/DX12RenderDeviceResources.h"
#include <vector>
#include <deque>

namespace Yes
{
	class DX12FrameState;
	struct DX12RenderPassContext;
	class RenderDeviceCommand;
	class DX12RenderCommandPool;

	class DX12RenderPass : public RenderDevicePass
	{
	public:
		void Init(DX12FrameState* state, DX12RenderCommandPool* pool, size_t argsCount);
		void Reset() override;
		RenderDeviceCommand* AddCommand(RenderCommandType type) override;
		void SetOutput(size_t slot, const RenderDeviceTextureRef& renderTarget, bool needClear, const V4F& clearColor, const B3F& viewPort) override;
		void SetDepthStencil(const RenderDeviceTextureRef& depthStencil, bool clearDepth, float depth, bool clearStencil, uint8 stencil, bool setViewport, const B3F* viewport) override;
		void SetArgument(int slot, RenderDeviceDrawcallArgument* arg) override;

		RenderDeviceTextureRef GetBackbuffer() override;
		void Execute(DX12RenderPassContext& context);
		DX12FrameState* GetFrameState() { return mFrameState; }
		void PreparePassArgs(DX12RenderPassContext* ctx);
		size_t GetPassArgCount() { return mArgs.size(); }
	private:
		std::deque<RenderDeviceCommand*>				mCommands;
		//rendertarget
		TRef<DX12Texture2D>				                mOutputTarget[8];
		V4F								                mClearColorValues[8];
		bool                                            mNeedClearColor[8];
		D3D12_VIEWPORT							        mViewport[8];
		D3D12_RECT								        mScissor[8];
		//depth stencil
		TRef<DX12Texture2D>				                mDepthStencil;
		float                                           mClearDepthValue;
		uint8                                           mClearStencilValue;
		bool                                            mNeedClearDepth;
		bool                                            mNeedClearStencil;
		bool									        mUseDepthOnlyViewport;
		D3D12_VIEWPORT							        mDepthOnlyViewport;
		D3D12_RECT								        mDepthOnlyScissor;

		DX12FrameState*                                 mFrameState;
		std::vector<TRef<RenderDeviceDrawcallArgument>> mArgs;
		DX12RenderCommandPool*			                mCommandPool;
	};
}