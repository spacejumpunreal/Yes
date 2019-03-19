#pragma once
#include "Yes.h"
#include "Concurrency/Thread.h"
#include "Concurrency/MultiThreadQueue.h"

#include <deque>
#include "Windows.h"
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

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
	class IDX12GPUMemoryAllocator;
	class IDX12GPUMemoryRegion
	{
	public:
		ID3D12Heap* GetHeap() { return mHeap; }
		UINT64 GetOffset() { return mOffset; }
		void Free()
		{
			Allocator->Free(this);
		}
	protected:
		ID3D12Heap* mHeap;
		UINT64 mOffset;
		UINT64 mStart;
		UINT64 mSize;
		IDX12GPUMemoryAllocator* Allocator;
		friend IDX12GPUMemoryAllocator;
	};
	class IDX12GPUMemoryAllocator
	{
	public:
		static const UINT64 ALIGN_4K = 4 * 1024;
		static const UINT64 ALIGN_64K = 64 * 1024;
		static const UINT64 ALIGN_4M = 4 * 1024 * 1024;
		virtual const IDX12GPUMemoryRegion* Allocate(UINT64 size, UINT64 alignment) = 0;
		virtual void Free(const IDX12GPUMemoryRegion* region) = 0;
	public:
		static UINT64 CalcAlignedSize(UINT64 currentOffset, UINT64 alignment, UINT64 size)
		{
			return GetNextAlignedOffset(currentOffset, alignment) + size - currentOffset;
		}
		static UINT64 GetNextAlignedOffset(UINT64 offset, UINT64 alignment)
		{
			return (offset + alignment - 1) & ~(alignment - 1);
		}
	protected:

	};

	class IDX12ResourceCreateRequest
	{
	public:
		virtual void StartCreation() = 0;
		virtual void FinishCreation() = 0;
		virtual ~IDX12ResourceCreateRequest() = 0;
	};

	class DX12AsyncResourceCreator
	{
		const size_t MAX_BATCH_SIZE = 16;
		const size_t AllocatorBlockSize = 16 * 1024 * 1024;
	public:
		DX12AsyncResourceCreator(ID3D12Device* dev)
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
			mUploadTempBufferAllocator = new DX12LinearBlockAllocator(
				HeapCreator(MemoryAccessCase::CPUUpload, dev),
				AllocatorBlockSize);
		}
		void AddRequest(IDX12ResourceCreateRequest* request)
		{
			mStartQueue.PushBack(request);
		}
		static void Entry(void* s)
		{
			auto self = (DX12AsyncResourceCreator*)s;
			self ->Loop();
		}
		void Loop()
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
						delete (*it);
					}
				}
				{//send command list to gpu
					CheckSucceeded(mCommandAllocator->Reset());
					CheckSucceeded(mCommandList->Close());
				}
				
			}
		}
		DX12LinearBlockAllocator& GetTempBufferAllocator()
		{
			return *mUploadTempBufferAllocator;
		}
	private:
		ID3D12Device* mDevice;
		DX12LinearBlockAllocator* mUploadTempBufferAllocator;
		COMRef<ID3D12Fence> mFence;
		COMRef<ID3D12CommandQueue> mCopyCommandQueue;
		COMRef<ID3D12CommandAllocator> mCommandAllocator;
		COMRef<ID3D12GraphicsCommandList> mCommandList;
		std::vector<IDX12ResourceCreateRequest*> mPopResults;
		MultiThreadQueue<IDX12ResourceCreateRequest*> mStartQueue;
		UINT64 mExpectedFenceValue;
		HANDLE mFenceEvent;
		Thread mResourceWorker;
	};


	class DX12RenderDeviceConstantBuffer : public RenderDeviceConstantBuffer
	{
	public:
		DX12RenderDeviceConstantBuffer(size_t size)
			: mData(malloc(size))
		{
		}
		~DX12RenderDeviceConstantBuffer()
		{
			free(mData);
		}
		bool IsReady()
		{
			return true;
		}
		void* Data() { return mData; }
	private:
		void* mData;
	};
	class DX12RenderDeviceShader : public RenderDeviceShader
	{
	public:
		DX12RenderDeviceShader(ID3D12Device* dev, const char* body, size_t size, const char* name)
		{
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
			mVS = DoCompileShader(body, size, name, "VSMain", "vs_5_0", compileFlags);
			mPS = DoCompileShader(body, size, name, "PSMain", "ps_5_0", compileFlags);
			COMRef<ID3DBlob> blob;
			CheckSucceeded(D3DGetBlobPart(mPS->GetBufferPointer(), mPS->GetBufferSize(), D3D_BLOB_ROOT_SIGNATURE, 0, &blob));
			CheckSucceeded(dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));

		}
		bool IsReady() override
		{
			return true;
		}
		~DX12RenderDeviceShader() override
		{
			CheckAlways(false);//not suppose to delete shader
		}
		COMRef<ID3DBlob> mVS;
		COMRef<ID3DBlob> mPS;
		COMRef<ID3D12RootSignature> mRootSignature;
	};

	class DX12RenderDevicePSO : public RenderDevicePSO
	{
	public:
		DX12RenderDevicePSO(ID3D12Device* dev, RenderDevicePSODesc& desc)
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
		bool IsReady()
		{
			return true;
		}
		void Apply(ID3D12GraphicsCommandList* cmdList)
		{
			cmdList->SetGraphicsRootSignature(mRootSignature.GetPtr());
			cmdList->SetPipelineState(mPSO.GetPtr());
		}
	private:
		COMRef<ID3D12PipelineState> mPSO;
		COMRef<ID3D12RootSignature> mRootSignature;
		CD3DX12_VIEWPORT mViewPort;
		CD3DX12_RECT mScissor;
	};

	class DX12RenderDeviceMesh : public RenderDeviceMesh
	{
	public:
		DX12RenderDeviceMesh(ID3D12Device* dev, void* vb, size_t vbSize, void* ib, size_t ibSize, IDX12GPUMemoryRegion* mem)
			: mIsReady(false)
		{
			//create cpu writable buffer
			//dev->CreatePlacedResource()
			//write to cpu buffer
			//create gpu buffer
			//queue things
		}
		void Apply(ID3D12GraphicsCommandList* cmdList)
		{}
	private:
		COMRef<ID3D12Resource> mVertexBuffer;
		COMRef<ID3D12Resource> mIndexBuffer;
		bool mIsReady;
	};
	class DX12AsyncResourceCreator;
	class DX12RenderDeviceMeshCreateRequest : public IDX12ResourceCreateRequest
	{
	public:
		DX12RenderDeviceMeshCreateRequest()
		{}
		virtual void StartCreation(DX12AsyncResourceCreator* creator)
		{
		}
		virtual void FinishCreation(DX12AsyncResourceCreator* creator)
		{
		}
		virtual ~DX12RenderDeviceMeshCreateRequest()
		{
		}
	private:
		ID3D12Device* mDevice;
		IDX12GPUMemoryRegion
	};

}