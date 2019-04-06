#include "Platform/DX12/DX12FrameState.h"
#include "Platform/DX12/DX12MemAllocators.h"
#include "Platform/DX12/DX12DescriptorHeapAllocators.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"

namespace Yes
{
	//DX12ConstantBufferManager
	static const UINT64 ConstantBufferAlignment = 256;
	DX12ConstantBufferManager::DX12ConstantBufferManager(ID3D12Device* dev)
		: mDevice(dev)
	{
		size_t allocatorBlockSize = 8 * 1024 * 1024;
		HeapCreator creator(MemoryAccessCase::CPUUpload, dev, D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
		mAllocator = CreateDX12LinearBlockAllocator(creator, allocatorBlockSize, allocatorBlockSize);
	}
	AllocatedCBV DX12ConstantBufferManager::Allocate(size_t size)
	{
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);
		D3D12_RESOURCE_ALLOCATION_INFO info = mDevice->GetResourceAllocationInfo(0, 1, &desc);
		DX12GPUMemoryRegion region = mAllocator->Allocate(info.SizeInBytes, info.Alignment);
		ID3D12Resource* resource;
		CheckSucceeded(mDevice->CreatePlacedResource(
			region.Heap, 
			region.Offset, 
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, 
			nullptr, 
			IID_PPV_ARGS(&resource)));
		return AllocatedCBV{ resource };
	}
	void DX12ConstantBufferManager::Reset()
	{
		for (auto it = mAllocatedResources.begin(); it != mAllocatedResources.end(); ++it)
		{
			(*it)->Release();
		}
		mAllocatedResources.clear();
		mAllocator->Reset();
	}

	//DX12CommandManager
	DX12CommandManager::DX12CommandManager(ID3D12Device* dev, ID3D12CommandQueue* cq)
		: mCommandQueue(cq)
	{
		CheckSucceeded(dev->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, 
			IID_PPV_ARGS(&mAllocator)));
		CheckSucceeded(dev->CreateCommandList(
			0, 
			D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, 
			mAllocator, nullptr, IID_PPV_ARGS(&mCommandList)));
		mCommandList->Close();
	}
	ID3D12GraphicsCommandList* DX12CommandManager::ResetAndGetCommandList()
	{
		mCommandList->Reset(mAllocator, nullptr);
		return mCommandList;
	}
	void DX12CommandManager::CloseAndExecuteCommandList()
	{
		mCommandList->Close();
		ID3D12CommandList* lst = mCommandList;
		mCommandQueue->ExecuteCommandLists(1, &lst);
	}
	void DX12CommandManager::Reset()
	{
		mAllocator->Reset();
	}
	//DX12FrameState
	static int HeapSlotBlockCount = 2048;
	DX12FrameState::DX12FrameState(ID3D12Device* dev, DX12Backbuffer* frameBuffer, ID3D12CommandQueue* cq)
		: mExpectedValue(0)
		, mConstantBufferManager(dev)
		, mLinearDescriptorHeapAllocator(
			CreateDX12LinearBlockDescriptorHeapAllocator(
				dev, 
				D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				HeapSlotBlockCount, HeapSlotBlockCount))
		, mCommandManager(dev, cq)
		, mFrameBuffer(frameBuffer)
	{
		CheckSucceeded(dev->CreateFence(mExpectedValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
		mEvent = CreateEvent(nullptr, FALSE, FALSE, L"DX12FrameState::mEvent");
	}
	DX12FrameState::~DX12FrameState()
	{
		WaitForFrame();
	}
	void DX12FrameState::Finish()
	{
		++mExpectedValue;
		ID3D12GraphicsCommandList* cmdList = ResetAndGetCommandList();
		mFrameBuffer->TransitToState(D3D12_RESOURCE_STATE_PRESENT, cmdList);
		CloseAndExecuteCommandList();
		CheckSucceeded(mFence->Signal(mExpectedValue));
	}
	void DX12FrameState::WaitForFrame()
	{
		if (mFence->GetCompletedValue() != mExpectedValue)
		{
			CheckAlways(mFence->SetEventOnCompletion(mExpectedValue, mEvent));
			WaitForSingleObject(mEvent, INFINITE);
		}
		mConstantBufferManager.Reset();
		mLinearDescriptorHeapAllocator->Reset();
		mCommandManager.Reset();
	}



}