#include "Platform/DX12/DX12RenderPass.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Platform/DX12/DX12FrameState.h"
#include "Platform/DX12/DX12ExecuteContext.h"
#include "Platform/DX12/DX12RenderCommand.h"
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
		CheckAlways(mCommands.empty());
		mCommands.clear();
		mName.clear();
		for (int i = 0; i < MaxRenderTargets; ++i)
		{
			mOutputTarget[i] = nullptr;
			mClearColorValues[i] = V4F{};
			mNeedClearColor[i] = true;
		}

		mNeedClearDepth = true;
		mNeedClearStencil = true;
		mClearDepthValue = 1.0f;
		mClearStencilValue = 0;
		mDepthStencil = nullptr;
		mFrameState = nullptr;
		mConstantBuffer = {};
		mCommandPool = nullptr;
	}
	RenderDeviceCommand* DX12Pass::AddCommand(RenderCommandType type)
	{
		RenderDeviceCommand* cmd = mCommandPool->AllocCommand(type);
		mCommands.push_back(cmd);
		return cmd;
	}
	void DX12Pass::SetOutput(const TRef<RenderDeviceRenderTarget>& renderTarget, int idx)
	{
		IDX12RenderTarget* rt = dynamic_cast<IDX12RenderTarget*>(renderTarget.GetPtr());
		mOutputTarget[idx] = rt;
	}
	void DX12Pass::SetClearColor(const V4F& clearColor, bool needed, int idx)
	{
		mClearColorValues[idx] = clearColor;
		mNeedClearColor[idx] = needed;
	}
	void DX12Pass::SetDepthStencil(const TRef<RenderDeviceDepthStencil>& depthStencil)
	{
		mDepthStencil = dynamic_cast<DX12DepthStencil*>(depthStencil.GetPtr());
	}
	void DX12Pass::SetClearDepth(float depth, uint8 stencil, bool neededDepth, bool needStencil)
	{
		mClearDepthValue = depth;
		mClearStencilValue = stencil;
		mNeedClearDepth = neededDepth;
		mNeedClearStencil = needStencil;
	}
	void DX12Pass::SetGlobalConstantBuffer(void* data, size_t size)
	{
		mConstantBuffer = mFrameState->GetConstantBufferManager().Allocate(size);
		void* wptr;
		CheckSucceeded(mConstantBuffer.Buffer->Map(0, nullptr, &wptr));
		memcpy(wptr, data, size);
	}
	TRef<RenderDeviceRenderTarget> DX12Pass::GetBackbuffer()
	{
		return (RenderDeviceRenderTarget*)mFrameState->GetBackbuffer();
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
				outputRTHandles[i] = mOutputTarget[i]->GetHandle(1);
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
			dsHandle = mDepthStencil->mHeapSpace.GetCPUHandle(0);
			context.CommandList->ClearDepthStencilView(dsHandle, flag, mClearDepthValue, mClearStencilValue, 0, nullptr);
			handlePtr = &dsHandle;
		}
		context.CommandList->OMSetRenderTargets(activeRTCount, outputRTHandles, FALSE, handlePtr);
		if (mConstantBuffer.Buffer == nullptr)
		{
			context.GlobalConstantBufferGPUAddress.ptr = 0;
		}
		else
		{
			context.GlobalConstantBufferGPUAddress.ptr = mConstantBuffer.Buffer->GetGPUVirtualAddress();
		}
		
		context.CommandList->RSSetViewports(1, &context.DefaultViewport);
		context.CommandList->RSSetScissorRects(1, &context.DefaultScissor);
		context.CommandList->IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
}