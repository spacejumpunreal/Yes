#pragma once
#include "Yes.h"

#include "Public/Memory/ObjectPool.h"
#include "Public/Graphics/RenderDevice.h"
#include "Platform/DX12/DX12FrameState.h"

namespace Yes
{
	class DX12Mesh;
	class DX12PSO;
	class IDX12RenderTarget;
	class DX12DepthStencil;

	struct ICommandFactory
	{
		virtual RenderDeviceCommand* AllocCommand() = 0;
		virtual void FreeCommand(RenderDeviceCommand*) = 0;
	};
	class DX12RenderCommandPool
	{
	public:
		DX12RenderCommandPool();
		~DX12RenderCommandPool();
		RenderDeviceCommand* AllocCommand(RenderCommandType type);
		void FreeCommand(RenderDeviceCommand* command);
	private:
		ICommandFactory** mFactories;
	};
	class DX12Drawcall : public RenderDeviceDrawcall
	{
	public:
		void Reset() override;
		void SetMesh(RenderDeviceMesh* mesh) override;
		void SetTextures(int idx, RenderDeviceTexture* texture) override;
		void SetConstantBuffer(void* data, size_t size, RenderDevicePass* pass);
		void SetPSO(RenderDevicePSO* pso) override;
		void Prepare(void* ctx) override;
		void Execute(void* ctx) override;
	public:
		TRef<DX12Mesh> Mesh;
		AllocatedCBV ConstantBuffer;
		TRef<DX12PSO> PSO;
	};
}
