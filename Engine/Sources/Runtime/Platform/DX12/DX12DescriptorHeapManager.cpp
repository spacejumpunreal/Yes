#include "Platform/DX12/DX12DescriptorHeapManager.h"
#include "Misc/Debug.h"



namespace Yes
{
	DX12DescriptorHeapManager::DX12DescriptorHeapManager(ID3D12Device * dev, UINT64 slotSize, size_t defaultHeapSlots)
		: mCurrentHeap(nullptr)
		, mCurrentHeapLeftSlots(0)
		, mCurrentHeapOffset(0)
		, mSlotSize(slotSize)
		, mDefaultHeapSlots(defaultHeapSlots)
		, mDevice(dev)
	{
	}
	AllocatedDescriptorHeapSpace DX12DescriptorHeapManager::Allocate(size_t count)
	{
		CheckAlways(count < mDefaultHeapSlots);
		if (mCurrentHeapLeftSlots < count)
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = (UINT)mDefaultHeapSlots;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			CheckSucceeded(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mCurrentHeap)));
			mHeaps.push_back(mCurrentHeap);
			mCurrentHeapLeftSlots = mDefaultHeapSlots;
			mCurrentHeapOffset = 0;
		}
		AllocatedDescriptorHeapSpace ret = {
			mCurrentHeap,
			mCurrentHeapOffset,
		};
		mCurrentHeapOffset += mSlotSize * count;
		mCurrentHeapLeftSlots -= count;
		return ret;
	}
	void DX12DescriptorHeapManager::Reset()
	{
		mCurrentHeap = nullptr;
		mCurrentHeapLeftSlots = 0;
		for (auto heap : mHeaps)
		{
			heap->Release();
		}
	}
}