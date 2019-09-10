#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Memory/RangeAllocator.h"
#include "Runtime/Platform/DX12/DX12BufferAllocator.h"
#include "Runtime/Platform/DX12/d3dx12.h"

namespace Yes
{
	struct BufferCreator
	{
	public:
		using RangePtr = UINT64;
		using RangeKey = ID3D12Resource*;
		ID3D12Resource* AllocRange(UINT64 size)
		{
			ID3D12Resource* res;
			CheckSucceeded(Device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(size),
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&res)));
			return res;
		}
		void FreeRange(ID3D12Resource* buffer)
		{
			buffer->Release();
		}
	public:
		ID3D12Device* Device;
	};

	class DX12LinearBlockBufferAllocator : public IDX12GPUBufferAllocator
	{
	public:
		DX12LinearBlockBufferAllocator(ID3D12Device* device, UINT64 blockSize, UINT64 maxReservation)
			: mImp(blockSize, maxReservation)
		{
			mImp.Device = device;
		}
		DX12GPUBufferRegion Allocate(UINT64 size, UINT64 alignment) override
		{
			ID3D12Resource* buffer;
			UINT64 start;
			mImp.Allocate(buffer, start, size, alignment);
			return DX12GPUBufferRegion(buffer, start, size);
		}
		void Free(const DX12GPUBufferRegion& region) override
		{
			CheckAlways(false, "no need to free");
		}
		void Reset() override
		{
			mImp.Reset();
		}
		void GetAllocationStats(size_t& count, size_t& used, size_t& total) override
		{
			return mImp.Stats(count, used, total);
		}
	private:
		LinearBlockRangeAllocator<ID3D12Resource*, UINT64, BufferCreator> mImp;
	};

	IDX12GPUBufferAllocator* CreateDX12LinearBlockBufferAllocator(ID3D12Device* device, UINT64 blockSize, UINT64 maxReservation)
	{
		return new DX12LinearBlockBufferAllocator(device, blockSize, maxReservation);
	}
}