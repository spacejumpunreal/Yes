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
		void SetOutput(TRef<RenderDeviceRenderTarget>& renderTarget, int idx) override;
		void SetClearColor(const V4F& clearColor, bool needed, int idx) override;
		void SetDepthStencil(TRef<RenderDeviceDepthStencil>& depthStencil) override;
		void SetClearDepth(float depth, uint8 stencil, bool neededDepth, bool needStencil, int idx) override;
		void SetGlobalConstantBuffer(void* data, size_t size) override;
		void Execute(DX12RenderPassContext& context);
		DX12FrameState* GetFrameState() { return mFrameState; }
	private:
	private:
		std::deque<RenderDeviceCommand*>   mCommands;
		TRef<IDX12RenderTarget>            mOutputTarget[8];
		V4F								   mClearColorValues[8];
		bool                               mNeedClearColor[8];
		TRef<DX12DepthStencil>             mDepthStencil;
		float                              mClearDepthValues[8];
		uint8                              mClearStencilValues[8];
		bool                               mNeedClearDepth[8];
		bool                               mNeedClearStencil[8];
		DX12FrameState*                    mFrameState;
		AllocatedCBV                       mConstantBuffer;
		DX12RenderCommandPool*			   mCommandPool;
	};
}