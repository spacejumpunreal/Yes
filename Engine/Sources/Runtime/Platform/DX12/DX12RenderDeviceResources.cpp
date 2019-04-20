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
		case TextureFormat::D24_UNORM_S8_UINT:
			return DXGI_FORMAT::DXGI_FORMAT_D24_UNORM_S8_UINT;
		case TextureFormat::D32_UNORM_S8_UINT:
			return DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		default:
			CheckAlways(false);
		}
		return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	static DXGI_FORMAT GetDepthShaderResourceViewFormat(TextureFormat format, bool readDepthValue)
	{
		if (readDepthValue)
		{
			switch (format)
			{
			case TextureFormat::D24_UNORM_S8_UINT:
				return DXGI_FORMAT::DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			case TextureFormat::D32_UNORM_S8_UINT:
				return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			default:
				CheckAlways(false);
			}
		}
		else
		{
			switch (format)
			{
			case TextureFormat::D24_UNORM_S8_UINT:
				return DXGI_FORMAT::DXGI_FORMAT_X24_TYPELESS_G8_UINT;
			case TextureFormat::D32_UNORM_S8_UINT:
				return DXGI_FORMAT::DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
			default:
				CheckAlways(false);
			}
		}
		return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
	}

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
	class DX12RenderDeviceMeshCopyRequest : public IDX12ResourceRequest
	{
	public:
		DX12RenderDeviceMeshCopyRequest(
			DX12Mesh* resource,
			ISharedBuffer* CPUData[2],
			size_t vertexStride,
			size_t indexStride,
			D3D12_RESOURCE_DESC desc[])
			: mResource(resource)
			, mVertexStride(vertexStride)
			, mIndexStride(indexStride)
		{
			resource->mIsReady = false;
			for (int i = 0; i < 2; ++i)
			{
				mCPUData[i] = CPUData[i];
				mDescs[i] = desc[i];
			}
		}
		virtual void StartRequest(DX12ResourceManager* creator) override
		{
			ID3D12Device* device                                   = creator->GetDevice();
			IDX12GPUMemoryAllocator& tempAllocator                 = creator->GetTempBufferAllocator(ResourceType::Buffer);
			ID3D12GraphicsCommandList* commandList                 = creator->GetCommandList();
			DX12Mesh* mesh										   = mResource.GetPtr();
			for (int i = 0; i < 2; ++i)
			{
				D3D12_RESOURCE_ALLOCATION_INFO info = device->GetResourceAllocationInfo(0, 1, &mDescs[i]);
				mTempMemRegion[i] = tempAllocator.Allocate(info.SizeInBytes, info.Alignment);
				//create resources
				//temp buffer
				CheckSucceeded(device->CreatePlacedResource(
					mTempMemRegion[i].Heap,
					mTempMemRegion[i].Offset,
					&mDescs[i],
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&mTempResource[i])));
				mTempResource[i]->SetName(L"MeshCreateRequestTempResource");
				//write to cpu accessable part
				void* bufferStart;
				CheckSucceeded(mTempResource[i]->Map(0, nullptr, &bufferStart));
				memcpy(bufferStart, mCPUData[i]->GetData(), mCPUData[i]->GetSize());
				mTempResource[i]->Unmap(0, nullptr);
				//copy on gpu then
				commandList->CopyBufferRegion(mResource->mDeviceResource[i], 0, mTempResource[i], 0, mCPUData[i]->GetSize());
			}
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
		TRef<DX12Mesh>                      mResource;
		ID3D12Resource*						mTempResource[2];
		DX12GPUMemoryRegion                 mTempMemRegion[2];
		SharedBufferRef                     mCPUData[2];
		size_t                              mVertexStride;
		size_t                              mIndexStride;
		D3D12_RESOURCE_DESC					mDescs[2];
	};
	
	DX12Mesh::DX12Mesh(size_t streamSizes[], size_t strides[], size_t indexCount, D3D12_RESOURCE_DESC desc[])
		: mIsReady(true)
		, mIndexCount((UINT)indexCount)
	{
		DX12ResourceManager& manager = DX12ResourceManager::GetDX12DeviceResourceManager();
		IDX12GPUMemoryAllocator& allocator = manager.GetSyncPersistentAllocator(ResourceType::Buffer);
		ID3D12Device* dev = manager.GetDevice();
		for (int i = 0; i < 2; ++i)
		{
			desc[i] = CD3DX12_RESOURCE_DESC::Buffer(streamSizes[i]);
			D3D12_RESOURCE_ALLOCATION_INFO info = dev->GetResourceAllocationInfo(0, 1, &desc[i]);
			mMemoryRegion[i] = allocator.Allocate(info.SizeInBytes, info.Alignment);
			CheckSucceeded(dev->CreatePlacedResource(
				mMemoryRegion[i].Heap, mMemoryRegion[i].Offset,
				&desc[i], D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(&mDeviceResource[i])));
		}
		mVertexBufferView = {};
		mVertexBufferView.BufferLocation = mDeviceResource[0]->GetGPUVirtualAddress();
		mVertexBufferView.SizeInBytes = (UINT)streamSizes[0];
		mVertexBufferView.StrideInBytes = (UINT)strides[0];

		mIndexBufferView = {};
		mIndexBufferView.BufferLocation = mDeviceResource[1]->GetGPUVirtualAddress();
		mIndexBufferView.SizeInBytes = (UINT)streamSizes[1];
		DXGI_FORMAT indexFormat = DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
		switch (strides[1])
		{
		case 2:
			indexFormat = DXGI_FORMAT::DXGI_FORMAT_R16_UINT;
			break;
		case 4:
			indexFormat = DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
			break;
		default:
			CheckAlways(false);
		}
		mIndexBufferView.Format = indexFormat;
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
		IDX12GPUMemoryAllocator& allocator = manager.GetSyncPersistentAllocator(ResourceType::Buffer);
		for (int i = 0; i < 2; ++i)
		{
			mDeviceResource[i]->Release();
			allocator.Free(mMemoryRegion[i]);
		}
		SharedObject::Destroy();
	}
	void DX12Mesh::AsyncInit(ISharedBuffer* bufferData[], size_t vertexStride, size_t indexStride, D3D12_RESOURCE_DESC desc[])
	{
		DX12RenderDeviceMeshCopyRequest* req = new DX12RenderDeviceMeshCopyRequest(
			this, bufferData, vertexStride, indexStride, desc);
		DX12ResourceManager::GetDX12DeviceResourceManager().AddRequest(req);
	}
	void DX12Mesh::TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList)
	{
		CheckAlways(false);
	}
	//DX12ResourceBase
	DX12ResourceBase::DX12ResourceBase()
		: mState(D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON)
	{}
	//DX12Texture2D
	class DX12RenderDeviceTexture2DCopyRequest : public IDX12ResourceRequest
	{
	public:
		DX12RenderDeviceTexture2DCopyRequest(DX12Texture2D* resource, RawImage* rawData, D3D12_RESOURCE_DESC* desc)
			: mResource(resource)
			, mRawData(rawData)
			, mFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
			, mDesc(*desc)
		{
			resource->mIsReady = false;
		}
		void StartRequest(DX12ResourceManager* creator) override
		{
			ID3D12Device* device = creator->GetDevice();
			IDX12GPUMemoryAllocator& tempAllocator = creator->GetTempBufferAllocator(ResourceType::Buffer);
			ID3D12GraphicsCommandList* commandList = creator->GetCommandList();

			D3D12_RESOURCE_ALLOCATION_INFO info = device->GetResourceAllocationInfo(0, 1, &mDesc);
			mTempMemoryRegion = tempAllocator.Allocate(info.SizeInBytes, info.Alignment);
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
			device->GetCopyableFootprints(&mDesc, 0, 1, 0, &subresourceFootprint, &rows, &rowSize, &totalBytes);

			//temp resources
			CheckSucceeded(device->CreatePlacedResource(
				mTempMemoryRegion.Heap,
				mTempMemoryRegion.Offset,
				&CD3DX12_RESOURCE_DESC::Buffer(totalBytes),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&mTempResource)));
			mTempResource->SetName(L"Texture2DCreateRequestTempResource");
			UpdateSubresources(
				commandList, 
				mResource->mTexture, 
				mTempResource,
				(UINT)0, (UINT)1, totalBytes,
				&subresourceFootprint, &rows, &rowSize, &subResourceStruct);
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
		D3D12_RESOURCE_DESC mDesc;
	};

	DX12Texture2D::DX12Texture2D(size_t width, size_t height, TextureFormat format, TextureUsage usage, D3D12_RESOURCE_DESC* desc)
		: mFormat(format)
		, mUsage(usage)
		, mIsReady(true)
	{
		InitTexture(width, height, format, usage, desc);
		InitHandles();
	}
	DX12Texture2D::DX12Texture2D(ID3D12Resource* resource, TextureFormat format, TextureUsage usage)
		: mFormat(format)
		, mUsage(usage)
		, mIsReady(true)
	{
		mTexture = resource;
		InitHandles();
	}
	void DX12Texture2D::Destroy()
	{
		mTexture->Release();
	}
	void DX12Texture2D::InitTexture(size_t width, size_t height, TextureFormat format, TextureUsage usage, D3D12_RESOURCE_DESC* desc)
	{
		ResourceType tp;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_NONE;
		switch (mUsage)
		{
		case TextureUsage::ShaderResource:
			tp = ResourceType::Texture;
			break;
		case TextureUsage::RenderTarget:
			tp = ResourceType::RenderTarget;
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			break;
		case TextureUsage::DepthStencil:
			tp = ResourceType::DepthStencil;
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			break;
		default:
			break;
		}
		DXGI_FORMAT apiFormat = GetTextureFormat(format);
		*desc = CD3DX12_RESOURCE_DESC::Tex2D(apiFormat, (UINT)width, (UINT)height);
		desc->Flags = flags;
		DX12ResourceManager& manager = DX12ResourceManager::GetDX12DeviceResourceManager();
		ID3D12Device* dev = manager.GetDevice();
		D3D12_RESOURCE_ALLOCATION_INFO info = dev->GetResourceAllocationInfo(0, 1, desc);

		IDX12GPUMemoryAllocator& memAllocator = manager.GetSyncPersistentAllocator(tp);
		mMemoryRegion = memAllocator.Allocate(info.SizeInBytes, info.Alignment);
		D3D12_CLEAR_VALUE cv = {};
		cv.Format = apiFormat;
		D3D12_CLEAR_VALUE* pcv = &cv;
		if (mUsage == TextureUsage::DepthStencil)
		{
			cv.DepthStencil = { 1.0f, 0 };
		}
		else if (mUsage == TextureUsage::RenderTarget)
		{
			for (int i = 0; i < 4; ++i)
			{
				cv.Color[i] = 0;
			}
		}
		else
		{
			pcv = nullptr;
		}
		CheckSucceeded(dev->CreatePlacedResource(
			mMemoryRegion.Heap, mMemoryRegion.Offset,
			desc, D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
			pcv,
			IID_PPV_ARGS(&mTexture)));
	}
	void DX12Texture2D::InitHandles()
	{
		DX12ResourceManager& manager = DX12ResourceManager::GetDX12DeviceResourceManager();
		ID3D12Device* dev = manager.GetDevice();
		{
			IDX12DescriptorHeapAllocator& allocator = manager.GetSyncDescriptorHeapAllocator(ResourceType::Texture);
			DX12DescriptorHeapSpace1& heapSpace = mHeapSpace[(int)TextureUsage::ShaderResource] = allocator.Allocate(1);
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
			D3D12_SHADER_RESOURCE_VIEW_DESC* pdesc;
			if (mUsage != TextureUsage::DepthStencil)
			{
				pdesc = nullptr;
			}
			else
			{
				pdesc = &desc;
				desc.Format = GetDepthShaderResourceViewFormat(mFormat, true);
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				desc.Texture2D.MipLevels = 1;
				desc.Texture2D.MostDetailedMip = 0;
				desc.Texture2D.ResourceMinLODClamp = 0.0f;
			}
			dev->CreateShaderResourceView(mTexture, pdesc, heapSpace.GetCPUHandle(0));
		}
		if (mUsage == TextureUsage::RenderTarget)
		{
			IDX12DescriptorHeapAllocator& allocator = manager.GetSyncDescriptorHeapAllocator(ResourceType::RenderTarget);
			DX12DescriptorHeapSpace1& heapSpace = mHeapSpace[(int)TextureUsage::RenderTarget] = allocator.Allocate(1);
			dev->CreateRenderTargetView(mTexture, nullptr, heapSpace.GetCPUHandle(0));
		}
		else if (mUsage == TextureUsage::DepthStencil)
		{
			IDX12DescriptorHeapAllocator& allocator = manager.GetSyncDescriptorHeapAllocator(ResourceType::DepthStencil);
			DX12DescriptorHeapSpace1& heapSpace = mHeapSpace[(int)TextureUsage::DepthStencil] = allocator.Allocate(1);
			dev->CreateDepthStencilView(mTexture, nullptr, heapSpace.GetCPUHandle(0));
		}
	}
	D3D12_CPU_DESCRIPTOR_HANDLE DX12Texture2D::GetCPUHandle(TextureUsage usage)
	{
		return mHeapSpace[(int)usage].GetCPUHandle(0);
	}
	void DX12Texture2D::SetName(wchar_t* name)
	{
		mTexture->SetName(name);
	}
	void DX12Texture2D::AsyncInit(RawImage * image, D3D12_RESOURCE_DESC* desc)
	{
		DX12ResourceManager& manager = DX12ResourceManager::GetDX12DeviceResourceManager();
		DX12RenderDeviceTexture2DCopyRequest* req = new DX12RenderDeviceTexture2DCopyRequest(this, image, desc);
		manager.AddRequest(req);
	}
	void DX12Texture2D::TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList)
	{
		if (mState != newState)
		{
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mTexture, mState, newState));
			mState = newState;
		}
	}
}