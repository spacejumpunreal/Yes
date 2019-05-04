#pragma once
#include "Yes.h"

struct ID3D12Device;

namespace Yes
{
	void RegisterDX12Device(ID3D12Device* dev);
	ID3D12Device* GetDX12Device();
}