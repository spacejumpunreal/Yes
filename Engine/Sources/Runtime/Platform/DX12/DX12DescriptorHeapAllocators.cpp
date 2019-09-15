#include "Runtime/Platform/DX12/DX12DescriptorHeapAllocators.h"
#include "Runtime/Public/Misc/Debug.h"
#include "Runtime/Public/Memory/RangeAllocator.h"
#include "Runtime/Platform/DX12/DX12Parameters.h"

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
	const size_t Size16K = 16 * 1024;
	void CreatePersistentDescriptorHeapAllocators(ID3D12Device* dev, IDX12DescriptorHeapAllocator* outAllocators[])
	{
		for (int j = 0; j < 2; ++j)
		{
			int base = j * (int)DescriptorHeapType::DescriptorHeapTypeCount;
			for (int i = 0; i < (int)DescriptorHeapType::DescriptorHeapTypeCount; ++i)
			{
				outAllocators[base + i] = CreateDX12BestFitDescriptorHeapAllocator(
					dev,
					DescriptorHeapType2D3D12_DESCRIPTOR_HEAP_TYPE[i],
					Size16K,
					Size16K,
					j != 0);
			}
		}
	}
	struct DescriptorHeapCreator
	{
		using RangePtr = UINT64;
		using RangeKey = ID3D12DescriptorHeap*;
		DescriptorHeapCreator() = default;
		DescriptorHeapCreator(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12Device* device, bool shaderVisible)
			: mDevice(device)
			, mHeapType(heapType)
			, mShaderVisible(shaderVisible)
		{}
		ID3D12DescriptorHeap* AllocRange(UINT64 size)
		{
			ID3D12DescriptorHeap* ret;
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = mHeapType;
			desc.NumDescriptors = (UINT)size;
			desc.Flags = mShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 0;
			CheckSucceeded(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&ret)));
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
		bool mShaderVisible;
	};

	class DX12LinearBlockDescriptorHeapAllocator : public IDX12DescriptorHeapAllocator
	{
	public:
		DX12LinearBlockDescriptorHeapAllocator(UINT64 blockSize, UINT64 maxReservation, UINT64 handleSize)
			: mImp(blockSize, maxReservation)
			, mHandleSize(handleSize)
		{}
		virtual DX12DescriptorHeapSpace1 Allocate(size_t count) override
		{ 
			ID3D12DescriptorHeap* heap;
			UINT64 offset;
			mImp.Allocate(heap, offset, count, 1);
			return DX12DescriptorHeapSpace1(heap, offset, mHandleSize, count);
		}
		virtual void Free(const DX12DescriptorHeapSpace1& space) override
		{
			mImp.Free(space.GetHeap(), space.GetOffset());
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
		size_t maxReservation,
		bool shaderVisible)
	{
		auto a = new DX12LinearBlockDescriptorHeapAllocator(
			blockSize, 
			maxReservation, 
			GetDX12RuntimeParameters().DescriptorHeapHandleSizes[heapType]);
		a->SetConfig(DescriptorHeapCreator{ heapType , dev, shaderVisible });
		return a;
	}

	class DX12BestFitDescriptorHeapAllocator : public IDX12DescriptorHeapAllocator
	{
	public:
		DX12BestFitDescriptorHeapAllocator(UINT64 blockSize, UINT64 maxReservation, UINT64 handleSize)
			: mImp(blockSize, maxReservation)
			, mHandleSize(handleSize)
		{}
		virtual DX12DescriptorHeapSpace1 Allocate(size_t count) override
		{
			ID3D12DescriptorHeap* heap;
			UINT64 offset;
			mImp.Allocate(heap, offset, count, 1);
			return DX12DescriptorHeapSpace1(heap, offset, mHandleSize, count);
		}
		virtual void Free(const DX12DescriptorHeapSpace1& space) override
		{
			mImp.Free(space.GetHeap(), space.GetOffset());
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
		size_t maxReservation,
		bool shaderVisible)
	{
		auto a = new DX12BestFitDescriptorHeapAllocator(
			blockSize,
			maxReservation,
			GetDX12RuntimeParameters().DescriptorHeapHandleSizes[heapType]);
		a->SetConfig(DescriptorHeapCreator{ heapType , dev, shaderVisible });
		return a;
	}
}