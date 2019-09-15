#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/SharedObject.h"
#include "Runtime/Platform/DX12/DX12RenderDeviceResources.h"
#include "Runtime/Platform/DX12/DX12ResourceSpace.h"

/*
#include "Runtime/Platform/DX12/d3dx12.h"
#include <d3d12.h>
#include <vector>
*/

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