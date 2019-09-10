#pragma once
#include "Runtime/Public/Yes.h"

#include "Runtime/Public/Graphics/RenderDevice.h"
#include "Runtime/Platform/DX12/DX12BufferAllocator.h"

namespace Yes
{
	class DX12ConstantBufferArgument : public RenderDeviceConstantBuffer
	{
	public:
		DX12ConstantBufferArgument(const DX12GPUBufferRegion& region, IDX12GPUBufferAllocator* allocator);
		void Destroy() override;
		void Apply(size_t slot, void* ctx) override;
		void Write(size_t offset, size_t size, void * content) override;
		size_t GetSize() override;
	private:
		DX12GPUBufferRegion mRegion;
		IDX12GPUBufferAllocator* mAllocator;
	};
	class DX12DescriptorHeap : public RenderDeviceDescriptorHeap
	{
	public:
		DX12DescriptorHeap(const DX12DescriptorHeapSpace1& space, IDX12DescriptorHeapAllocator* allocator);
		TRef<RenderDeviceDescriptorHeapRange> CreateRange(size_t offset, size_t length) override;
		size_t GetSize() override;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(size_t idx);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(size_t idx);
		void Destroy() override;
		void Apply(ID3D12GraphicsCommandList* list);
	private:
		DX12DescriptorHeapSpace1 mSpace;
		IDX12DescriptorHeapAllocator* mAllocator;
	};
	class DX12DescriptorTableArgument : public RenderDeviceDescriptorHeapRange
	{
	public:
		DX12DescriptorTableArgument(DX12DescriptorHeap* heap, size_t offset, size_t size);
		void Apply(size_t slot, void* ctx) override;
		void SetRange(size_t offset, size_t length, const RenderDeviceTexture* textures[]) override;
		size_t GetSize() override;
		TRef<RenderDeviceDescriptorHeap> GetDescriptorHeap() override;
	private:
		TRef<DX12DescriptorHeap> mHeap;
		size_t mOffset;
		size_t mSize;
	};
}