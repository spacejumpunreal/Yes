#include "Runtime/Platform/DX12/DX12RenderPass.h"
#include "Runtime/Platform/DX12/DX12RenderDeviceResources.h"
#include "Runtime/Platform/DX12/DX12FrameState.h"
#include "Runtime/Platform/DX12/DX12ExecuteContext.h"
#include "Runtime/Platform/DX12/DX12RenderCommand.h"
#include "Runtime/Public/Memory/SizeUtils.h"
#include "Runtime/Public/Misc/Debug.h"

namespace Yes
{
	//DX12RenderDevicePass
	void DX12RenderPass::Init(DX12FrameState* state, DX12RenderCommandPool* pool, size_t argsCount)
	{
		mFrameState = state;
		mCommandPool = pool;
		mArgs.resize(argsCount);
	}
	void DX12RenderPass::Reset()
	{
		mName.clear();
		CheckAlways(mCommands.empty());
		mCommands.clear();
		for (int i = 0; i < MaxRenderTargets; ++i)
		{
			mOutputTarget[i] = nullptr;
			mClearColorValues[i] = V4F{};
			mNeedClearColor[i] = true;
			mViewport[i] = {};
			mScissor[i] = {};
		}
		mDepthStencil = nullptr;
		mClearDepthValue = 1.0f;
		mClearStencilValue = 0;
		mNeedClearDepth = true;
		mNeedClearStencil = true;
		mUseDepthOnlyViewport = false;
		mFrameState = nullptr;
		mArgs.clear();
		mCommandPool = nullptr;
	}
	RenderDeviceCommand* DX12RenderPass::AddCommand(RenderCommandType type)
	{
		RenderDeviceCommand* cmd = mCommandPool->AllocCommand(type);
		mCommands.push_back(cmd);
		return cmd;
	}
	void GetDeviceViewport(const V3I& rtSize, const B3F viewport, D3D12_VIEWPORT& deviceViewport, D3D12_RECT& scissor)
	{
		deviceViewport = CD3DX12_VIEWPORT(
			viewport.Min.x, (float)(rtSize.y) - (viewport.Max.y),
			viewport.Max.x - viewport.Min.x, viewport.Max.y - viewport.Min.y,
			viewport.Min.z, viewport.Max.z);
		scissor.left = (LONG)viewport.Min.x;
		scissor.right = (LONG)(viewport.Max.x);
		scissor.top = (LONG)(rtSize.y - viewport.Max.y);
		scissor.bottom = (LONG)(rtSize.y - viewport.Min.y);
	}
	void DX12RenderPass::SetOutput(
		size_t slot, const RenderDeviceTextureRef& renderTarget, 
		bool needClear, const V4F& clearColor, 
		const B3F& viewport)
	{
		DX12Texture2D* rt = dynamic_cast<DX12Texture2D*>(renderTarget.GetPtr());
		mOutputTarget[slot] = rt;
		mClearColorValues[slot] = clearColor;
		mNeedClearColor[slot] = needClear;
		V3I rtSize = rt->GetSize();
		GetDeviceViewport(rtSize, viewport, mViewport[slot], mScissor[slot]);
	}
	void DX12RenderPass::SetDepthStencil(const RenderDeviceTextureRef& depthStencil, bool clearDepth, float depth, bool clearStencil, uint8 stencil, bool setViewport, const B3F* viewport)
	{
		mDepthStencil = dynamic_cast<DX12Texture2D*>(depthStencil.GetPtr());
		mNeedClearDepth = clearDepth;
		mClearDepthValue = depth;
		mNeedClearStencil = clearStencil;
		mClearStencilValue = stencil;
		mUseDepthOnlyViewport = setViewport;
		if (setViewport)
		{
			V3I rtSize = depthStencil->GetSize();
			GetDeviceViewport(rtSize, *viewport, mDepthOnlyViewport, mDepthOnlyScissor);
		}
	}
	void DX12RenderPass::SetArgument(int slot, RenderDeviceDrawcallArgument* arg)
	{
		CheckAlways(slot < mArgs.size());
		mArgs[slot] = arg;
	}
	RenderDeviceTextureRef DX12RenderPass::GetBackbuffer()
	{
		return mFrameState->GetBackbuffer();
	}
	void DX12RenderPass::Execute(DX12RenderPassContext& context)
	{
		UINT activeRTCount;
		D3D12_CPU_DESCRIPTOR_HANDLE outputRTHandles[MaxRenderTargets];
		//output RenderTargets
		for (int i = 0; i < MaxRenderTargets; ++i)
		{
			if (mOutputTarget[i].GetPtr() != nullptr)
			{
				outputRTHandles[i] = mOutputTarget[i]->GetCPUHandle(TextureUsage::RenderTarget);
				mOutputTarget[i]->TransitToState(D3D12_RESOURCE_STATE_RENDER_TARGET, context.CommandList);
				if (mNeedClearColor[i])
				{
					context.CommandList->ClearRenderTargetView(outputRTHandles[i], (const float*)&mClearColorValues[i], 0, nullptr);
				}
			}
			else
			{
				activeRTCount = i;
				break;
			}
		}
		//depth stencil
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsHandle = {};
		D3D12_CPU_DESCRIPTOR_HANDLE* handlePtr = nullptr;
		if (mDepthStencil.GetPtr())
		{
			D3D12_CLEAR_FLAGS flag = {};
			if (mNeedClearDepth)
			{
				flag |= D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH;
			}
			else
			{
				flag &= ~D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH;
			}
			if (mNeedClearStencil)
			{
				flag |= D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_STENCIL;
			}
			else
			{
				flag &= ~D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_STENCIL;
			}
			dsHandle = mDepthStencil->GetCPUHandle(TextureUsage::DepthStencil);
			mDepthStencil->TransitToState(D3D12_RESOURCE_STATE_DEPTH_WRITE, context.CommandList);
			context.CommandList->ClearDepthStencilView(dsHandle, flag, mClearDepthValue, mClearStencilValue, 0, nullptr);
			handlePtr = &dsHandle;
		}
		context.CommandList->OMSetRenderTargets(activeRTCount, outputRTHandles, FALSE, handlePtr);
		if (mUseDepthOnlyViewport)
		{
			context.CommandList->RSSetViewports(1, &mDepthOnlyViewport);
			context.CommandList->RSSetScissorRects(1, &mDepthOnlyScissor);
		}
		else
		{
			context.CommandList->RSSetViewports(8, mViewport);
			context.CommandList->RSSetScissorRects(8, mScissor);
		}
		context.CommandList->IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//context
		for (RenderDeviceCommand* cmd : mCommands)
		{
			cmd->Prepare(&context);
		}
		for (RenderDeviceCommand* cmd : mCommands)
		{
			cmd->Execute(&context);
			cmd->Reset();
			mCommandPool->FreeCommand(cmd);
		}
		mCommands.clear();
	}
	void DX12RenderPass::PreparePassArgs(DX12RenderPassContext* ctx)
	{//set pass global args
		for (size_t i = 0; i < mArgs.size(); ++i)
		{
			mArgs[i]->Apply(i, ctx);
		}
	}
}