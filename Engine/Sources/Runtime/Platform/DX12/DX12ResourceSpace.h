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

	struct DX12DescriptorHeapSpace
	{
		ID3D12DescriptorHeap* Heap;
		UINT64 Offset;
		bool IsValid() const
		{
			return Heap != nullptr;
		}
	};
}