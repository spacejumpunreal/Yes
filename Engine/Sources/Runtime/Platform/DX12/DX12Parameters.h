#pragma once
#include "Yes.h"

#include <d3d12.h>

namespace Yes
{
	class DX12RuntimeParameters
	{
	public:
		size_t DescriptorHeapHandleSizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	};
	const DX12RuntimeParameters& GetDX12RuntimeParameters();
	void InitDX12RuntimeParameters(ID3D12Device* dev);
}