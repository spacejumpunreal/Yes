#pragma once
#include "Runtime/Public/Yes.h"
#include "Windows.h"
#include <d3d12.h>

namespace Yes
{
	enum class ResourceType
	{
		Buffer,
		Texture,
		RenderTarget,
		DepthStencil,
		ResourceTypeCount,
	};

	struct DX12GPUMemoryRegion
	{
		ID3D12Heap* Heap;
		UINT64 Offset;
		bool IsValid() const
		{
			return Heap != nullptr;
		}
	};

	struct DX12DescriptorHeapSpace1
	{
	public:
		DX12DescriptorHeapSpace1(const DX12DescriptorHeapSpace1& other) = default;
		DX12DescriptorHeapSpace1(DX12DescriptorHeapSpace1&& other) = default;
		DX12DescriptorHeapSpace1(ID3D12DescriptorHeap* heap, UINT64 offset, UINT64 stepSize, size_t count)
			: mHeap(heap)
			, mOffset(offset)
			, mStepSize(stepSize)
			, mCount(count)
		{}
		DX12DescriptorHeapSpace1& operator=(DX12DescriptorHeapSpace1&& other)
		{
			mHeap = other.mHeap;
			mOffset = other.mOffset;
			mStepSize = other.mStepSize;
			mCount = other.mCount;
			return *this;
		}
		DX12DescriptorHeapSpace1()
			: mHeap(nullptr)
		{}
		bool IsValid() const
		{
			return mHeap != nullptr;
		}
		ID3D12DescriptorHeap* GetHeap() const
		{
			return mHeap;
		}
		UINT64 GetOffset() const
		{
			return mOffset;
		}
		size_t GetSize() const
		{
			return mCount;
		}
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(size_t slotIndex) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(size_t slotIndex) const;

	private:
		ID3D12DescriptorHeap* mHeap;
		UINT64 mOffset;
		UINT64 mStepSize;
		size_t mCount;

		friend class DX12LinearBlockDescriptorHeapAllocator;
		friend class DX12BestFitDescriptorHeapAllocator;
	};

	struct DX12GPUBufferRegion
	{
	public:
		DX12GPUBufferRegion(ID3D12Resource* buffer, UINT64 offset, UINT64 size);
		DX12GPUBufferRegion(const DX12GPUBufferRegion& other) = default;
		DX12GPUBufferRegion& operator=(const DX12GPUBufferRegion& other) = default;
		DX12GPUBufferRegion& operator=(DX12GPUBufferRegion&& other) = default;
		DX12GPUBufferRegion();
		void Write(void* src, UINT64 size, UINT64 offset);
		bool IsValid() { return mResource != nullptr; }
		size_t GetSize() { return mSize; }
		UINT64 GetGPUAddress();
	private:
		UINT64 mOffset;
		UINT64 mSize;
		ID3D12Resource* mResource;
	};
}