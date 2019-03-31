#include "Platform/DX12/DX12DescriptorHeapAllocators.h"
#include "Platform/DX12/DX12Parameters.h"
#include "Memory/RangeAllocator.h"
#include "Misc/Debug.h"

namespace Yes
{
	enum class DescriptorHeapType
	{
		SRV,
		RTV,
		DSV,
		DescriptorHeapTypeCount,
	};
	D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType2D3D12_DESCRIPTOR_HEAP_TYPE[] =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
	};
	size_t GetDescriptorHeapTypeCount()
	{
		return (size_t)DescriptorHeapType::DescriptorHeapTypeCount;
	}
	static DescriptorHeapType ResourceType2AllocatorIndex[] =
	{
		DescriptorHeapType::SRV,
		DescriptorHeapType::SRV,
		DescriptorHeapType::RTV,
		DescriptorHeapType::DSV,
	};
	size_t GetDescriptorHeapAllocatorIndex(ResourceType type)
	{
		return (size_t)ResourceType2AllocatorIndex[(int)type];
	}
	void CreateDescriptorHeapAllocators(
		ID3D12Device* dev,
		bool isTemp,
		IDX12DescriptorHeapAllocator* outAllocators[])
	{
		const size_t Size16K = 16 * 1024;
		if (isTemp)
		{
			for (int i = 0; i < (int)DescriptorHeapType::DescriptorHeapTypeCount; ++i)
			{
				outAllocators[i] = CreateDX12LinearBlockDescriptorHeapAllocator(
					dev,
					DescriptorHeapType2D3D12_DESCRIPTOR_HEAP_TYPE[i],
					Size16K,
					Size16K);
			}
		}
		else
		{
			for (int i = 0; i < (int)DescriptorHeapType::DescriptorHeapTypeCount; ++i)
			{
				outAllocators[i] = CreateDX12BestFitDescriptorHeapAllocator(
					dev,
					DescriptorHeapType2D3D12_DESCRIPTOR_HEAP_TYPE[i],
					Size16K,
					Size16K);
			}
		}
	}
	struct DescriptorHeapCreator
	{
		using RangePtr = UINT64;
		using RangeKey = ID3D12DescriptorHeap*;
		DescriptorHeapCreator() = default;
		DescriptorHeapCreator(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12Device* device)
			: mDevice(device)
			, mHeapType(heapType)
		{}
		ID3D12DescriptorHeap* AllocRange(UINT64 size)
		{
			ID3D12DescriptorHeap* ret;
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = mHeapType;
			desc.NumDescriptors = (UINT)size;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 0;
			mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&ret));
			return ret;
		}
		void FreeRange(ID3D12DescriptorHeap* key)
		{
			key->Release();
		}
		DescriptorHeapCreator& operator=(const DescriptorHeapCreator&) = default;
		void SetConfig(const DescriptorHeapCreator& other)
		{
			(*this) = other;
		}
	private:
		ID3D12Device* mDevice;
		D3D12_DESCRIPTOR_HEAP_TYPE mHeapType;
	};

	class DX12LinearBlockDescriptorHeapAllocator : public IDX12DescriptorHeapAllocator
	{
	public:
		DX12LinearBlockDescriptorHeapAllocator(UINT64 blockSize, UINT64 maxReservation, UINT64 handleSize)
			: mImp(blockSize, maxReservation)
		{}
		virtual DX12DescriptorHeapSpace Allocate(size_t count) override
		{ 
			DX12DescriptorHeapSpace ret;
			UINT64 offset;
			mImp.Allocate(ret.Heap, offset, count, 1);
			ret.Offset.ptr = offset * mHandleSize;
			return ret;
		}
		virtual void Free(const DX12DescriptorHeapSpace& space) override
		{
			mImp.Free(space.Heap, space.Offset.ptr / mHandleSize);
		}
		virtual void Reset() override 
		{ 
			mImp.Reset(); 
		}
		void GetAllocationStats(size_t& count, size_t& used, size_t& total) override
		{
			mImp.Stats(count, used, total);
		}
		void SetConfig(const DescriptorHeapCreator& creator)
		{
			mImp.SetConfig(creator);
		}
	private:
		LinearBlockRangeAllocator<ID3D12DescriptorHeap*, UINT64, DescriptorHeapCreator> mImp;
		UINT64 mHandleSize;
	};

	IDX12DescriptorHeapAllocator* CreateDX12LinearBlockDescriptorHeapAllocator(
		ID3D12Device* dev, 
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, 
		size_t blockSize, 
		size_t maxReservation)
	{
		auto a = new DX12LinearBlockDescriptorHeapAllocator(
			blockSize, 
			maxReservation, 
			GetDX12RuntimeParameters().DescriptorHeapHandleSizes[heapType]);
		a->SetConfig(DescriptorHeapCreator{ heapType , dev });
		return a;
	}

	class DX12BestFitDescriptorHeapAllocator : public IDX12DescriptorHeapAllocator
	{
	public:
		DX12BestFitDescriptorHeapAllocator(UINT64 blockSize, UINT64 maxReservation, UINT64 handleSize)
			: mImp(blockSize, maxReservation)
			, mHandleSize(handleSize)
		{}
		virtual DX12DescriptorHeapSpace Allocate(size_t count) override
		{
			DX12DescriptorHeapSpace ret;
			UINT64 offset;
			mImp.Allocate(ret.Heap, offset, count, 1);
			ret.Offset.ptr = mHandleSize * offset;
			return ret;
		}
		virtual void Free(const DX12DescriptorHeapSpace& space) override
		{
			mImp.Free(space.Heap, space.Offset.ptr / mHandleSize);
		}
		virtual void Reset() override
		{
			mImp.Reset();
		}
		void GetAllocationStats(size_t& count, size_t& used, size_t& total) override
		{
			mImp.Stats(count, used, total);
		}
		void SetConfig(const DescriptorHeapCreator& creator)
		{
			mImp.SetConfig(creator);
		}
	private:
		BestFitRangeAllocator<ID3D12DescriptorHeap*, UINT64, DescriptorHeapCreator> mImp;
		UINT64 mHandleSize;
	};


	IDX12DescriptorHeapAllocator* CreateDX12BestFitDescriptorHeapAllocator(
		ID3D12Device* dev, 
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, 
		size_t blockSize, 
		size_t maxReservation)
	{
		auto a = new DX12BestFitDescriptorHeapAllocator(
			blockSize,
			maxReservation,
			GetDX12RuntimeParameters().DescriptorHeapHandleSizes[heapType]);
		a->SetConfig(DescriptorHeapCreator{ heapType , dev });
		return a;
	}
}