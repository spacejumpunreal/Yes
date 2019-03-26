#pragma once
#include "Yes.h"
#include "Windows.h"
#include <d3d12.h>
#include "d3dx12.h"

namespace Yes
{
	class IDX12GPUMemoryAllocator;

	class IDX12GPUMemoryRegion
	{
	public:
		virtual ID3D12Heap* GetHeap() const = 0;
		virtual UINT64 GetOffset() const = 0;
		virtual void Free() const = 0;
	};

	class IDX12GPUMemoryAllocator
	{
	public:
		static const UINT64 ALIGN_4K = 4 * 1024;
		static const UINT64 ALIGN_64K = 64 * 1024;
		static const UINT64 ALIGN_4M = 4 * 1024 * 1024;
		virtual const IDX12GPUMemoryRegion* Allocate(UINT64 size, UINT64 alignment) = 0;
		virtual void Free(const IDX12GPUMemoryRegion* region) = 0;
		virtual void GetAllocationStats(size_t& count, size_t& used, size_t& total) 
		{
			count = 0;
			used = 0;
			total = 0;
		}
		static UINT64 CalcAlignedSize(UINT64 currentOffset, UINT64 alignment, UINT64 size)
		{
			return GetNextAlignedOffset(currentOffset, alignment) + size - currentOffset;
		}
		static UINT64 GetNextAlignedOffset(UINT64 offset, UINT64 alignment)
		{
			return (offset + alignment - 1) & ~(alignment - 1);
		}
	protected:

	};
	enum class ResourceType
	{
		Buffer,
		Texture,
		RenderTarget,
		ResourceTypeCount,
	};

	enum class MemoryAccessCase
	{
		GPUAccessOnly,
		CPUUpload,
	};
	struct HeapCreator
	{
	public:
		HeapCreator(MemoryAccessCase accessFlag, ID3D12Device* device, ResourceType resourceType)
			: mAccessFlag(accessFlag)
			, mDevice(device)
			, mResourceType(resourceType)
		{
		}
		HeapCreator(const HeapCreator& other) = default;
		ID3D12Heap* CreateHeap(UINT64 size);
		ID3D12Device* mDevice;
		MemoryAccessCase mAccessFlag;
		ResourceType mResourceType;
	};
	IDX12GPUMemoryAllocator* CreateDX12LinearBlockAllocator(const HeapCreator& creator, UINT64 blockSize);
	IDX12GPUMemoryAllocator* CreateDX12FirstFitAllocator(const HeapCreator& creator, UINT64 blockSize);
}