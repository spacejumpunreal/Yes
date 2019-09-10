#pragma once
#include "Runtime/Public/Yes.h"

#include "Runtime/Platform/DX12/DX12ResourceSpace.h"

namespace Yes
{
	class IDX12GPUBufferAllocator
	{
	public:
		virtual DX12GPUBufferRegion Allocate(UINT64 size, UINT64 alignment) = 0;
		virtual void Free(const DX12GPUBufferRegion& region) = 0;
		virtual void Reset() = 0;
		virtual void GetAllocationStats(size_t& count, size_t& used, size_t& total) = 0;
	};
	IDX12GPUBufferAllocator* CreateDX12LinearBlockBufferAllocator(ID3D12Device* device, UINT64 blockSize, UINT64 maxReservation);
}