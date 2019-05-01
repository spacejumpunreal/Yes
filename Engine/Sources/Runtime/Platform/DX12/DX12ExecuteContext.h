#pragma once
#include "Yes.h"
#include "Platform/DX12/DX12ResourceSpace.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Misc/SharedObject.h"

#include <d3d12.h>
#include "Platform/DX12/d3dx12.h"
#include <vector>

namespace Yes
{
	class IDX12RenderTarget;
	class IDX12DescriptorHeapAllocator;

	struct DX12RenderPassContext
	{
	public:
		DX12RenderPassContext(
			ID3D12GraphicsCommandList* cmdList,
			const CD3DX12_VIEWPORT& viewport,
			const CD3DX12_RECT& scissor,
			const D3D12_GPU_DESCRIPTOR_HANDLE& globalConstantBufferAddress,
			DX12DescriptorHeapSpace1& srvHeap,
			size_t srvHeapSize,
			ID3D12Device* dev);
		void StartDescritorTable();
		void AddDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* handle, size_t count);
		const DX12DescriptorHeapSpace1& GetHeapSpace() { return mSRVHeap; }
		size_t GetNextHeapRangeStart();
		size_t GetNextHeapRangeLength();
		void Prepare(TRef<DX12Texture2D> textures[], size_t count);
		size_t GetGlobalDescriptorTableSize() { return mGlobalDescriptorTableSize; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetGlobalDescriptorTableHandle() { return mGlobalDescriptorTableHandle; }
	public:
		ID3D12GraphicsCommandList*			CommandList;
		const CD3DX12_VIEWPORT				DefaultViewport;
		const CD3DX12_RECT					DefaultScissor;
		const D3D12_GPU_DESCRIPTOR_HANDLE	GlobalConstantBufferGPUAddress;
	private:
		DX12DescriptorHeapSpace1 mSRVHeap;
		size_t mSRVHeapSize;
		ID3D12Device* mDevice;
		std::vector<size_t> mHeapOffsets;
		size_t mWriteOffset;
		size_t mReadIndex;
		size_t mGlobalDescriptorTableSize;
		D3D12_GPU_DESCRIPTOR_HANDLE mGlobalDescriptorTableHandle;
	};
}