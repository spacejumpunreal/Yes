#include "Runtime/Platform/DX12/DX12ResourceSpace.h"
#include "Runtime/Public/Misc/Debug.h"

#include "Runtime/Platform/DX12/d3dx12.h"
#include <algorithm>


namespace Yes
{
	D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapSpace1::GetCPUHandle(size_t slotIndex) const
	{
		CheckAlways(0 <= slotIndex && slotIndex < mCount);
		D3D12_CPU_DESCRIPTOR_HANDLE ret;
		ret.ptr = (mOffset + slotIndex) * mStepSize + mHeap->GetCPUDescriptorHandleForHeapStart().ptr;
		return ret;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeapSpace1::GetGPUHandle(size_t slotIndex) const
	{
		CheckAlways(0 <= slotIndex && slotIndex < mCount);
		D3D12_GPU_DESCRIPTOR_HANDLE ret;
		ret.ptr = (mOffset + slotIndex) * mStepSize + mHeap->GetGPUDescriptorHandleForHeapStart().ptr;
		return ret;
	}

	DX12GPUBufferRegion::DX12GPUBufferRegion(ID3D12Resource* buffer, UINT64 offset, UINT64 size)
	: mOffset(offset)
	, mSize(size)
	, mResource(buffer)
	{
	}
	DX12GPUBufferRegion::DX12GPUBufferRegion()
		: mResource(nullptr)
		, mOffset(0)
		, mSize(0)
	{
	}
	void DX12GPUBufferRegion::Write(void* src, UINT64 size, UINT64 offset)
	{
		CheckAlways(offset <= mSize && offset + size <= mSize);
		D3D12_RANGE dummyRange {0, 0};
		void* wptr;
		CheckSucceeded(mResource->Map(0, &dummyRange, &wptr));
		uint8* wStart = ((uint8*)wptr) + mOffset + offset;
		std::memcpy(wStart, src, size);
		CD3DX12_RANGE wrange = CD3DX12_RANGE(mOffset + offset, mOffset + offset + size);
		mResource->Unmap(0, &wrange);
	}

	UINT64 DX12GPUBufferRegion::GetGPUAddress()
	{
		if (IsValid())
			return mResource->GetGPUVirtualAddress() + mOffset;
		else
			return 0;
	}
}

