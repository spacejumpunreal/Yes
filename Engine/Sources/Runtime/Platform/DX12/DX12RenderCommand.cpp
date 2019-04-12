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
		mFactories[(int)command->CommandType]->FreeCommand(command);
	}

	//DX12RenderDeviceDrawcall
	void DX12Drawcall::Reset()
	{
		Mesh = nullptr;
		ConstantBuffer = {};
		PSO = nullptr;
	}
	void DX12Drawcall::SetMesh(RenderDeviceMesh* mesh)
	{
		Mesh = mesh;
	}
	void DX12Drawcall::SetTextures(int idx, RenderDeviceTexture* texture)
	{}
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
		IDX12DescriptorHeapAllocator* allocator = context->HeapAllocator;
		//CBV
		//DX12DescriptorHeapSpace space = allocator->Allocate(1);
		//SRV
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
		Mesh->Apply(gl);
		UINT ic = Mesh->GetIndexCount();
		gl->DrawIndexedInstanced(ic, 1, 0, 0, 0);
	}
}