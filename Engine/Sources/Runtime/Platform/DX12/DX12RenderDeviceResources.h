#pragma once
#include "Yes.h"
#include "Misc/Thread.h"

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
	struct IDX12GPUMemoryRegion
	{
		ID3D12Heap* Heap;
		UINT64 Offset;
		UINT64 Start;
		UINT64 Size;
		IDX12GPUMemoryAllocator* Allocator;
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

	class DX12AsyncResourceCreator
	{
	public:
		DX12AsyncResourceCreator(ID3D12Device* dev)
			: mDevice(dev)
		{

		}
		static void Entry(void* s)
		{
			auto self = (DX12AsyncResourceCreator*)s;
			self ->Loop();
		}
		void Loop()
		{

		}
	private:
		ID3D12Device* mDevice;
		//a queue with wakeup: concurrent queue
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
			dev->CreatePlacedResource()
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

}