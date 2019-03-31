#pragma once
#include "Yes.h"

#include "Platform/DX12/DX12ResourceSpace.h"
#include <deque>

namespace Yes
{

	class IDX12DescriptorHeapAllocator
	{
	public:
		virtual DX12DescriptorHeapSpace Allocate(size_t count) = 0;
		virtual void Free(const DX12DescriptorHeapSpace& space) = 0;
		virtual void Reset() = 0;
		virtual void GetAllocationStats(size_t& count, size_t& used, size_t& total) = 0;
	};

	size_t GetDescriptorHeapTypeCount();
	size_t GetDescriptorHeapAllocatorIndex(ResourceType type);
	void CreateDescriptorHeapAllocators(
		ID3D12Device* dev,
		bool isTemp,
		IDX12DescriptorHeapAllocator* outAllocators[]);

	IDX12DescriptorHeapAllocator* CreateDX12LinearBlockDescriptorHeapAllocator(
		ID3D12Device* dev, 
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, 
		size_t blockSize,
		size_t maxReservation);
	IDX12DescriptorHeapAllocator* CreateDX12BestFitDescriptorHeapAllocator(
		ID3D12Device* dev, 
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, 
		size_t blockSize,
		size_t maxReservation);

}