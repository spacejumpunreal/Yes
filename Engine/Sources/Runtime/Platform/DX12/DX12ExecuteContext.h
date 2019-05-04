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
	class DX12RenderPass;

	struct DX12RenderPassContext
	{
	public:
		DX12RenderPassContext(ID3D12GraphicsCommandList* cmdList, DX12RenderPass* pass);
		ID3D12GraphicsCommandList* CommandList;
		DX12RenderPass* Pass;
	};
}