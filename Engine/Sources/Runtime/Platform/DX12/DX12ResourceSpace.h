#pragma once
#include "Yes.h"
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
		DX12DescriptorHeapSpace1(ID3D12DescriptorHeap* heap, UINT64 offset, UINT64 stepSize)
			: mHeap(heap)
			, mOffset(offset)
			, mStepSize(stepSize)
		{}
		DX12DescriptorHeapSpace1& operator=(DX12DescriptorHeapSpace1&& other)
		{
			mHeap = other.mHeap;
			mOffset = other.mOffset;
			mStepSize = other.mStepSize;
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
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(int slotIndex) const
		{
			D3D12_CPU_DESCRIPTOR_HANDLE ret;
			ret.ptr = (mOffset + slotIndex) * mStepSize + mHeap->GetCPUDescriptorHandleForHeapStart().ptr;
			return ret;
		}
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(int slotIndex) const
		{
			D3D12_GPU_DESCRIPTOR_HANDLE ret;
			ret.ptr = (mOffset + slotIndex) * mStepSize + mHeap->GetGPUDescriptorHandleForHeapStart().ptr;
			return ret;
		}

	private:
		ID3D12DescriptorHeap* mHeap;
		UINT64 mOffset;
		UINT64 mStepSize;

		friend class DX12LinearBlockDescriptorHeapAllocator;
		friend class DX12BestFitDescriptorHeapAllocator;
	};
}