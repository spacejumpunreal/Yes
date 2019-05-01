#include "Platform/DX12/DX12RenderCommand.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Platform/DX12/DX12ExecuteContext.h"
#include "Platform/DX12/DX12RenderPass.h"
#include "Public/Memory/ObjectPool.h"
#include "Public/Misc/Utils.h"
#include "Public/Memory/SizeUtils.h"

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
	#include "Graphics/RenderCommandTypeList.inl"
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
		Mesh = nullptr;
		ConstantBuffer = {};
		PSO = nullptr;
		Textures.clear();
	}
	void DX12Drawcall::SetMesh(RenderDeviceMesh* mesh)
	{
		Mesh = mesh;
	}
	void DX12Drawcall::SetTextures(int idx, RenderDeviceTexture* texture)
	{
		DX12Texture2D* tex = dynamic_cast<DX12Texture2D*>(texture);
		if (idx >= Textures.size())
		{
			Textures.resize(idx + 1);
		}
		Textures[idx] = tex;
	}
	void DX12Drawcall::SetConstantBuffer(void* data, size_t size, RenderDevicePass* pass)
	{
		DX12Pass* dx12Pass = dynamic_cast<DX12Pass*>(pass);
		DX12FrameState* state = dx12Pass->GetFrameState();
		const size_t SizeAlign = 4;
		size_t alignedSize = AlignSize(size, SizeAlign);
		CheckAlways(alignedSize == size);
		ConstantBuffer = state->GetConstantBufferAllocator()->Allocate(alignedSize, SizeAlign);
		ConstantBuffer.Write(data, size);
	}
	void DX12Drawcall::SetPSO(RenderDevicePSO* pso)
	{
		PSO = pso;
	}
	void DX12Drawcall::Prepare(void* ctx)
	{
		DX12RenderPassContext* context = (DX12RenderPassContext*)ctx;
		context->StartDescritorTable();
		for (int i = 0; i < Textures.size(); ++i)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE handle = Textures[i]->GetCPUHandle(TextureUsage::ShaderResource);
			context->AddDescriptor(&handle, 1);
		}
	}
	void DX12Drawcall::Execute(void * ctx)
	{
		DX12RenderPassContext* context = (DX12RenderPassContext*)ctx;
		ID3D12GraphicsCommandList* gl = context->CommandList;
		PSO->Apply(gl);
		//global constant buffer
		gl->SetGraphicsRootConstantBufferView(0, context->GlobalConstantBufferGPUAddress.ptr);
		//local constant buffer
		gl->SetGraphicsRootConstantBufferView(1, ConstantBuffer.GetGPUAddress());
		//global textures
		if (context->GetGlobalDescriptorTableSize() > 0)
		{
			gl->SetGraphicsRootDescriptorTable(2, context->GetGlobalDescriptorTableHandle());
		}
		//local textures
		size_t len = context->GetNextHeapRangeLength();
		size_t idx = (int)context->GetNextHeapRangeStart();
		if (len > 0)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE handle = context->GetHeapSpace().GetGPUHandle((int)idx);
			gl->SetGraphicsRootDescriptorTable(3, handle);
		}
		Mesh->Apply(gl);
		UINT ic = Mesh->GetIndexCount();
		gl->DrawIndexedInstanced(ic, 1, 0, 0, 0);
	}
	//DX12Drawcall
	size_t DX12Drawcall::GetDescriptorHeapSlotCount()
	{
		return Textures.size();
	}
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