#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Platform/DX12/DX12Allocators.h"
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
		D3D12_INPUT_ELEMENT_DESC VF_P3F_T2F_LAYOUT[] =
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
		, mPopResults(MAX_BATCH_SIZE)
		, mFenceEvent(CreateEvent(nullptr, FALSE, FALSE, L"AsyncResourceCreator::mFenceEvent"))
		, mResourceWorker(Entry, this)
	{
		if (mFenceEvent == nullptr)
		{
			HRESULT_FROM_WIN32(GetLastError());
			CheckAlways(false);
		}
		mUploadTempBufferAllocator = CreateDX12LinearBlockAllocator(
			HeapCreator(MemoryAccessCase::CPUUpload, dev),
			AllocatorBlockSize);
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

		std::vector<IDX12ResourceCreateRequest*> waitFinishQueue;

		while (true)
		{
			mPopResults.clear();
			mStartQueue.Pop(MAX_BATCH_SIZE, mPopResults);
			CheckSucceeded(mCommandList->Reset(mCommandAllocator.GetPtr(), nullptr));
			for (auto it = mPopResults.begin(); it != mPopResults.end(); ++it)
			{
				(*it)->StartCreation();
			}
			{//wait for last operation to finish
				if (mFence->GetCompletedValue() != mExpectedFenceValue)
				{
					CheckSucceeded(mFence->SetEventOnCompletion(mExpectedFenceValue, mFenceEvent));
				}
			}
			{//finish them
				for (auto it = waitFinishQueue.begin(); it != waitFinishQueue.end(); ++it)
				{
					(*it)->FinishCreation();
				}
			}
			{//send command list to gpu
				CheckSucceeded(mCommandAllocator->Reset());
				CheckSucceeded(mCommandList->Close());
			}

		}
	}

	//shader
	DX12RenderDeviceShader::DX12RenderDeviceShader(ID3D12Device* dev, const char* body, size_t size, const char* name)
	{
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		mVS = DoCompileShader(body, size, name, "VSMain", "vs_5_0", compileFlags);
		mPS = DoCompileShader(body, size, name, "PSMain", "ps_5_0", compileFlags);
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
	DX12RenderDeviceMesh::DX12RenderDeviceMesh(ID3D12Device* dev, void* vb, size_t vbSize, void* ib, size_t ibSize, IDX12GPUMemoryRegion* mem)
		: mIsReady(false)
	{
		//create cpu writable buffer
		//dev->CreatePlacedResource()
		//write to cpu buffer
		//create gpu buffer
		//queue things
	}
	void DX12RenderDeviceMesh::Apply(ID3D12GraphicsCommandList* cmdList)
	{}
	class DX12RenderDeviceMeshCreateRequest : public IDX12ResourceCreateRequest
	{
	public:
		DX12RenderDeviceMeshCreateRequest(
			DX12RenderDeviceMesh* resource,
			ISharedBuffer* CPUData[2],
			const IDX12GPUMemoryRegion* heapRegion[2])
			: mResource(resource)
		{
			for (int i = 0; i < 2; ++i)
			{
				mCPUData[i] = CPUData[i];
			}
			for (int i = 0; i < 2; ++i)
			{
				mHeapRegion[i] = heapRegion[i];
			}
		}
		virtual void StartCreation(DX12AsyncResourceCreator* creator)
		{
			IDX12GPUMemoryAllocator& tempAllocator = creator->GetTempBufferAllocator();
			for (int i = 0; i < 2; ++i)
			{
				CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(mCPUData[i]->GetSize());
				D3D12_RESOURCE_ALLOCATION_INFO info = mDevice->GetResourceAllocationInfo(0, 1, &desc);
				mTempBuffer[i] = tempAllocator.Allocate(info.SizeInBytes, info.Alignment);
			}
		}
		virtual void FinishCreation(DX12AsyncResourceCreator* creator)
		{
			mResource->mIsReady = true;
			for (int i = 0; i < 2; ++i)
			{
				mTempBuffer[i]->Free();
			}
			delete this;
		}
	private:
		ID3D12Device* mDevice;
		TRef<DX12RenderDeviceMesh> mResource;
		SharedBufferRef mCPUData[2];
		const IDX12GPUMemoryRegion* mHeapRegion[2];
		const IDX12GPUMemoryRegion* mTempBuffer[2];
	};
}