#include "Runtime/Platform/DX12/DX12Device.h"
#include "Runtime/Public/Misc/Debug.h"

namespace Yes
{
	static ID3D12Device* GDX12Device;
	void RegisterDX12Device(ID3D12Device * dev)
	{
		CheckAlways(GDX12Device == nullptr);
		GDX12Device = dev;
	}
	ID3D12Device* GetDX12Device()
	{
		return GDX12Device;
	}
}

