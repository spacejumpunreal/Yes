#include "Platform/DX12/DX12Parameters.h"

namespace Yes
{
	static DX12RuntimeParameters GDX12RuntimeParameters;
	const DX12RuntimeParameters& GetDX12RuntimeParameters()
	{
		return GDX12RuntimeParameters;
	}
	void InitDX12RuntimeParameters(ID3D12Device * dev)
	{
		for (int i = 0; i < (int)D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			GDX12RuntimeParameters.DescriptorHeapHandleSizes[i] =
				dev->GetDescriptorHandleIncrementSize((D3D12_DESCRIPTOR_HEAP_TYPE)i);
		}
	}
}