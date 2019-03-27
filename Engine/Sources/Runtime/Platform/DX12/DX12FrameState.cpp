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
		HeapCreator creator(MemoryAccessCase::CPUUpload, dev, ResourceType::Buffer);
		mAllocator = CreateDX12LinearBlockAllocator(creator, allocatorBlockSize);
	}
	AllocatedCBV DX12ConstantBufferManager::Allocate(size_t size)
	{
		const IDX12GPUMemoryRegion* region = mAllocator->Allocate(size, ConstantBufferAlignment);
		mAllocatedRegions.push_back(region);
		ID3D12Resource* resource;
		CheckSucceeded(mDevice->CreatePlacedResource(
			region->GetHeap(), 
			region->GetOffset(), 
			&CD3DX12_RESOURCE_DESC::Buffer(size),
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
		for (auto it = mAllocatedRegions.begin(); it != mAllocatedRegions.end(); ++it)
		{
			(*it)->Free();
		}
		mAllocatedRegions.clear();
	}

	//DX12CommandManager
	DX12CommandManager::DX12CommandManager(ID3D12Device * dev)
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
	void DX12CommandManager::Reset()
	{
		mAllocator->Reset();
	}

	//DX12FrameState
	static int HeapSlotBlockCount = 2048;
	DX12FrameState::DX12FrameState(ID3D12Device* dev, DX12RenderDeviceRenderTarget* frameBuffer)
		: mFrameBuffer(frameBuffer)
		, mExpectedValue(0)
		, mConstantBufferManager(dev)
		, mLinearDescriptorHeapAllocator(
			CreateDX12LinearDescriptorHeapAllocator(
				dev, 
				D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				HeapSlotBlockCount))
		, mCommandManager(dev)
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
		CheckAlways(mFence->Signal(mExpectedValue));
	}
	void DX12FrameState::WaitForFrame()
	{
		if (mFence->GetCompletedValue() != mExpectedValue)
		{
			CheckAlways(mFence->SetEventOnCompletion(mExpectedValue, mEvent));
		}
		mConstantBufferManager.Reset();
		mLinearDescriptorHeapAllocator->Reset();
		mCommandManager.Reset();
	}



}