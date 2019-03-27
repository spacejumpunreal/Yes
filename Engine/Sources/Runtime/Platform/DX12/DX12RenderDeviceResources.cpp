#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Platform/DX12/DX12MemAllocators.h"
#include "Platform/DXUtils.h"
#include "Graphics/RenderDevice.h"
#include "Misc/Utils.h"
#include "Misc/Debug.h"

namespace Yes
{


	//helper functions
	//IA layouts
	static std::pair<D3D12_INPUT_ELEMENT_DESC*, UINT> GetInputLayoutForVertexFormat(VertexFormat vf)
	{
		static D3D12_INPUT_ELEMENT_DESC VF_P3F_T2F_LAYOUT[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		switch (vf)
		{
		case VertexFormat::VF_P3F_T2F:
			return std::make_pair(VF_P3F_T2F_LAYOUT, (UINT)ARRAY_COUNT(VF_P3F_T2F_LAYOUT));
		default:
			CheckAlways(false, "unknown vertex format");
			return {};
		}
	}
	//texture format
	static DXGI_FORMAT GetTextureFormat(TextureFormat format)
	{
		if (false)
		{
		}
		else
		{
			return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
		}
	}

	DX12AsyncResourceCreator::DX12AsyncResourceCreator(ID3D12Device* dev)
		: mDevice(dev)
		, mFenceEvent(CreateEvent(nullptr, FALSE, FALSE, L"AsyncResourceCreator::mFenceEvent"))
		, mResourceWorker(Entry, this)
	{
		if (mFenceEvent == nullptr)
		{
			HRESULT_FROM_WIN32(GetLastError());
			CheckAlways(false);
		}
		for (int i = 0; i < (int)ResourceType::ResourceTypeCount; ++i)
		{
			mUploadTempBufferAllocator[i] = CreateDX12LinearBlockAllocator(
				HeapCreator(MemoryAccessCase::CPUUpload, dev, (ResourceType)i),
				AllocatorBlockSize);
			mPersistentAllocator[i] = CreateDX12FirstFitAllocator(
				HeapCreator(MemoryAccessCase::GPUAccessOnly, dev, (ResourceType)i),
				AllocatorBlockSize);
		}

		
	}
	void DX12AsyncResourceCreator::AddRequest(IDX12ResourceCreateRequest* request)
	{
		mStartQueue.PushBack(request);
	}
	void DX12AsyncResourceCreator::Entry(void* s)
	{
		auto self = (DX12AsyncResourceCreator*)s;
		self->Loop();
	}
	void DX12AsyncResourceCreator::Loop()
	{
		mExpectedFenceValue = 0;
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		CheckSucceeded(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCopyCommandQueue)));
		CheckSucceeded(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&mCommandAllocator)));
		CheckSucceeded(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, mCommandAllocator.GetPtr(), nullptr, IID_PPV_ARGS(&mCommandList)));
		CheckSucceeded(mCommandList->Close());
		CheckSucceeded(mDevice->CreateFence(mExpectedFenceValue, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&mFence)));

		std::vector<IDX12ResourceCreateRequest*> popResults;
		while (true)
		{
			mStartQueue.Pop(MAX_BATCH_SIZE, popResults);
			CheckSucceeded(mCommandList->Reset(mCommandAllocator.GetPtr(), nullptr));
			for (auto it = popResults.begin(); it != popResults.end(); ++it)
			{
				(*it)->StartCreation(this);
			}
			//send command list to gpu
			CheckSucceeded(mCommandList->Close());
			ID3D12CommandList* cmdList = mCommandList.GetPtr();
			mCopyCommandQueue->ExecuteCommandLists(1, &cmdList);
			{//wait for last operation to finish
				++mExpectedFenceValue;
				CheckSucceeded(mCopyCommandQueue->Signal(mFence.GetPtr(), mExpectedFenceValue));
				UINT64 readBack = mFence->GetCompletedValue();
				if (readBack != mExpectedFenceValue)
				{
					CheckSucceeded(mFence->SetEventOnCompletion(mExpectedFenceValue, mFenceEvent));
				}
			}
			CheckSucceeded(mCommandAllocator->Reset());
			{//finish them
				for (auto it = popResults.begin(); it != popResults.end(); ++it)
				{
					(*it)->FinishCreation(this);
				}
			}
			popResults.clear();
		}
	}

	//shader
	DX12RenderDeviceShader::DX12RenderDeviceShader(ID3D12Device* dev, const char* body, size_t size, const char* name)
	{
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		mVS = DoCompileShader(body, size, name, "VSMain", "vs_5_1", compileFlags);
		mPS = DoCompileShader(body, size, name, "PSMain", "ps_5_1", compileFlags);
		COMRef<ID3DBlob> blob;
		CheckSucceeded(D3DGetBlobPart(mPS->GetBufferPointer(), mPS->GetBufferSize(), D3D_BLOB_ROOT_SIGNATURE, 0, &blob));
		CheckSucceeded(dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}
	DX12RenderDeviceShader::~DX12RenderDeviceShader()
	{
		CheckAlways(false);//not suppose to delete shader
	}

	//pso
	DX12RenderDevicePSO::DX12RenderDevicePSO(ID3D12Device* dev, RenderDevicePSODesc& desc)
	{
		D3D12_INPUT_ELEMENT_DESC* layout;
		UINT count;
		std::tie(layout, count) = GetInputLayoutForVertexFormat(desc.VF);
		DX12RenderDeviceShader* shader = static_cast<DX12RenderDeviceShader*>(desc.Shader.GetPtr());

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { layout, count };
		psoDesc.pRootSignature = shader->mRootSignature.GetPtr();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(shader->mVS.GetPtr());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(shader->mPS.GetPtr());
		if (desc.StateKey != PSOStateKey::Default)
		{
		}
		else
		{
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = FALSE;
			psoDesc.DepthStencilState.StencilEnable = FALSE;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.SampleDesc.Count = 1;
		}
		psoDesc.NumRenderTargets = desc.RTCount;
		for (int i = 0; i < desc.RTCount; ++i)
		{
			psoDesc.RTVFormats[i] = GetTextureFormat(desc.RTs[i]);
		}
		psoDesc.SampleDesc.Count = 1;
		CheckSucceeded(dev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
	}
	//PSO
	void DX12RenderDevicePSO::Apply(ID3D12GraphicsCommandList* cmdList)
	{
		cmdList->SetGraphicsRootSignature(mRootSignature.GetPtr());
		cmdList->SetPipelineState(mPSO.GetPtr());
	}

	//Mesh
	class DX12RenderDeviceMeshCreateRequest : public IDX12ResourceCreateRequest
	{
	public:
		DX12RenderDeviceMeshCreateRequest(
			DX12RenderDeviceMesh* resource,
			ISharedBuffer* CPUData[2])
			: mResource(resource)
		{
			for (int i = 0; i < 2; ++i)
			{
				mCPUData[i] = CPUData[i];
			}
		}
		virtual void StartCreation(DX12AsyncResourceCreator* creator) override
		{
			ID3D12Device* device = creator->GetDevice();
			IDX12GPUMemoryAllocator& tempAllocator = creator->GetTempBufferAllocator(ResourceType::Buffer);
			IDX12GPUMemoryAllocator& persistentAllocator = creator->GetPersistentAllocator(ResourceType::Buffer);
			ID3D12GraphicsCommandList* commandList = creator->GetCommandList();
			DX12RenderDeviceMesh* mesh = mResource.GetPtr();
			DX12DeviceResourceWithMemoryRegion* resourceHandles[2] = {
				&mesh->mVertexBuffer,
				&mesh->mIndexBuffer,
			};
			for (int i = 0; i < 2; ++i)
			{
				CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(mCPUData[i]->GetSize());
				D3D12_RESOURCE_ALLOCATION_INFO info = device->GetResourceAllocationInfo(0, 1, &desc);
				mTempResource[i].MemoryRegion = tempAllocator.Allocate(info.SizeInBytes, info.Alignment);
				resourceHandles[i]->MemoryRegion = persistentAllocator.Allocate(info.SizeInBytes, info.Alignment);
				//create resources
				//temp buffer
				CheckSucceeded(device->CreatePlacedResource(
					mTempResource[i].MemoryRegion->GetHeap(),
					mTempResource[i].MemoryRegion->GetOffset(),
					&desc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&mTempResource[i].Resource)));
				//persistent buffer
				CheckSucceeded(device->CreatePlacedResource(
					resourceHandles[i]->MemoryRegion->GetHeap(),
					resourceHandles[i]->MemoryRegion->GetOffset(),
					&desc,
					D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&resourceHandles[i]->Resource)));
				//write to cpu accessable part
				void* bufferStart;
				CD3DX12_RANGE range(0, 0);
				CheckSucceeded(mTempResource[i].Resource->Map(0, &range, &bufferStart));
				memcpy(bufferStart, mCPUData[i]->GetData(), mCPUData[i]->GetSize());
				//copy on gpu then
				commandList->CopyBufferRegion(resourceHandles[i]->Resource, 0, mTempResource[i].Resource, 0, mCPUData[i]->GetSize());
				auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
					resourceHandles[i]->Resource,
					D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ);
				commandList->ResourceBarrier(1, &barrier);
			}
		}
		virtual void FinishCreation(DX12AsyncResourceCreator* creator) override
		{
			mResource->mIsReady = true;
			delete this;
		}
	private:
		TRef<DX12RenderDeviceMesh> mResource;
		DX12DeviceResourceWithMemoryRegion mTempResource[2];
		SharedBufferRef mCPUData[2];
	};

	DX12RenderDeviceMesh::DX12RenderDeviceMesh(DX12AsyncResourceCreator* creator, ISharedBuffer* buffers[2])
		: mIsReady(false)
	{
		DX12RenderDeviceMeshCreateRequest* req = new DX12RenderDeviceMeshCreateRequest(this, buffers);
		creator->AddRequest(req);
	}
	void DX12RenderDeviceMesh::Apply(ID3D12GraphicsCommandList* cmdList)
	{}

	DX12DeviceResourceWithMemoryRegion::DX12DeviceResourceWithMemoryRegion(ID3D12Resource* resource, const IDX12GPUMemoryRegion* region)
		: Resource(resource)
		, MemoryRegion(region)
	{}

	DX12DeviceResourceWithMemoryRegion::~DX12DeviceResourceWithMemoryRegion()
	{
		if (Resource)
		{
			Resource->Release();
		}
		if (MemoryRegion)
		{
			MemoryRegion->Free();
		}
	}

	DX12RenderDeviceRenderTarget::~DX12RenderDeviceRenderTarget()
	{
		mRenderTarget->Release();
	}
	void DX12RenderDeviceRenderTarget::TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList)
	{
		if (newState != mState)
		{
			cmdList->ResourceBarrier(
				1, 
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mRenderTarget,
					mState,
					newState));
			mState = newState;
		}
	}
}