#include "Platform/DX12/DX12ExecuteContext.h"
#include "Platform/DX12/DX12Parameters.h"
#include "Misc/Debug.h"
#include "d3d12.h"

namespace Yes
{
	DX12RenderPassContext::DX12RenderPassContext(
		ID3D12GraphicsCommandList* cmdList,
		const CD3DX12_VIEWPORT& viewport,
		const CD3DX12_RECT& scissor,
		const D3D12_GPU_DESCRIPTOR_HANDLE& globalConstantBufferAddress,
		DX12DescriptorHeapSpace1& srvHeap,
		size_t srvHeapSize,
		ID3D12Device* dev)
		: CommandList(cmdList)
		, DefaultViewport(viewport)
		, DefaultScissor(scissor)
		, GlobalConstantBufferGPUAddress(globalConstantBufferAddress)
		, mSRVHeap(srvHeap)
		, mSRVHeapSize(srvHeapSize)
		, mDevice(dev)
		, mWriteOffset(0)
		, mReadIndex(0)
	{}
	void DX12RenderPassContext::StartDescritorTable()
	{
		mHeapOffsets.push_back(mWriteOffset);
	}
	void DX12RenderPassContext::AddDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* handle, size_t count)
	{
		CheckAlways(mWriteOffset + count <= mSRVHeapSize);
		mDevice->CopyDescriptorsSimple(
			(UINT)count, 
			mSRVHeap.GetCPUHandle((int)mWriteOffset), 
			*handle, 
			D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		mWriteOffset += count;
	}
	size_t DX12RenderPassContext::GetNextHeapRangeStart()
	{ 
		return mHeapOffsets[mReadIndex++]; 
	}
	size_t DX12RenderPassContext::GetNextHeapRangeLength()
	{
		if (mReadIndex == mHeapOffsets.size() - 1)
		{
			return mSRVHeapSize - mHeapOffsets[mReadIndex];
		}
		else
		{
			return mHeapOffsets[mReadIndex + 1] - mHeapOffsets[mReadIndex];
		}
	}
}

