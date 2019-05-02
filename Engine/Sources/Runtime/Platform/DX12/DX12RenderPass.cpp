#include "Platform/DX12/DX12RenderPass.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Platform/DX12/DX12FrameState.h"
#include "Platform/DX12/DX12ExecuteContext.h"
#include "Platform/DX12/DX12RenderCommand.h"
#include "Public/Memory/SizeUtils.h"
#include "Public/Misc/Debug.h"

namespace Yes
{
	//DX12RenderDevicePass
	void DX12Pass::Init(DX12FrameState* state, DX12RenderCommandPool* pool)
	{
		mFrameState = state;
		mCommandPool = pool;
	}
	void DX12Pass::Reset()
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
		mConstantBuffer = {};
		mTextures.clear();
		mCommandPool = nullptr;
		mDescritptorHeapSize = 0;
	}
	RenderDeviceCommand* DX12Pass::AddCommand(RenderCommandType type)
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
	void DX12Pass::SetOutput(
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

	void DX12Pass::SetDepthStencil(const RenderDeviceTextureRef& depthStencil, bool clearDepth, float depth, bool clearStencil, uint8 stencil, bool setViewport, const B3F* viewport)
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
	void DX12Pass::SetGlobalTexture(int idx, const RenderDeviceTextureRef& texture)
	{
		DX12Texture2D* tex = dynamic_cast<DX12Texture2D*>(texture.GetPtr());
		if (idx >= mTextures.size())
		{
			mTextures.resize(idx + 1);
		}
		mTextures[idx] = tex;
	}
	void DX12Pass::SetGlobalConstantBuffer(void* data, size_t size)
	{
		const size_t SizeAlign = 4;
		size_t alignedSize = AlignSize(size, SizeAlign);
		CheckAlways(alignedSize == size);
		mConstantBuffer = mFrameState->GetConstantBufferAllocator()->Allocate(alignedSize, SizeAlign);
		mConstantBuffer.Write(data, size);
	}
	RenderDeviceTextureRef DX12Pass::GetBackbuffer()
	{
		return mFrameState->GetBackbuffer();
	}
	void DX12Pass::CollectDescriptorHeapSize()
	{
		mDescritptorHeapSize = 0;
		mDescritptorHeapSize += mTextures.size();
		for (RenderDeviceCommand* cmd : mCommands)
		{
			mDescritptorHeapSize += cmd->GetDescriptorHeapSlotCount();
		}
	}
	void DX12Pass::Execute(DX12RenderPassContext& context)
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
		ID3D12DescriptorHeap* hs = context.GetHeapSpace().GetHeap();
		//context
		for (RenderDeviceCommand* cmd : mCommands)
		{
			cmd->Prepare(&context);
		}
		context.CommandList->SetDescriptorHeaps(1, &hs);
		for (RenderDeviceCommand* cmd : mCommands)
		{
			cmd->Execute(&context);
			cmd->Reset();
			mCommandPool->FreeCommand(cmd);
		}
		mCommands.clear();
	}
	D3D12_GPU_DESCRIPTOR_HANDLE DX12Pass::GetGlobalConstantBufferGPUAddress()
	{
		return D3D12_GPU_DESCRIPTOR_HANDLE{ mConstantBuffer.GetGPUAddress() };
	}
	void DX12Pass::Prepare(DX12RenderPassContext* ctx)
	{
		ctx->Prepare(mTextures.data(), mTextures.size());
	}
}