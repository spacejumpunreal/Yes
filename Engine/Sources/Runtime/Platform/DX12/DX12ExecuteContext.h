#pragma once
#include "Yes.h"

#include <d3d12.h>
#include "Platform/DX12/d3dx12.h"
namespace Yes
{
	class IDX12RenderTarget;
	class IDX12DescriptorHeapAllocator;

	struct DX12RenderPassContext
	{
		ID3D12GraphicsCommandList* CommandList;
		IDX12DescriptorHeapAllocator* HeapAllocator;
		//IDX12RenderTarget*         Backbuffer;
		CD3DX12_VIEWPORT           DefaultViewport;
		CD3DX12_RECT			   DefaultScissor;

		//temp variables
		D3D12_GPU_DESCRIPTOR_HANDLE GlobalConstantBufferGPUAddress;
		size_t PassDescriptorHeapSize;
	};
}