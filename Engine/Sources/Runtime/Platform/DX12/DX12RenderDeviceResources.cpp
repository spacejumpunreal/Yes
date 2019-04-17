#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Platform/DX12/DX12MemAllocators.h"
#include "Platform/DX12/DX12Parameters.h"
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
		static D3D12_INPUT_ELEMENT_DESC VF_P3FN3FT2F_LAYOUT[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		switch (vf)
		{
		case VertexFormat::VF_P3F_T2F:
			return std::make_pair(VF_P3FN3FT2F_LAYOUT, (UINT)ARRAY_COUNT(VF_P3FN3FT2F_LAYOUT));
		default:
			CheckAlways(false, "unknown vertex format");
			return {};
		}
	}
	//texture format
	static DXGI_FORMAT GetTextureFormat(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::R8G8B8A8_UNORM:
			return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat::R24_UNORM_X8_TYPELESS:
			return DXGI_FORMAT::DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case TextureFormat::D24_UNORM_S8_UINT:
			return DXGI_FORMAT::DXGI_FORMAT_D24_UNORM_S8_UINT;
		default:
			CheckAlways(false);
		}
		return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
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
				mMemoryRegions.push_back(regions[i]);
			}
		}
		void AddDescriptorSpaces(const DX12DescriptorHeapSpace1* spaces, size_t count)
		{
			for (int i = 0; i < count; ++i)
			{
				mDescriptorSpaces.push_back(spaces[i]);
			}
		}
		virtual void StartRequest(DX12ResourceManager* creator)
		{
			IDX12GPUMemoryAllocator& mem = creator->GetAsyncPersistentAllocator(mResourceType);
			for (auto& region : mMemoryRegions)
			{
				mem.Free(region);
			}

		}
		virtual void FinishRequest(DX12ResourceManager* creator)
		{}
	private:
		ResourceType mResourceType;
		std::deque<DX12GPUMemoryRegion> mMemoryRegions;
		std::deque<DX12DescriptorHeapSpace1> mDescriptorSpaces;
	};

	DX12ResourceManager* DX12ResourceManager::Instance;
	DX12ResourceManager::DX12ResourceManager(ID3D12Device* dev)
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
		CreateGPUMemoryAllocators(dev, MemoryAccessCase::GPUAccessOnly, mSyncMemoryAllocator);
		CreateGPUMemoryAllocators(dev, MemoryAccessCase::CPUUpload, mUploadTempBufferAllocator);

		size_t descTypeCount = GetDescriptorHeapTypeCount();
		CheckAlways(descTypeCount == DescriptorHeapAllocatorTypeCount);
		CreateNonShaderVisibleDescriptorHeapAllocators(dev, false, mAsyncDescriptorHeapAllocator);
		CreateNonShaderVisibleDescriptorHeapAllocators(dev, false, mSyncDescriptorHeapAllocator);
		mResourceWorker = std::move(Thread(Entry, this, L"ResourceManagerThread"));
	}
	void DX12ResourceManager::AddRequest(IDX12ResourceRequest* request)
	{
		mStartQueue.PushBack(request);
	}
	void DX12ResourceManager::Entry(void* s)
	{
		auto self = (DX12ResourceManager*)s;
		self->Loop();
	}
	void DX12ResourceManager::Loop()
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
					WaitForSingleObject(mFenceEvent, INFINITE);
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

	IDX12GPUMemoryAllocator & DX12ResourceManager::GetTempBufferAllocator(ResourceType resourceType)
	{
		return *mUploadTempBufferAllocator[GetGPUMemoryAllocatorIndex(resourceType)];
	}

	IDX12GPUMemoryAllocator & DX12ResourceManager::GetAsyncPersistentAllocator(ResourceType resourceType)
	{
		return *mAsyncMemoryAllocator[GetGPUMemoryAllocatorIndex(resourceType)];
	}

	IDX12GPUMemoryAllocator & DX12ResourceManager::GetSyncPersistentAllocator(ResourceType resourceType)
	{
		return *mSyncMemoryAllocator[GetGPUMemoryAllocatorIndex(resourceType)];
	}

	IDX12DescriptorHeapAllocator & DX12ResourceManager::GetAsyncDescriptorHeapAllocator(ResourceType resourceType)
	{
		return *mAsyncDescriptorHeapAllocator[GetDescriptorHeapAllocatorIndex(resourceType)];
	}

	IDX12DescriptorHeapAllocator & DX12ResourceManager::GetSyncDescriptorHeapAllocator(ResourceType resourceType)
	{
		return *mSyncDescriptorHeapAllocator[GetDescriptorHeapAllocatorIndex(resourceType)];
	}

	//shader
	DX12Shader::DX12Shader(ID3D12Device* dev, const char* body, size_t size, const char* name)
	{
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		mVS               = DoCompileShader(body, size, name, "VSMain", "vs_5_1", compileFlags);
		mPS               = DoCompileShader(body, size, name, "PSMain", "ps_5_1", compileFlags);
		COMRef<ID3DBlob> blob;
		CheckSucceeded(D3DGetBlobPart(mPS->GetBufferPointer(), mPS->GetBufferSize(), D3D_BLOB_ROOT_SIGNATURE, 0, &blob));
		CheckSucceeded(dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}
	DX12Shader::~DX12Shader()
	{
		CheckAlways(false);//not suppose to delete shader
	}

	//pso
	DX12PSO::DX12PSO(ID3D12Device* dev, RenderDevicePSODesc& desc)
	{
		D3D12_INPUT_ELEMENT_DESC* layout;
		UINT count;
		std::tie(layout, count)        = GetInputLayoutForVertexFormat(desc.VF);
		DX12Shader* shader = static_cast<DX12Shader*>(desc.Shader.GetPtr());
		mRootSignature = shader->mRootSignature;

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
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK;
			psoDesc.RasterizerState.FrontCounterClockwise = true;
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
	void DX12PSO::Apply(ID3D12GraphicsCommandList* cmdList)
	{
		cmdList->SetGraphicsRootSignature(mRootSignature.GetPtr());
		cmdList->SetPipelineState(mPSO.GetPtr());
	}

	//Mesh
	class DX12RenderDeviceMeshCreateRequest : public IDX12ResourceRequest
	{
	public:
		DX12RenderDeviceMeshCreateRequest(
			DX12Mesh* resource,
			ISharedBuffer* CPUData[2],
			size_t vertexStride,
			size_t indexStride)
			: mResource(resource)
			, mVertexStride(vertexStride)
			, mIndexStride(indexStride)
		{
			for (int i = 0; i < 2; ++i)
			{
				mCPUData[i] = CPUData[i];
			}
		}
		virtual void StartRequest(DX12ResourceManager* creator) override
		{
			ID3D12Device* device                                   = creator->GetDevice();
			IDX12GPUMemoryAllocator& tempAllocator                 = creator->GetTempBufferAllocator(ResourceType::Buffer);
			IDX12GPUMemoryAllocator& persistentAllocator           = creator->GetAsyncPersistentAllocator(ResourceType::Buffer);
			IDX12DescriptorHeapAllocator& descriptorHeapAllocator  = creator->GetAsyncDescriptorHeapAllocator(ResourceType::Buffer);
			ID3D12GraphicsCommandList* commandList                 = creator->GetCommandList();
			DX12Mesh* mesh                             = mResource.GetPtr();
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
				mTempResource[i]->SetName(L"MeshCreateRequestTempResource");

				//persistent buffer
				CheckSucceeded(device->CreatePlacedResource(
					mesh->mMemoryRegion[i].Heap,
					mesh->mMemoryRegion[i].Offset,
					&desc,
					D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
					nullptr,
					IID_PPV_ARGS(&mesh->mDeviceResource[i])));
				//write to cpu accessable part
				void* bufferStart;
				CheckSucceeded(mTempResource[i]->Map(0, nullptr, &bufferStart));
				memcpy(bufferStart, mCPUData[i]->GetData(), mCPUData[i]->GetSize());
				mTempResource[i]->Unmap(0, nullptr);
				//copy on gpu then
				commandList->CopyBufferRegion(mResource->mDeviceResource[i], 0, mTempResource[i], 0, mCPUData[i]->GetSize());
				ID3D12Resource* res = mResource->mDeviceResource[i];
			}
			mesh->mVertexBufferView = {};
			mesh->mVertexBufferView.BufferLocation = mesh->mDeviceResource[0]->GetGPUVirtualAddress();
			mesh->mVertexBufferView.SizeInBytes = (UINT)mCPUData[0]->GetSize();
			mesh->mVertexBufferView.StrideInBytes = (UINT)mVertexStride;

			mesh->mIndexBufferView = {};
			mesh->mIndexBufferView.BufferLocation = mesh->mDeviceResource[1]->GetGPUVirtualAddress();
			mesh->mIndexBufferView.SizeInBytes = (UINT)mCPUData[1]->GetSize();
			DXGI_FORMAT indexFormat = DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
			switch (mIndexStride)
			{
			case 2:
				indexFormat = DXGI_FORMAT::DXGI_FORMAT_R16_UINT;
				break;
			case 4:
			default:
				indexFormat = DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
				break;
			}
			mesh->mIndexBufferView.Format = indexFormat;
		}
		virtual void FinishRequest(DX12ResourceManager* creator) override
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
		TRef<DX12Mesh>         mResource;
		ID3D12Resource* mTempResource[2];
		DX12GPUMemoryRegion mTempMemRegion[2];
		SharedBufferRef                    mCPUData[2];
		size_t mVertexStride;
		size_t mIndexStride;
	};
	
	DX12Mesh::DX12Mesh(DX12ResourceManager* creator, ISharedBuffer* CPUData[2], size_t vertexStride, size_t indexCount, size_t indexStride)
		: mIsReady(false)
		, mIndexCount((UINT)indexCount)
	{
		DX12RenderDeviceMeshCreateRequest* req = new DX12RenderDeviceMeshCreateRequest(this, CPUData, vertexStride, indexStride);
		creator->AddRequest(req);
	}
	void DX12Mesh::Apply(ID3D12GraphicsCommandList* cmdList)
	{
		cmdList->IASetIndexBuffer(&mIndexBufferView);
		cmdList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	}
	void DX12Mesh::SetName(wchar_t * name)
	{
		mDeviceResource[0]->SetName(name);
		mDeviceResource[1]->SetName(name);
	}
	void DX12Mesh::Destroy()
	{
		DX12ResourceManager& manager = DX12ResourceManager::GetDX12DeviceResourceManager();
		DX12ReleaseSpaceRequest* req = new DX12ReleaseSpaceRequest(ResourceType::Buffer);
		for (int i = 0; i < 2; ++i)
		{
			mDeviceResource[i]->Release();
		}
		req->AddMemoryRegions(mMemoryRegion, 2);
		manager.AddRequest(req);
		SharedObject::Destroy();
	}
	//DX12ResourceBase
	DX12ResourceBase::DX12ResourceBase()
		: mState(D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON)
	{}
	//IDX12RenderTarget
	IDX12RenderTarget::IDX12RenderTarget()
		: mRenderTarget(nullptr)
	{}
	void IDX12RenderTarget::TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList)
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
	IDX12RenderTarget::~IDX12RenderTarget()
	{
		mRenderTarget->Release();
	}
	void IDX12RenderTarget::SetName(wchar_t * name)
	{
		mRenderTarget->SetName(name);
	}
	//DX12RenderTarget
	DX12RenderTarget::DX12RenderTarget(
		size_t width, 
		size_t height, 
		TextureFormat format, 
		ID3D12Device* device)
	{
		DX12ResourceManager* manager = &DX12ResourceManager::GetDX12DeviceResourceManager();
		DXGI_FORMAT texFormat = GetTextureFormat(format);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
			texFormat,
			(UINT64)width,
			(UINT)height,
			1,
			1,
			1,
			0,
			D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
			D3D12_TEXTURE_LAYOUT::D3D12_TEXTURE_LAYOUT_UNKNOWN,
			0);
		D3D12_CLEAR_VALUE ocv = {};
		ocv.Format = texFormat;
		for (int i = 0; i < 4; ++i)
		{
			ocv.Color[i] = 0.0f;
		}
		D3D12_RESOURCE_ALLOCATION_INFO ainfo = device->GetResourceAllocationInfo(0, 1, &desc);
		IDX12GPUMemoryAllocator& ma = manager->GetSyncPersistentAllocator(ResourceType::RenderTarget);
		IDX12DescriptorHeapAllocator& ha = manager->GetSyncDescriptorHeapAllocator(ResourceType::RenderTarget);
		mMemoryRegion = ma.Allocate(ainfo.SizeInBytes, ainfo.Alignment);
		device->CreatePlacedResource(
			mMemoryRegion.Heap,
			mMemoryRegion.Offset,
			&desc,
			D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
			&ocv,
			IID_PPV_ARGS(&mRenderTarget));
		size_t srvStep = GetDX12RuntimeParameters().DescriptorHeapHandleSizes[D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
		size_t rtvStep = GetDX12RuntimeParameters().DescriptorHeapHandleSizes[D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV];

		mWriteTargetHeapSpace = manager->GetSyncDescriptorHeapAllocator(ResourceType::Texture).Allocate(srvStep);
		mReadTargetHeapSpace = manager->GetSyncDescriptorHeapAllocator(ResourceType::RenderTarget).Allocate(rtvStep);

		device->CreateShaderResourceView(mRenderTarget, nullptr, mReadTargetHeapSpace.GetCPUHandle(0));
		device->CreateRenderTargetView(mRenderTarget, nullptr, mWriteTargetHeapSpace.GetCPUHandle(0));
	}
	void DX12RenderTarget::Destroy()
	{
		DX12ResourceManager& manager = DX12ResourceManager::GetDX12DeviceResourceManager();
		manager.GetSyncDescriptorHeapAllocator(ResourceType::Texture).Free(mReadTargetHeapSpace);
		manager.GetSyncDescriptorHeapAllocator(ResourceType::RenderTarget).Free(mWriteTargetHeapSpace);
		manager.GetSyncPersistentAllocator(ResourceType::RenderTarget).Free(mMemoryRegion);
		SharedObject::Destroy();
	}
	D3D12_CPU_DESCRIPTOR_HANDLE DX12RenderTarget::GetHandle(RenderTargetDescriptorIndex idx)
	{
		switch (idx)
		{
		case RenderTargetDescriptorIndex::RTV:
			return mWriteTargetHeapSpace.GetCPUHandle(0);
		case RenderTargetDescriptorIndex::SRV:
			return mReadTargetHeapSpace.GetCPUHandle(0);
		default:
			CheckAlways(false);
			return D3D12_CPU_DESCRIPTOR_HANDLE();
		}
	}
	D3D12_CPU_DESCRIPTOR_HANDLE DX12RenderTarget::GetSRVHandle()
	{
		return mReadTargetHeapSpace.GetCPUHandle(0);
	}
	//DX12Backbuffer
	DX12Backbuffer::DX12Backbuffer(ID3D12Resource* backbuffer, ID3D12Device* device, wchar_t* name)
	{
		if (name != nullptr)
		{
			backbuffer->SetName(name);
		}
		DX12ResourceManager* manager = &DX12ResourceManager::GetDX12DeviceResourceManager();
		mRenderTarget = backbuffer;
		IDX12DescriptorHeapAllocator& ha = manager->GetSyncDescriptorHeapAllocator(ResourceType::RenderTarget);
		mWriteTargetHeapSpace = ha.Allocate(1);
		device->CreateRenderTargetView(mRenderTarget, nullptr, mWriteTargetHeapSpace.GetCPUHandle(0));
		mState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE DX12Backbuffer::GetHandle(RenderTargetDescriptorIndex idx)
	{
		switch (idx)
		{
		case RenderTargetDescriptorIndex::RTV:
			return mWriteTargetHeapSpace.GetCPUHandle(0);
		default:
			CheckAlways(false);
			return D3D12_CPU_DESCRIPTOR_HANDLE();
		}
	}
	void DX12Backbuffer::Destroy()
	{
		DX12ResourceManager* manager = &DX12ResourceManager::GetDX12DeviceResourceManager();
		manager->GetSyncDescriptorHeapAllocator(ResourceType::RenderTarget).Free(mWriteTargetHeapSpace);
		SharedObject::Destroy();
	}
	//DX12DepthStencil
	DX12DepthStencil::DX12DepthStencil(size_t width, size_t height, TextureFormat format, ID3D12Device* device)
	{
		DX12ResourceManager* manager = &DX12ResourceManager::GetDX12DeviceResourceManager();
		DXGI_FORMAT texFormat = GetTextureFormat(format);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
			texFormat,
			(UINT64)width,
			(UINT)height,
			1,
			1,
			1,
			0,
			D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
			D3D12_TEXTURE_LAYOUT::D3D12_TEXTURE_LAYOUT_UNKNOWN,
			0);
		D3D12_CLEAR_VALUE ocv = {};
		ocv.DepthStencil.Depth = 1.0f;
		ocv.DepthStencil.Stencil = 0;
		ocv.Format = texFormat;

		D3D12_RESOURCE_ALLOCATION_INFO ainfo = device->GetResourceAllocationInfo(0, 1, &desc);
		IDX12GPUMemoryAllocator& ma = manager->GetSyncPersistentAllocator(ResourceType::RenderTarget);
		IDX12DescriptorHeapAllocator& ha = manager->GetSyncDescriptorHeapAllocator(ResourceType::RenderTarget);
		mMemoryRegion = ma.Allocate(ainfo.SizeInBytes, ainfo.Alignment);
		CheckSucceeded(device->CreatePlacedResource(
			mMemoryRegion.Heap,
			mMemoryRegion.Offset,
			&desc,
			D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
			&ocv,
			IID_PPV_ARGS(&mBuffer)));
		mHeapSpace = manager->GetSyncDescriptorHeapAllocator(ResourceType::DepthStencil).Allocate(1);
		device->CreateDepthStencilView(mBuffer, nullptr, mHeapSpace.GetCPUHandle(0));
	}
	void DX12DepthStencil::Destroy()
	{//main thread
		DX12ResourceManager* manager = &DX12ResourceManager::GetDX12DeviceResourceManager();
		IDX12DescriptorHeapAllocator& ha = manager->GetSyncDescriptorHeapAllocator(ResourceType::DepthStencil);
		ha.Free(mHeapSpace);
		IDX12GPUMemoryAllocator& ma = manager->GetSyncPersistentAllocator(ResourceType::DepthStencil);
		ma.Free(mMemoryRegion);
		SharedObject::Destroy();
	}
	void DX12DepthStencil::SetName(wchar_t* name)
	{
		mBuffer->SetName(name);
	}
	void DX12DepthStencil::TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList)
	{
		if (newState != mState)
		{
			cmdList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mBuffer,
					mState,
					newState));
			mState = newState;
		}
	}
	//DX12Texture2D
	class DX12RenderDeviceTexture2DCreateRequest : public IDX12ResourceRequest
	{
	public:
		DX12RenderDeviceTexture2DCreateRequest(DX12Texture2D* resource, RawImage* rawData)
			: mResource(resource)
			, mRawData(rawData)
			, mFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
		{}
		void StartRequest(DX12ResourceManager* creator) override
		{
			ID3D12Device* device = creator->GetDevice();
			IDX12GPUMemoryAllocator& tempAllocator = creator->GetTempBufferAllocator(ResourceType::Buffer);
			IDX12GPUMemoryAllocator& persistentAllocator = creator->GetAsyncPersistentAllocator(ResourceType::Texture);
			IDX12DescriptorHeapAllocator& descriptorHeapAllocator = creator->GetAsyncDescriptorHeapAllocator(ResourceType::Texture);
			ID3D12GraphicsCommandList* commandList = creator->GetCommandList();

			CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				mFormat, 
				(UINT64)mRawData->GetWidth(), 
				(UINT)mRawData->GetHeight());
			D3D12_RESOURCE_ALLOCATION_INFO info = device->GetResourceAllocationInfo(0, 1, &textureDesc);
			mTempMemoryRegion = tempAllocator.Allocate(info.SizeInBytes, info.Alignment);
			mResource->mMemoryRegion = persistentAllocator.Allocate(info.SizeInBytes, info.Alignment);
			//persistent buffer
			CheckSucceeded(device->CreatePlacedResource(
				mResource->mMemoryRegion.Heap,
				mResource->mMemoryRegion.Offset,
				&textureDesc,
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(&mResource->mTexture)));

			D3D12_SUBRESOURCE_DATA subResourceStruct =
			{
				mRawData->GetData(),
				(LONG_PTR)mRawData->GetRowSize(),
				(LONG_PTR)mRawData->GetSliceSize(),
			};
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT subresourceFootprint;
			UINT rows;
			UINT64 rowSize;
			UINT64 totalBytes;
			auto desc = mResource->mTexture->GetDesc();
			device->GetCopyableFootprints(&desc, 0, 1, 0, &subresourceFootprint, &rows, &rowSize, &totalBytes);

			//temp resources
			CheckSucceeded(device->CreatePlacedResource(
				mTempMemoryRegion.Heap,
				mTempMemoryRegion.Offset,
				&CD3DX12_RESOURCE_DESC::Buffer(totalBytes),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&mTempResource)));
			mTempResource->SetName(L"Texture2DCreateRequestTempResource");
			{//single slice
				UpdateSubresources(
					commandList, 
					mResource->mTexture, 
					mTempResource,
					(UINT)0, (UINT)1, totalBytes,
					&subresourceFootprint, &rows, &rowSize, &subResourceStruct);
				/*
				commandList->ResourceBarrier(
					1,
					&CD3DX12_RESOURCE_BARRIER::Transition(
						mResource->mTexture,
						D3D12_RESOURCE_STATE_COPY_DEST,
						D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
				);
				*/
			}
			mResource->mHeapSpace = descriptorHeapAllocator.Allocate(1);
			device->CreateShaderResourceView(mResource->mTexture, nullptr, mResource->mHeapSpace.GetCPUHandle(0));
		}
		void FinishRequest(DX12ResourceManager* creator) override
		{
			mResource->mIsReady = true;
			for (int i = 0; i < 2; ++i)
			{
				mTempResource->Release();
				creator->GetTempBufferAllocator(ResourceType::Buffer);
			}
			delete this;
		}
	private:
		TRef<DX12Texture2D> mResource;
		ID3D12Resource* mTempResource;
		TRef<RawImage> mRawData;
		DXGI_FORMAT mFormat;
		DX12GPUMemoryRegion mTempMemoryRegion;
	};

	DX12Texture2D::DX12Texture2D(DX12ResourceManager* creator, RawImage* image)
	{
		auto req = new DX12RenderDeviceTexture2DCreateRequest(this, image);
		creator->AddRequest(req);
	}
	void DX12Texture2D::Destroy()
	{//need to be async
		DX12ResourceManager& manager = DX12ResourceManager::GetDX12DeviceResourceManager();
		DX12ReleaseSpaceRequest* req = new DX12ReleaseSpaceRequest(ResourceType::Texture);
		mTexture->Release();
		req->AddMemoryRegions(&mMemoryRegion, 1);
		req->AddDescriptorSpaces(&mHeapSpace, 1);
		manager.AddRequest(req);
	}
	D3D12_CPU_DESCRIPTOR_HANDLE DX12Texture2D::GetSRVHandle()
	{
		return mHeapSpace.GetCPUHandle(0);
	}
	void DX12Texture2D::SetName(wchar_t* name)
	{
		mTexture->SetName(name);
	}
}