#include "Runtime/Platform/DX12/DX12ExecuteContext.h"
#include "Runtime/Public/Misc/Debug.h"
#include "Runtime/Platform/DX12/DX12Parameters.h"
#include "Runtime/Platform/DX12/DX12RenderDeviceResources.h"
#include "Runtime/Platform/DX12/DX12RenderPass.h"

namespace Yes
{
	DX12RenderPassContext::DX12RenderPassContext(ID3D12GraphicsCommandList* cmdList, DX12RenderPass* pass)
		: CommandList(cmdList)
		, Pass(pass)
	{}
}

