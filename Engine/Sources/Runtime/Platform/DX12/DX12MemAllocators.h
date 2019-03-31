#pragma once
#include "Yes.h"

#include "Platform/DX12/DX12ResourceSpace.h"

#include "d3dx12.h"


namespace Yes
{
	class IDX12GPUMemoryAllocator
	{
	public:
		static const UINT64 ALIGN_4K = 4 * 1024;
		static const UINT64 ALIGN_64K = 64 * 1024;
		static const UINT64 ALIGN_4M = 4 * 1024 * 1024;
	public:
		virtual DX12GPUMemoryRegion Allocate(UINT64 size, UINT64 alignment) = 0;
		virtual void Free(const DX12GPUMemoryRegion& region) = 0;
		virtual void Reset() = 0;
		virtual void GetAllocationStats(size_t& count, size_t& used, size_t& total) = 0;
	};


	enum class MemoryAccessCase : byte
	{
		GPUAccessOnly,
		CPUUpload,
	};

	struct HeapCreator
	{
	public:
		HeapCreator(MemoryAccessCase accessFlag, ID3D12Device* device, D3D12_HEAP_FLAGS resourceType)
			: mAccessFlag(accessFlag)
			, mDevice(device)
			, mResourceFlag(resourceType)
		{
		}
		HeapCreator() = default;
		HeapCreator(const HeapCreator&) = default;
		void SetConfig(const HeapCreator& other);
		ID3D12Heap* CreateHeap(UINT64 size);
	public:
		using RangePtr = UINT64;
		using RangeKey = ID3D12Heap*;
		ID3D12Heap* AllocRange(UINT64 size)
		{
			return CreateHeap(size);
		}
		void FreeRange(ID3D12Heap* heap)
		{
			heap->Release();
		}
	private:
		ID3D12Device* mDevice;
		MemoryAccessCase mAccessFlag;
		D3D12_HEAP_FLAGS mResourceFlag;
	};

	size_t GetGPUMemoryHeapTypeCount();
	size_t GetGPUMemoryAllocatorIndex(ResourceType type);
	void CreateGPUMemoryAllocators(
		ID3D12Device* dev,
		MemoryAccessCase access,
		IDX12GPUMemoryAllocator* outAllocators[]);
	IDX12GPUMemoryAllocator* CreateDX12LinearBlockAllocator(const HeapCreator& creator, UINT64 blockSize, UINT64 maxReservation);
	IDX12GPUMemoryAllocator* CreateDX12BestFitAllocator(const HeapCreator& creator, UINT64 blockSize, UINT64 maxReservation);
}