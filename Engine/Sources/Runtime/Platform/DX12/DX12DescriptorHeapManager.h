#pragma once
#include "Yes.h"

#include <Windows.h>
#include <d3d12.h>
#include <deque>

namespace Yes
{
	struct AllocatedDescriptorHeapSpace
	{
		ID3D12DescriptorHeap* Heap;
		D3D12_GPU_DESCRIPTOR_HANDLE Start;
	};

	class DX12DescriptorHeapManager
	{
	public:
		DX12DescriptorHeapManager(ID3D12Device* dev, UINT64 slotSize, size_t defaultHeapSlots);
		AllocatedDescriptorHeapSpace Allocate(size_t count);
		void Reset();
	private:
		ID3D12DescriptorHeap* mCurrentHeap;
		size_t mCurrentHeapLeftSlots;
		UINT64 mCurrentHeapOffset;
		UINT64 mSlotSize;
		size_t mDefaultHeapSlots;
		ID3D12Device* mDevice;
		std::deque<ID3D12DescriptorHeap*> mHeaps;
	};
}