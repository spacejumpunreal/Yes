#include "Platform/DX12/DX12DescriptorHeapAllocators.h"
#include "Platform/DX12/DX12Parameters.h"
#include "Misc/Debug.h"

namespace Yes
{
	class DX12LinearDescriptorHeapAllocator : public IDX12DescriptorHeapAllocator
	{
	public:
		DX12LinearDescriptorHeapAllocator(ID3D12Device* dev, D3D12_DESCRIPTOR_HEAP_TYPE heapType, size_t defaultHeapSlots)
			: mCurrentHeap(nullptr)
			, mCurrentHeapLeftSlots(0)
			, mCurrentHeapOffset(0)
			, mSlotSize(GetDX12RuntimeParameters().DescriptorHeapHandleSizes[heapType])
			, mDefaultHeapSlots(defaultHeapSlots)
			, mDevice(dev)
		{
		}
		AllocatedDescriptorHeapSpace Allocate(size_t count) override
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
		void Free(const AllocatedDescriptorHeapSpace& space)
		{
			CheckAlways(false, "you do not need to call this");
		}
		void Reset() override
		{
			mCurrentHeap = nullptr;
			mCurrentHeapLeftSlots = 0;
			for (auto heap : mHeaps)
			{
				heap->Release();
			}
		}
	private:
		ID3D12DescriptorHeap* mCurrentHeap;
		size_t mCurrentHeapLeftSlots;
		UINT64 mCurrentHeapOffset;
		UINT64 mSlotSize;
		size_t mDefaultHeapSlots;
		ID3D12Device* mDevice;
		std::deque<ID3D12DescriptorHeap*> mHeaps;
	};
	IDX12DescriptorHeapAllocator* CreateDX12LinearDescriptorHeapAllocator(ID3D12Device* dev, D3D12_DESCRIPTOR_HEAP_TYPE heapType, size_t defaultHeapSlots)
	{
		return new DX12LinearDescriptorHeapAllocator(dev, heapType, defaultHeapSlots);
	}

	class DX12BestFitDescritorHeapAllocator
	{
	public:
		DX12BestFitDescritorHeapAllocator(ID3D12Device* dev, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t defaultHeapSlots);
		AllocatedDescriptorHeapSpace Allocate(size_t count);
		void Reset();
	};
	IDX12DescriptorHeapAllocator* CreateDX12BestFitDescriptorHeapAllocator(ID3D12Device* dev, D3D12_DESCRIPTOR_HEAP_TYPE heapType, size_t defaultHeapSlots)
	{
		return new DX12LinearDescriptorHeapAllocator(dev, heapType, defaultHeapSlots);
	}
}