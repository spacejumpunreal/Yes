#include "Runtime/Platform/DX12/DX12RenderCommand.h"
#include "Runtime/Public/Misc/Utils.h"
#include "Runtime/Public/Memory/ObjectPool.h"
#include "Runtime/Public/Memory/SizeUtils.h"
#include "Runtime/Platform/DX12/DX12DrawcallArgument.h"
#include "Runtime/Platform/DX12/DX12ExecuteContext.h"
#include "Runtime/Platform/DX12/DX12RenderDeviceResources.h"
#include "Runtime/Platform/DX12/DX12RenderPass.h"

namespace Yes
{
	//command factory, pool related
	template<typename T>
	struct DX12CommandFactory : ICommandFactory
	{
	public:
		DX12CommandFactory()
			: mPool(1024)
		{}
		RenderDeviceCommand* AllocCommand() override
		{
			return mPool.Allocate();
		}
		virtual void FreeCommand(RenderDeviceCommand* c) override
		{
			mPool.Deallocate((T*)c);
		}
	private:
		ObjectPool<T> mPool;
	};

	DX12RenderCommandPool::DX12RenderCommandPool()
	{
		static ICommandFactory* factories[] =
		{
	#define DefineLabel(label) new DX12CommandFactory<DX12##label>()
	#include "Runtime/Graphics/RenderCommandTypeList.inl"
	#undef DefineLabel
		};
		mFactories = factories;
	}
	DX12RenderCommandPool::~DX12RenderCommandPool()
	{}
	RenderDeviceCommand * DX12RenderCommandPool::AllocCommand(RenderCommandType type)
	{
		return mFactories[(int)type]->AllocCommand();
	}
	void DX12RenderCommandPool::FreeCommand(RenderDeviceCommand * command)
	{
		mFactories[(int)command->GetCommandType()]->FreeCommand(command);
	}

	//DX12Drawcall
	void DX12Drawcall::Reset()
	{
		mMesh = nullptr;
		mArgs.clear();
		mPSO = nullptr;
		mDescriptorHeap = nullptr;
	}
	void DX12Drawcall::SetMesh(RenderDeviceMesh* mesh)
	{
		mMesh = mesh;
	}
	void DX12Drawcall::SetArgument(int slot, RenderDeviceDrawcallArgument* arg)
	{
		if (slot >= mArgs.size())
		{
			mArgs.resize(slot + 1);
		}
		mArgs[slot] = arg;
	}
	void DX12Drawcall::SetDescriptorHeap(RenderDeviceDescriptorHeap * descriptorHeap)
	{
		mDescriptorHeap = descriptorHeap;
	}
	void DX12Drawcall::SetPSO(RenderDevicePSO* pso)
	{
		mPSO = pso;
	}
	void DX12Drawcall::Prepare(void* /*ctx*/)
	{
	}
	void DX12Drawcall::Execute(void* ctx)
	{
		DX12RenderPassContext* context = (DX12RenderPassContext*)ctx;
		ID3D12GraphicsCommandList* gl = context->CommandList;
		mPSO->Apply(gl);
		if (mDescriptorHeap != nullptr)
		{
			mDescriptorHeap->Apply(gl);
		}
		context->Pass->PreparePassArgs(context);
		size_t baseSlot = context->Pass->GetPassArgCount();
		for (size_t i = 0; i < mArgs.size(); ++i)
		{
			mArgs[i]->Apply(baseSlot + i, ctx);
		}
		mMesh->Apply(gl);
		UINT ic = mMesh->GetIndexCount();
		gl->DrawIndexedInstanced(ic, 1, 0, 0, 0);
	}
	//DX12Drawcall
	void DX12Barrier::Reset()
	{
		mResourceBarriers.clear();
	}
	void DX12Barrier::Execute(void* ctx)
	{
		DX12RenderPassContext* context = (DX12RenderPassContext*)ctx;
		ID3D12GraphicsCommandList* gl = context->CommandList;
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		for (int i = 0; i < mResourceBarriers.size(); ++i)
		{
			auto* devResource = (ID3D12Resource*)mResourceBarriers[i].Resource->GetTransitionTarget();
			D3D12_RESOURCE_STATES prevState = StateAbstract2Device(mResourceBarriers[i].Resource->GetState());
			D3D12_RESOURCE_STATES newState = StateAbstract2Device(mResourceBarriers[i].NewState);
			if (prevState != newState)
			{
				barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
					devResource,
					prevState,
					newState));
				mResourceBarriers[i].Resource->SetState(mResourceBarriers[i].NewState);
			}
		}
		if (barriers.size() > 0)
		{
			gl->ResourceBarrier((UINT)barriers.size(), barriers.data());
		}
	}
	void DX12Barrier::AddResourceBarrier(const TRef<RenderDeviceResource>& resource, RenderDeviceResourceState newState)
	{
		mResourceBarriers.push_back({ resource, newState });
	}
}