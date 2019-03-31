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

	struct DX12ReleaseSpaceRequest : public IDX12ResourceRequest
	{
		DX12ReleaseSpaceRequest(ResourceType resourceType)
			: mResourceType(resourceType)
		{}
		void AddMemoryRegions(const DX12GPUMemoryRegion* regions, size_t count)
		{
			for (int i = 0; i < count; ++i)
			{
				mMemoryRegions[i] = regions[i];
			}
		}
		void AddDescriptorSpaces(const DX12DescriptorHeapSpace* spaces, size_t count)
		{
			for (int i = 0; i < count; ++i)
			{
				mDescriptorSpaces[i] = spaces[i];
			}
		}
		virtual void StartRequest(DX12DeviceResourceManager* creator)
		{
			IDX12GPUMemoryAllocator& mem = creator->GetAsyncPersistentAllocator(mResourceType);
			for (auto& region : mMemoryRegions)
			{
				mem.Free(region);
			}

		}
		virtual void FinishRequest(DX12DeviceResourceManager* creator)
		{}
	private:
		ResourceType mResourceType;
		std::deque<DX12GPUMemoryRegion> mMemoryRegions;
		std::deque<DX12DescriptorHeapSpace> mDescriptorSpaces;
	};

	DX12DeviceResourceManager* DX12DeviceResourceManager::Instance;
	DX12DeviceResourceManager::DX12DeviceResourceManager(ID3D12Device* dev)
		: mDevice(dev)
		, mFenceEvent(CreateEvent(nullptr, FALSE, FALSE, L"AsyncResourceCreator::mFenceEvent"))
	{
		CheckAlways(Instance == nullptr);
		Instance = this;
		if (mFenceEvent == nullptr)
		{
			HRESULT_FROM_WIN32(GetLastError());
			CheckAlways(false);
		}
		//allocators
		size_t memTypeCount = GetGPUMemoryHeapTypeCount();
		CheckAlways(memTypeCount == MemoryAllocatorTypeCount);
		CreateGPUMemoryAllocators(dev, MemoryAccessCase::GPUAccessOnly, mAsyncMemoryAllocator);
		CreateGPUMemoryAllocators(dev, MemoryAccessCase::CPUUpload, mUploadTempBufferAllocator);
		
		size_t descTypeCount = GetDescriptorHeapTypeCount();
		CheckAlways(descTypeCount == DescriptorHeapAllocatorTypeCount);
		CreateDescriptorHeapAllocators(dev, false, mAsyncDescriptorHeapAllocator);

		mResourceWorker = std::move(Thread(Entry, this));
	}
	void DX12DeviceResourceManager::AddRequest(IDX12ResourceRequest* request)
	{
		mStartQueue.PushBack(request);
	}
	void DX12DeviceResourceManager::Entry(void* s)
	{
		auto self = (DX12DeviceResourceManager*)s;
		self->Loop();
	}
	void DX12DeviceResourceManager::Loop()
	{
		mExpectedFenceValue                = 0;
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_COPY;
		CheckSucceeded(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCopyCommandQueue)));
		CheckSucceeded(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&mCommandAllocator)));
		CheckSucceeded(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, mCommandAllocator.GetPtr(), nullptr, IID_PPV_ARGS(&mCommandList)));
		CheckSucceeded(mCommandList->Close());
		CheckSucceeded(mDevice->CreateFence(mExpectedFenceValue, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&mFence)));

		std::vector<IDX12ResourceRequest*> popResults;
		while (true)
		{
			mStartQueue.Pop(MAX_BATCH_SIZE, popResults);
			CheckSucceeded(mCommandList->Reset(mCommandAllocator.GetPtr(), nullptr));
			for (auto it = popResults.begin(); it != popResults.end(); ++it)
			{
				(*it)->StartRequest(this);
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
					(*it)->FinishRequest(this);
				}
			}
			popResults.clear();
		}
	}

	IDX12GPUMemoryAllocator & DX12DeviceResourceManager::GetTempBufferAllocator(ResourceType resourceType)
	{
		return *mUploadTempBufferAllocator[GetGPUMemoryAllocatorIndex(resourceType)];
	}

	IDX12GPUMemoryAllocator & DX12DeviceResourceManager::GetAsyncPersistentAllocator(ResourceType resourceType)
	{
		return *mAsyncMemoryAllocator[GetGPUMemoryAllocatorIndex(resourceType)];
	}

	IDX12GPUMemoryAllocator & DX12DeviceResourceManager::GetSyncPersistentAllocator(ResourceType resourceType)
	{
		return *mSyncMemoryAllocator[GetGPUMemoryAllocatorIndex(resourceType)];
	}

	IDX12DescriptorHeapAllocator & DX12DeviceResourceManager::GetAsyncDescriptorHeapAllocator(ResourceType resourceType)
	{
		return *mAsyncDescriptorHeapAllocator[GetDescriptorHeapAllocatorIndex(resourceType)];
	}

	IDX12DescriptorHeapAllocator & DX12DeviceResourceManager::GetSyncDescriptorHeapAllocator(ResourceType resourceType)
	{
		return *mSyncDescriptorHeapAllocator[GetDescriptorHeapAllocatorIndex(resourceType)];
	}

	//shader
	DX12RenderDeviceShader::DX12RenderDeviceShader(ID3D12Device* dev, const char* body, size_t size, const char* name)
	{
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		mVS               = DoCompileShader(body, size, name, "VSMain", "vs_5_1", compileFlags);
		mPS               = DoCompileShader(body, size, name, "PSMain", "ps_5_1", compileFlags);
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
		std::tie(layout, count)        = GetInputLayoutForVertexFormat(desc.VF);
		DX12RenderDeviceShader* shader = static_cast<DX12RenderDeviceShader*>(desc.Shader.GetPtr());

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout                        = { layout, count };
		psoDesc.pRootSignature                     = shader->mRootSignature.GetPtr();
		psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(shader->mVS.GetPtr());
		psoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(shader->mPS.GetPtr());
		if (desc.StateKey != PSOStateKey::Default)
		{
		}
		else
		{
			psoDesc.RasterizerState                 = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.BlendState                      = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable   = FALSE;
			psoDesc.DepthStencilState.StencilEnable = FALSE;
			psoDesc.SampleMask                      = UINT_MAX;
			psoDesc.PrimitiveTopologyType           = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.SampleDesc.Count                = 1;
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
	class DX12RenderDeviceMeshCreateRequest : public IDX12ResourceRequest
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
		virtual void StartRequest(DX12DeviceResourceManager* creator) override
		{
			ID3D12Device* device                                   = creator->GetDevice();
			IDX12GPUMemoryAllocator& tempAllocator                 = creator->GetTempBufferAllocator(ResourceType::Buffer);
			IDX12GPUMemoryAllocator& persistentAllocator           = creator->GetAsyncPersistentAllocator(ResourceType::Buffer);
			IDX12DescriptorHeapAllocator& descriptorHeapAllocator  = creator->GetAsyncDescriptorHeapAllocator(ResourceType::Buffer);
			ID3D12GraphicsCommandList* commandList                 = creator->GetCommandList();
			DX12RenderDeviceMesh* mesh                             = mResource.GetPtr();
			for (int i = 0; i < 2; ++i)
			{
				CD3DX12_RESOURCE_DESC desc          = CD3DX12_RESOURCE_DESC::Buffer(mCPUData[i]->GetSize());
				D3D12_RESOURCE_ALLOCATION_INFO info = device->GetResourceAllocationInfo(0, 1, &desc);
				mTempMemRegion[i]       = tempAllocator.Allocate(info.SizeInBytes, info.Alignment);
				mesh->mMemoryRegion[i]    = persistentAllocator.Allocate(info.SizeInBytes, info.Alignment);
				//create resources
				//temp buffer
				CheckSucceeded(device->CreatePlacedResource(
					mTempMemRegion[i].Heap,
					mTempMemRegion[i].Offset,
					&desc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&mTempResource[i])));
				//persistent buffer
				CheckSucceeded(device->CreatePlacedResource(
					mesh->mMemoryRegion[i].Heap,
					mesh->mMemoryRegion[i].Offset,
					&desc,
					D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&mesh->mDeviceResource[i])));
				//write to cpu accessable part
				void* bufferStart;
				CD3DX12_RANGE range(0, 0);
				CheckSucceeded(mTempResource[i]->Map(0, &range, &bufferStart));
				memcpy(bufferStart, mCPUData[i]->GetData(), mCPUData[i]->GetSize());
				//copy on gpu then
				commandList->CopyBufferRegion(mResource->mDeviceResource[i], 0, mTempResource[i], 0, mCPUData[i]->GetSize());
				ID3D12Resource* res = mResource->mDeviceResource[i];
				CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
					res,
					D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ);
				commandList->ResourceBarrier(1, &barrier);
				//DescriptorHeap
				mesh->mHeapSpace = descriptorHeapAllocator.Allocate(1);
			}
		}
		virtual void FinishRequest(DX12DeviceResourceManager* creator) override
		{
			mResource->mIsReady = true;
			for (int i = 0; i < 2; ++i)
			{
				mTempResource[i]->Release();
				creator->GetTempBufferAllocator(ResourceType::Buffer).Free(mTempMemRegion[i]);
			}
			delete this;
		}
	private:
		TRef<DX12RenderDeviceMesh>         mResource;
		ID3D12Resource* mTempResource[2];
		DX12GPUMemoryRegion mTempMemRegion[2];
		SharedBufferRef                    mCPUData[2];
	};
	
	DX12RenderDeviceMesh::DX12RenderDeviceMesh(DX12DeviceResourceManager* creator, ISharedBuffer* buffers[2])
		: mIsReady(false)
	{
		DX12RenderDeviceMeshCreateRequest* req = new DX12RenderDeviceMeshCreateRequest(this, buffers);
		creator->AddRequest(req);
	}
	void DX12RenderDeviceMesh::Apply(ID3D12GraphicsCommandList* cmdList)
	{}
	void DX12RenderDeviceMesh::Destroy()
	{
		DX12DeviceResourceManager& manager = DX12DeviceResourceManager::GetDX12DeviceResourceManager();
		DX12ReleaseSpaceRequest* req = new DX12ReleaseSpaceRequest(ResourceType::Buffer);
		for (int i = 0; i < 2; ++i)
		{
			mDeviceResource[i]->Release();
			req->AddMemoryRegions(mMemoryRegion, 2);
			req->AddDescriptorSpaces(&mHeapSpace, 1);
		}
		manager.AddRequest(req);
	}

	IDX12RenderDeviceRenderTarget::~IDX12RenderDeviceRenderTarget()
	{
		mRenderTarget->Release();
	}
	void IDX12RenderDeviceRenderTarget::TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList)
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
	D3D12_CPU_DESCRIPTOR_HANDLE IDX12RenderDeviceRenderTarget::GetHandle()
	{
		return D3D12_CPU_DESCRIPTOR_HANDLE();
	}
	void DX12Backbuffer::Destroy()
	{
		DX12DeviceResourceManager& manager = DX12DeviceResourceManager::GetDX12DeviceResourceManager();
		manager.GetSyncDescriptorHeapAllocator(ResourceType::RenderTarget).Free(mHeapSpace);
	}
}