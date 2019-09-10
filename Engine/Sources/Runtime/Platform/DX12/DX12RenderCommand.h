#pragma once
#include "Runtime/Public/Yes.h"

#include "Runtime/Public/Memory/ObjectPool.h"
#include "Runtime/Public/Graphics/RenderDevice.h"
#include "Runtime/Platform/DX12/DX12FrameState.h"
#include "Runtime/Platform/DX12/DX12ResourceSpace.h"

namespace Yes
{
	class DX12Mesh;
	class DX12PSO;
	class IDX12RenderTarget;
	class DX12DepthStencil;
	class IDX12ShaderReadableTexture;
	class DX12DescriptorHeap;

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
		void Prepare(void* ctx) override;
		void Execute(void* ctx) override;

		void SetPSO(RenderDevicePSO* pso) override;
		void SetMesh(RenderDeviceMesh* mesh) override;
		void SetArgument(int slot, RenderDeviceDrawcallArgument* arg) override;
		void SetDescriptorHeap(RenderDeviceDescriptorHeap* descriptorHeap) override;
	private:
		TRef<DX12PSO> mPSO;
		TRef<DX12Mesh> mMesh;
		std::vector<TRef<RenderDeviceDrawcallArgument>> mArgs;
		TRef<DX12DescriptorHeap> mDescriptorHeap;
	};
	class DX12Barrier : public RenderDeviceBarrier
	{
	public:
		void Reset() override;
		void Prepare(void* ctx) override {}
		void Execute(void* ctx) override;

		void AddResourceBarrier(const TRef<RenderDeviceResource>& resources, RenderDeviceResourceState newState) override;
	public:
		struct ResourceBarrierItem
		{
			TRef<RenderDeviceResource> Resource;
			RenderDeviceResourceState NewState;
		};
		std::vector<ResourceBarrierItem> mResourceBarriers;
	};
}

