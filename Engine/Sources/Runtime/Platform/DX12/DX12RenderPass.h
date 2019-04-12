#pragma once

#include "Yes.h"
#include "Public/Misc/SharedObject.h"
#include "Public/Graphics/RenderDevice.h"
#include "Public/Misc/Utils.h"
#include "Platform/DX12/DX12FrameState.h"
#include "Platform/DX12/DX12RenderDeviceModule.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"
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
		void SetOutput(const TRef<RenderDeviceRenderTarget>& renderTarget, int idx) override;
		void SetClearColor(const V4F& clearColor, bool needed, int idx) override;
		void SetDepthStencil(const TRef<RenderDeviceDepthStencil>& depthStencil) override;
		void SetClearDepth(float depth, uint8 stencil, bool neededDepth, bool needStencil) override;
		void SetGlobalConstantBuffer(void* data, size_t size) override;
		TRef<RenderDeviceRenderTarget> GetBackbuffer() override;
		void Execute(DX12RenderPassContext& context);
		DX12FrameState* GetFrameState() { return mFrameState; }
	private:
	private:
		std::deque<RenderDeviceCommand*>   mCommands;
		TRef<IDX12RenderTarget>            mOutputTarget[8];
		V4F								   mClearColorValues[8];
		bool                               mNeedClearColor[8];
		TRef<DX12DepthStencil>             mDepthStencil;
		float                              mClearDepthValue;
		uint8                              mClearStencilValue;
		bool                               mNeedClearDepth;
		bool                               mNeedClearStencil;
		DX12FrameState*                    mFrameState;
		DX12GPUBufferRegion                mConstantBuffer;
		DX12RenderCommandPool*			   mCommandPool;
	};
}