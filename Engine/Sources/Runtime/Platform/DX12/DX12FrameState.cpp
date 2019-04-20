#include "Platform/DX12/DX12FrameState.h"
#include "Platform/DX12/DX12MemAllocators.h"
#include "Platform/DX12/DX12DescriptorHeapAllocators.h"
#include "Platform/DX12/DX12BufferAllocator.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"

namespace Yes
{
	//DX12CommandManager
	DX12CommandManager::DX12CommandManager(ID3D12Device* dev, ID3D12CommandQueue* cq, int backbufferIndex)
		: mCommandQueue(cq)
	{
		wchar_t tempName[1024];
		swprintf(tempName, 1024, L"Frame%d", backbufferIndex);
		CheckSucceeded(dev->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, 
			IID_PPV_ARGS(&mAllocator)));
		mAllocator->SetName(tempName);
		CheckSucceeded(dev->CreateCommandList(
			0, 
			D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, 
			mAllocator, nullptr, IID_PPV_ARGS(&mCommandList)));
		mCommandList->SetName(tempName);
		CheckSucceeded(mCommandList->Close());
	}
	ID3D12GraphicsCommandList* DX12CommandManager::ResetAndGetCommandList()
	{
		mCommandList->Reset(mAllocator, nullptr);
		return mCommandList;
	}
	void DX12CommandManager::CloseAndExecuteCommandList()
	{
		CheckSucceeded(mCommandList->Close());
		ID3D12CommandList* lst = mCommandList;
		mCommandQueue->ExecuteCommandLists(1, &lst);
	}
	void DX12CommandManager::Reset()
	{
		mAllocator->Reset();
	}
	//DX12FrameState
	static int HeapSlotBlockCount = 2048;
	DX12FrameState::DX12FrameState(ID3D12Device* dev, DX12Texture2D* frameBuffer, ID3D12CommandQueue* cq, int backBufferIndex)
		: mExpectedValue(0)
		, mConstantBufferAllocator(CreateDX12LinearBlockBufferAllocator(dev, 512 * 1024, 512 * 1024))
		, mLinearDescriptorHeapAllocator(CreateShaderVisibileDescriptorHeapAllocator(dev))
		, mCommandManager(dev, cq, backBufferIndex)
		, mFrameBuffer(frameBuffer)
		, mCommandQueue(cq)
		, mBackBufferIndex(backBufferIndex)
	{
		CheckSucceeded(dev->CreateFence(mExpectedValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		mEvent = CreateEvent(nullptr, FALSE, FALSE, L"DX12FrameState::mEvent");
	}
	DX12FrameState::~DX12FrameState()
	{
		WaitGPUAndCleanup();
	}
	void DX12FrameState::CPUFinish()
	{
		++mExpectedValue;
		ID3D12GraphicsCommandList* cmdList = mCommandManager.ResetAndGetCommandList();
		mFrameBuffer->TransitToState(D3D12_RESOURCE_STATE_PRESENT, cmdList);
		mCommandManager.CloseAndExecuteCommandList();
		CheckSucceeded(mCommandQueue->Signal(mFence.GetPtr(), mExpectedValue));
	}
	void DX12FrameState::WaitGPUAndCleanup()
	{
		if (mFence->GetCompletedValue() != mExpectedValue)
		{
			CheckSucceeded(mFence->SetEventOnCompletion(mExpectedValue, mEvent));
			WaitForSingleObject(mEvent, INFINITE);
		}
		mConstantBufferAllocator->Reset();
		mLinearDescriptorHeapAllocator->Reset();
		mCommandManager.Reset();
	}
}