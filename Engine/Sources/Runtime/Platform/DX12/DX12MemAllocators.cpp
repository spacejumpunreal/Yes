#include "Platform/DX12/DX12MemAllocators.h"
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
	
	static ID3D12Heap* CreateHeapWithConfig(UINT64 size, MemoryAccessCase accessFlag, ID3D12Device* device, ResourceType resourceType)
	{
		D3D12_HEAP_DESC desc = {};
		desc.SizeInBytes = size;
		D3D12_HEAP_FLAGS resType;
		switch (resourceType)
		{
		case ResourceType::Buffer:
			resType = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
			break;
		case ResourceType::Texture:
			resType = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
			break;
		case ResourceType::RenderTarget:
			resType = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
			break;
		default:
			resType = D3D12_HEAP_FLAG_NONE;
			break;
		}
		switch (accessFlag)
		{
		case MemoryAccessCase::GPUAccessOnly:
			desc.Properties.Type = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT;
			desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			desc.Properties.CreationNodeMask = 0;
			desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL::D3D12_MEMORY_POOL_UNKNOWN;
			desc.Flags = resType;
			break;
		case MemoryAccessCase::CPUUpload:
			desc.Properties.Type = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_UPLOAD;
			desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			desc.Properties.CreationNodeMask = 0;
			desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL::D3D12_MEMORY_POOL_UNKNOWN;
			desc.Flags = resType;
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
		return CreateHeapWithConfig(size, mAccessFlag, mDevice, mResourceType);
	}
	class DX12GPUMemoryRegion : public IDX12GPUMemoryRegion
	{
	public:
		ID3D12Heap* GetHeap() const { return mHeap; }
		UINT64 GetOffset() const { return mOffset; }
		void Free() const
		{
			mAllocator->Free(this);
		}
		ID3D12Heap* mHeap;
		UINT64 mOffset;
		UINT64 mStart;
		UINT64 mSize;
		IDX12GPUMemoryAllocator* mAllocator;
	};


	class DX12FirstFitAllocator : public IDX12GPUMemoryAllocator
	{
		struct AvailableRange
		{
			ID3D12Heap* Heap;
			UINT64 Start;
		};
		using AvailableRangeTree = std::multimap<UINT64, AvailableRange>;
		using RangeTree = std::map<UINT64, UINT64>;
		using HeapMap = std::unordered_map<ID3D12Heap*, RangeTree>;
	private:
		HeapCreator mCreator;
		UINT64 mDefaultBlockSize;
		AvailableRangeTree mAvailableMem;
		HeapMap mHeapMap;
		ObjectPool<DX12GPUMemoryRegion> mRegionPool;
	public:
		DX12FirstFitAllocator(HeapCreator creator, UINT64 defaultBlockSize)
			: mCreator(creator)
			, mDefaultBlockSize(defaultBlockSize)
			, mRegionPool(1024)
		{}
		virtual const IDX12GPUMemoryRegion* Allocate(UINT64 size, UINT64 alignment)
		{
			auto it = mAvailableMem.lower_bound(size);
			DX12GPUMemoryRegion* ret = mRegionPool.Allocate();
			ret->mAllocator = this;
			bool found = false;
			while (it != mAvailableMem.end())
			{
				UINT64 alignedSize = CalcAlignedSize(it->second.Start, alignment, size);
				if (alignedSize <= it->first)
				{
					UINT64 begin = ret->mStart = it->second.Start;
					ret->mOffset = GetNextAlignedOffset(it->second.Start, alignment);
					ret->mSize = alignedSize;
					ID3D12Heap* heap = ret->mHeap = it->second.Heap;
					UINT64 leftSize = it->first - alignedSize;
					mAvailableMem.erase(it);
					mHeapMap[heap].erase(begin);
					//add modified region
					if (leftSize > 0)
					{
						UINT64 newStart = ret->mStart + alignedSize;
						mAvailableMem.insert(std::make_pair(leftSize, AvailableRange{ heap, newStart }));
						mHeapMap[heap].insert(std::make_pair(newStart, leftSize));
					}
					return ret;
				}
			}
			//no available block, need to add new Heap
			ret->mOffset = 0;
			ret->mStart = 0;
			ret->mSize = size;
			ID3D12Heap* heap;
			if (size >= mDefaultBlockSize)
			{//no need to add to free list
				heap = mCreator.CreateHeap(size);
			}
			else
			{//need to add to free list
				heap = mCreator.CreateHeap(mDefaultBlockSize);
				//need add left range
				UINT64 leftSize = mDefaultBlockSize - size;
				mAvailableMem.insert(std::make_pair(leftSize, AvailableRange{ heap, size }));
				mHeapMap[heap].insert(std::make_pair(size, leftSize));
			}
			ret->mHeap = heap;
			return ret;
		}
		virtual void Free(const IDX12GPUMemoryRegion* region)
		{
			CheckAlways(false);//not implemented
		}
	};
	IDX12GPUMemoryAllocator* CreateDX12FirstFitAllocator(const HeapCreator& creator, UINT64 blockSize)
	{
		return new DX12FirstFitAllocator(creator, blockSize);
	}
	class DX12LinearBlockAllocator : public IDX12GPUMemoryAllocator
	{
		struct BlockData
		{
			ID3D12Heap* Heap;
			int References;
		};
	private:
		UINT64 mBlockSize;
		size_t mAllocationCount;
		size_t mAllocationUsed;
		size_t mAllocationTotal;
		std::list<BlockData> mHeaps;
		UINT64 mCurrentBlockLeftSize;
		HeapCreator mHeapCreator;
		ObjectPool<DX12GPUMemoryRegion> mRegionPool;
	public:
		DX12LinearBlockAllocator(const HeapCreator& creator, UINT64 blockSize)
			: mBlockSize(blockSize)
			, mAllocationCount(0)
			, mAllocationUsed(0)
			, mAllocationTotal(0)
			, mCurrentBlockLeftSize(0)
			, mHeapCreator(creator)
			, mRegionPool(1024)
		{}
		const IDX12GPUMemoryRegion* Allocate(UINT64 size, UINT64 alignment) override
		{
			CheckAlways(size <= mBlockSize);
			UINT64 sizeAfterAlign = CalcAlignedSize(mBlockSize - mCurrentBlockLeftSize, alignment, size);
			if (size > mCurrentBlockLeftSize)
			{
				mHeaps.push_front(BlockData{ mHeapCreator.CreateHeap(mBlockSize), 1 });
				mCurrentBlockLeftSize = mBlockSize;
				mAllocationTotal += mBlockSize;
			}
			DX12GPUMemoryRegion* ret = mRegionPool.Allocate();
			ret->mHeap = mHeaps.back().Heap;
			ret->mStart = mBlockSize - mCurrentBlockLeftSize;
			ret->mOffset = GetNextAlignedOffset(ret->mStart, alignment);
			ret->mSize = size;
			ret->mAllocator = this;
			++mAllocationCount;
			mAllocationUsed += size;
			return ret;
		}
		void Free(const IDX12GPUMemoryRegion* reg) override
		{
			const DX12GPUMemoryRegion* region = (const DX12GPUMemoryRegion*)reg;
			for (auto it = mHeaps.begin(); it != mHeaps.end(); ++it)
			{
				if (it->Heap == region->mHeap)
				{
					--mAllocationUsed;
					--mAllocationCount;
					--it->References;
					if (it->References == 0)
					{
						it->Heap->Release();
						mHeaps.erase(it);
						mAllocationTotal -= mBlockSize;
					}
					mRegionPool.Deallocate(const_cast<DX12GPUMemoryRegion*>(region));
					return;
				}
			}
			CheckAlways(false, "freeing non existing");
		}
		void GetAllocationStats(size_t& count, size_t& used, size_t& total) override
		{
			count = mAllocationCount;
			used = mAllocationUsed;
			total = mAllocationTotal;
		}
	};
	IDX12GPUMemoryAllocator* CreateDX12LinearBlockAllocator(const HeapCreator& creator, UINT64 blockSize)
	{
		return new DX12LinearBlockAllocator(creator, blockSize);
	}
}