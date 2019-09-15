#include "Runtime/Platform/DX12/DX12DrawcallArgument.h"
#include "Runtime/Public/Graphics/RenderDevice.h"
#include "Runtime/Platform/DX12/DX12Device.h"
#include "Runtime/Platform/DX12/DX12ExecuteContext.h"

namespace Yes
{
	//DX12ConstantBufferArgument
	DX12ConstantBufferArgument::DX12ConstantBufferArgument(const DX12GPUBufferRegion& region, IDX12GPUBufferAllocator* allocator)
		: mRegion(region)
		, mAllocator(allocator)
	{}
	void DX12ConstantBufferArgument::Destroy()
	{
		if (mAllocator != nullptr)
		{
			mAllocator->Free(mRegion);
		}
		SharedObject::Destroy();//will call delete this
	}
	void DX12ConstantBufferArgument::Apply(size_t slot, void* ctx)
	{
		DX12RenderPassContext* context = (DX12RenderPassContext*)ctx;
		ID3D12GraphicsCommandList* gl = context->CommandList;
		gl->SetGraphicsRootConstantBufferView((UINT)slot, mRegion.GetGPUAddress());
	}
	void DX12ConstantBufferArgument::Write(size_t offset, size_t size, void * content)
	{
		size_t sz = mRegion.GetSize();
		CheckAlways(offset <= sz && offset + size <= sz);
		mRegion.Write(content, size, offset);
	}
	size_t DX12ConstantBufferArgument::GetSize()
	{
		return mRegion.GetSize();
	}
	//DX12DescriptorHeap
	DX12DescriptorHeap::DX12DescriptorHeap(const DX12DescriptorHeapSpace1& space, IDX12DescriptorHeapAllocator* allocator)
		: mSpace(space)
		, mAllocator(allocator)
	{
	}
	TRef<RenderDeviceDescriptorHeapRange> DX12DescriptorHeap::CreateRange(size_t offset, size_t length)
	{
		auto* ret = new DX12DescriptorTableArgument(this, offset, length);
		return ret;
	}
	size_t DX12DescriptorHeap::GetSize()
	{
		return mSpace.GetSize();
	}
	D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetGPUHandle(size_t idx)
	{
		return mSpace.GetGPUHandle(idx);
	}
	D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeap::GetCPUHandle(size_t idx)
	{
		return mSpace.GetCPUHandle(idx);
	}
	void DX12DescriptorHeap::Destroy()
	{
		if (mAllocator != nullptr)
		{
			mAllocator->Free(mSpace);
		}
		SharedObject::Destroy();
	}
	void DX12DescriptorHeap::Apply(ID3D12GraphicsCommandList* list)
	{
		ID3D12DescriptorHeap* heap = mSpace.GetHeap();
		list->SetDescriptorHeaps(1, &heap);
	}
	//DX12ConstantBufferArgument
	DX12DescriptorTableArgument::DX12DescriptorTableArgument(DX12DescriptorHeap* heap, size_t offset, size_t size)
		: mHeap(heap)
		, mOffset(offset)
		, mSize(size)
	{}
	void DX12DescriptorTableArgument::Apply(size_t slot, void* ctx)
	{
		DX12RenderPassContext* context = (DX12RenderPassContext*)ctx;
		ID3D12GraphicsCommandList* gl = context->CommandList;
		gl->SetGraphicsRootDescriptorTable((UINT)slot, mHeap->GetGPUHandle(mOffset));
	}
	void DX12DescriptorTableArgument::SetRange(size_t offset, size_t length, const RenderDeviceTexture* textures[])
	{
		ID3D12Device* dev = GetDX12Device();
		CheckAlways(offset <= mSize && offset + length <= mSize);
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;
		std::vector<UINT> rangeLength;
		UINT destRangeLength = (UINT)length;
		for (size_t i = 0; i < length; ++i)
		{
			auto* tex = (DX12Texture2D*)textures[i];
			handles.push_back(tex->GetCPUHandle(TextureUsage::ShaderResource));
			rangeLength.push_back(1);
		}
		D3D12_CPU_DESCRIPTOR_HANDLE dst = mHeap->GetCPUHandle(mOffset);
		dev->CopyDescriptors(
			1, 
			&dst, 
			&destRangeLength, 
			(UINT)handles.size(), 
			handles.data(), 
			rangeLength.data(), 
			D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	size_t DX12DescriptorTableArgument::GetSize()
{
	return mSize;
}

	TRef<RenderDeviceDescriptorHeap> DX12DescriptorTableArgument::GetDescriptorHeap()
	{
		return mHeap.GetPtr();
	}



#if false
{
	size_t sz = mRegion.GetSize();
	CheckAlways(offset <= sz && offset + length <= sz);
	UINT destRangeLength = (UINT)length;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;
	std::vector<UINT> rangeLength;
	for (size_t i = 0; i < length; ++i)
	{
		auto* tex = (DX12Texture2D*)textures[i];
		handles.push_back(tex->GetCPUHandle(TextureUsage::ShaderResource));
		rangeLength.push_back(1);
	}
	mDevice->CopyDescriptors(
		1, 
		&mRegion.GetCPUHandle(offset), 
		&destRangeLength, 
		destRangeLength, 
		handles.data(), 
		rangeLength.data(), 
		D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
#endif
}