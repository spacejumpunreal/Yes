#include "Platform/DX12/DX12ResourceSpace.h"
#include "Public/Misc/Debug.h"

#include "Platform/DX12/d3dx12.h"
#include <algorithm>

Yes::DX12GPUBufferRegion::DX12GPUBufferRegion(ID3D12Resource* buffer, UINT64 offset, UINT64 size)
	: mOffset(offset)
	, mSize(size)
	, mResource(buffer)
{
}

Yes::DX12GPUBufferRegion::DX12GPUBufferRegion()
	: mResource(nullptr)
	, mOffset(0)
	, mSize(0)
{
}

void Yes::DX12GPUBufferRegion::Write(void* src, UINT64 size)
{
	size = std::min(size, mSize);
	void* wptr;
	CheckSucceeded(mResource->Map(0, nullptr, &wptr));
	wptr = (unsigned char*)wptr + mOffset;
	std::memcpy(wptr, src, size);
	CD3DX12_RANGE wrange = CD3DX12_RANGE(mOffset, mOffset + size);
	mResource->Unmap(0, &wrange);
}

UINT64 Yes::DX12GPUBufferRegion::GetGPUAddress()
{
	return mResource->GetGPUVirtualAddress() + mOffset;
}
