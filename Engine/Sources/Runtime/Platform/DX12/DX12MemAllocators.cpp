#include "Platform/DX12/DX12MemAllocators.h"
#include "Memory/RangeAllocator.h"
#include "Misc/Debug.h"
#include "Misc/Math.h"
#include "Memory/ObjectPool.h"

#include <d3d12.h>
#include "d3dx12.h"

#include <unordered_map>
#include <map>
#include <algorithm>

namespace Yes
{
	enum class GPUMemoryHeapType
	{
		Buffer,
		NonRTDSTexture,
		RTDSTexture,
		GPUMemoryHeapTypeCount,
	};
	size_t GetGPUMemoryHeapTypeCount()
	{
		return (size_t)GPUMemoryHeapType::GPUMemoryHeapTypeCount;
	}
	static const GPUMemoryHeapType ResourceType2AllocatorIndex[] =
	{
		GPUMemoryHeapType::Buffer,
		GPUMemoryHeapType::NonRTDSTexture,
		GPUMemoryHeapType::RTDSTexture,
		GPUMemoryHeapType::RTDSTexture,
	};
	size_t GetGPUMemoryAllocatorIndex(ResourceType type)
	{
		return (size_t)ResourceType2AllocatorIndex[(int)type];
	}
	D3D12_HEAP_FLAGS MemoryHeapType2HeapFlags[] =
	{
		D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
		D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES,
		D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES,
	};
	void CreateGPUMemoryAllocators(ID3D12Device* dev, MemoryAccessCase access, IDX12GPUMemoryAllocator* outAllocators[])
	{
		const UINT64 Size4M = 4 * 1024 * 1024;
		switch (access)
		{
		case MemoryAccessCase::GPUAccessOnly:
			for (int i = 0; i < (int)GPUMemoryHeapType::GPUMemoryHeapTypeCount; ++i)
			{
				outAllocators[i] = CreateDX12BestFitAllocator(
					HeapCreator(access, dev, MemoryHeapType2HeapFlags[i]),
					Size4M,
					0);
			}
		case MemoryAccessCase::CPUUpload:
			for (int i = 0; i < (int)GPUMemoryHeapType::GPUMemoryHeapTypeCount; ++i)
			{
				outAllocators[i] = CreateDX12LinearBlockAllocator(
					HeapCreator(access, dev, MemoryHeapType2HeapFlags[i]),
					Size4M,
					Size4M * 2);
			}
			break;
		default:
			CheckAlways(false);
		}
	}
	static ID3D12Heap* CreateHeapWithConfig(UINT64 size, MemoryAccessCase accessFlag, ID3D12Device* device, D3D12_HEAP_FLAGS resourceFlag)
	{
		D3D12_HEAP_DESC desc = {};
		desc.SizeInBytes = size;
		switch (accessFlag)
		{
		case MemoryAccessCase::GPUAccessOnly:
			desc.Properties.Type = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT;
			desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			desc.Properties.CreationNodeMask = 0;
			desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL::D3D12_MEMORY_POOL_UNKNOWN;
			desc.Flags = resourceFlag;
			break;
		case MemoryAccessCase::CPUUpload:
			desc.Properties.Type = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_UPLOAD;
			desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			desc.Properties.CreationNodeMask = 0;
			desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL::D3D12_MEMORY_POOL_UNKNOWN;
			desc.Flags = resourceFlag;
			break;
		default:
			CheckAlways(false);
		}
		desc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
		ID3D12Heap* heap;
		CheckSucceeded(device->CreateHeap(&desc, IID_PPV_ARGS(&heap)));
		return heap;
	}
	ID3D12Heap* HeapCreator::CreateHeap(UINT64 size)
	{
		return CreateHeapWithConfig(size, mAccessFlag, mDevice, mResourceFlag);
	}
	void HeapCreator::SetConfig(const HeapCreator& other)
	{
		*this = other;
	}

	class Dx12BestFitMemAllocator : public IDX12GPUMemoryAllocator
	{
	public:
		Dx12BestFitMemAllocator(UINT64 blockSize, UINT64 maxReservation)
			: mImp(blockSize, maxReservation)
		{}
		DX12GPUMemoryRegion Allocate(UINT64 size, UINT64 alignment) override
		{
			DX12GPUMemoryRegion ret;
			mImp.Allocate(ret.Heap, ret.Offset, size, alignment);
			return ret;
		}
		void Free(const DX12GPUMemoryRegion& region) override
		{
			mImp.Free(region.Heap, region.Offset);
		}
		virtual void Reset() override
		{
			mImp.Reset();
		}
		void GetAllocationStats(size_t& count, size_t& used, size_t& total) override
		{
			mImp.Stats(count, used, total);
		}
		void SetConfig(const HeapCreator& hc)
		{
			mImp.SetConfig(hc);
		}
	private:
		BestFitRangeAllocator<ID3D12Heap*, UINT64, HeapCreator> mImp;
	};
	IDX12GPUMemoryAllocator* CreateDX12BestFitAllocator(const HeapCreator& creator, UINT64 blockSize, UINT64 maxReservation)
	{
		auto a = new Dx12BestFitMemAllocator(blockSize, maxReservation);
		a->SetConfig(creator);
		return a;
	}

	class DX12LinearBlockMemAllocator : public IDX12GPUMemoryAllocator
	{
	public:
		DX12LinearBlockMemAllocator(UINT64 blockSize, UINT64 maxReservation)
			: mImp(blockSize, maxReservation)
		{}
		DX12GPUMemoryRegion Allocate(UINT64 size, UINT64 alignment) override
		{
			DX12GPUMemoryRegion ret;
			mImp.Allocate(ret.Heap, ret.Offset, size, alignment);
			return ret;
		}
		void Free(const DX12GPUMemoryRegion& region) override
		{
			mImp.Free(region.Heap, region.Offset);
		}
		virtual void Reset() override
		{
			mImp.Reset();
		}
		void GetAllocationStats(size_t& count, size_t& used, size_t& total) override
		{
			mImp.Stats(count, used, total);
		}
		void SetConfig(const HeapCreator& hc)
		{
			mImp.SetConfig(hc);
		}
	private:
		LinearBlockRangeAllocator<ID3D12Heap*, UINT64, HeapCreator> mImp;
	};

	IDX12GPUMemoryAllocator* CreateDX12LinearBlockAllocator(const HeapCreator& creator, UINT64 blockSize, UINT64 maxReservation)
	{
		auto a = new DX12LinearBlockMemAllocator(blockSize, maxReservation);
		a->SetConfig(creator);
		return a;
	}
}