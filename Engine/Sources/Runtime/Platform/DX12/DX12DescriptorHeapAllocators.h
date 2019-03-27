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

	class IDX12DescriptorHeapAllocator
	{
	public:
		virtual AllocatedDescriptorHeapSpace Allocate(size_t count) = 0;
		virtual void Free(const AllocatedDescriptorHeapSpace& space) = 0;
		virtual void Reset() = 0;
	};
	IDX12DescriptorHeapAllocator* CreateDX12LinearDescriptorHeapAllocator(ID3D12Device* dev, D3D12_DESCRIPTOR_HEAP_TYPE heapType, size_t defaultHeapSlots);


}