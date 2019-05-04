#include "Platform/DX12/DX12ExecuteContext.h"
#include "Platform/DX12/DX12Parameters.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Platform/DX12/DX12RenderPass.h"
#include "Misc/Debug.h"
#include "d3d12.h"

namespace Yes
{
	DX12RenderPassContext::DX12RenderPassContext(ID3D12GraphicsCommandList* cmdList, DX12RenderPass* pass)
		: CommandList(cmdList)
		, Pass(pass)
	{}
}

