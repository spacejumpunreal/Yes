#include "Platform/DX12/DX12DemoModule.h"
#include "Platform/WindowsWindowModule.h"
#include "Core/FileModule.h"
#include "Core/TickModule.h"
#include "Platform/DXUtils.h"
#include "Core/System.h"
#include "Misc/Utils.h"
#include "Misc/Debug.h"

#include "Windows.h"
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

namespace Yes
{
	struct ConstantData
	{
		float param0[4];
		float param1[4];
	};
	class DX12DemoModuleImp : public IModule, public ITickable
	{
	private:
		COMRef<IDXGISwapChain3> mSwapChain;
		COMRef<ID3D12Device> mDevice;
		COMRef<ID3D12CommandQueue> mCommandQueue;
		COMRef<ID3D12Resource>* mBackbuffers;
		COMRef<ID3D12DescriptorHeap> mRTVHeap;
		COMRef<ID3D12CommandAllocator> mCommandAllocator;
		COMRef<ID3D12GraphicsCommandList> mCommandList;
		COMRef<ID3D12Fence> mFence;
		HANDLE mFenceEvent;

		//PSO resources
		COMRef<ID3DBlob> mVS;
		COMRef<ID3DBlob> mPS;
		COMRef<ID3D12RootSignature> mRootSignature;
		COMRef<ID3D12PipelineState> mPipelineState;
		CD3DX12_VIEWPORT mViewport;
		CD3DX12_RECT mScissorRect;

		//resource
		COMRef<ID3D12Resource> mVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		COMRef<ID3D12Resource> mConstantBuffer;
		COMRef<ID3D12DescriptorHeap> mSRVHeap;
		UINT8* mCBStart;

		COMRef<ID3D12Resource> mTexture;
		COMRef<ID3D12Resource> mUploadHeap;

		//fence
		UINT64 mFenceValue;

		UINT mRTVDescriptorSize;
		float mClearColor[4];
		int mFrameCounts;
		int mFrameIdx;
	private:
		void InitDevice()
		{
			WindowsWindowModule* wm = GET_MODULE(WindowsWindowModule);
			HWND wh = *(HWND*)wm->GetWindowHandle();
			UINT dxgiFactoryFlags = 0;
			COMRef<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
			COMRef<IDXGIFactory4> factory;
			CheckSucceeded(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

			COMRef<IDXGIAdapter1> hardwareAdapter;
			auto featureLevel = D3D_FEATURE_LEVEL_12_1;
			GetHardwareAdapter(factory.GetPtr(), &hardwareAdapter, featureLevel);
			CheckSucceeded(D3D12CreateDevice(hardwareAdapter.GetPtr(), featureLevel, IID_PPV_ARGS(&mDevice)));

			//command queue
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			CheckSucceeded(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.BufferCount = mFrameCounts;
			int width, height;
			wm->GetWindowRect(width, height);
			swapChainDesc.Width = width;
			swapChainDesc.Height = height;
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.SampleDesc.Count = 1;
			COMRef<IDXGISwapChain1> sc;
			CheckSucceeded(factory->CreateSwapChainForHwnd(
				mCommandQueue.GetPtr(),
				wh,
				&swapChainDesc,
				nullptr,
				nullptr,
				&sc));
			sc.As(mSwapChain);
			CheckSucceeded(factory->MakeWindowAssociation(wh, DXGI_MWA_NO_ALT_ENTER));

			mFenceValue = 1;
			mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}
		void InitPipeline()
		{
			//backbuffer view and descriptor heap
			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.NumDescriptors = mFrameCounts;
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				CheckSucceeded(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mRTVHeap)));
				D3D12_CPU_DESCRIPTOR_HANDLE heapStart = mRTVHeap->GetCPUDescriptorHandleForHeapStart();
				UINT incrementSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				for (int i = 0; i < mFrameCounts; ++i)
				{
					CheckSucceeded(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBackbuffers[i])));
					mDevice->CreateRenderTargetView(mBackbuffers[i].GetPtr(), nullptr, heapStart);
					heapStart.ptr += incrementSize;
				}
			}
			//command allocator
			CheckSucceeded(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator)));
			//command list
			CheckSucceeded(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.GetPtr(), nullptr, IID_PPV_ARGS(&mCommandList)));
			CheckSucceeded(mCommandList->Close());
			//event
			CheckSucceeded(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
			mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (mFenceEvent == nullptr)
			{
				HRESULT_FROM_WIN32(GetLastError());
				CheckAlways(false);
			}
		}
		void InitPSO()
		{
			FileModule* fm = GET_MODULE(FileModule);
			const char* shaderFileName = "FirstStep.hlsl";
			SharedBufferRef shaderText = fm->ReadFileContent(shaderFileName);
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
			mVS = DoCompileShader((const char*)shaderText->GetData(), shaderText->GetSize(), shaderFileName, "VSMain", "vs_5_0", compileFlags);
			mPS = DoCompileShader((const char*)shaderText->GetData(), shaderText->GetSize(), shaderFileName, "PSMain", "ps_5_0", compileFlags);
			// Define the vertex input layout.
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			//create root signature
			{
				//TODO
				D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
				if (FAILED(mDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
				{
					featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
				}
				const UINT nRootParams = 1;
				CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
				CD3DX12_ROOT_PARAMETER1 rootParameters[nRootParams];
				ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 0);
				ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 1);
				rootParameters[0].InitAsDescriptorTable(2, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;

				D3D12_STATIC_SAMPLER_DESC sampler = {};
				sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
				sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				sampler.MipLODBias = 0;
				sampler.MaxAnisotropy = 0;
				sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
				sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
				sampler.MinLOD = 0.0f;
				sampler.MaxLOD = D3D12_FLOAT32_MAX;
				sampler.ShaderRegister = 0;
				sampler.RegisterSpace = 0;
				sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

				D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
					D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
					| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
					| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
					| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
				rootSignatureDesc.Init_1_1(nRootParams, rootParameters, 1, &sampler, rootSignatureFlags);

				COMRef<ID3DBlob> signature, error;
				CheckSucceeded(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
				CheckSucceeded(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
			}

			//create pso
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				psoDesc.InputLayout = { inputElementDescs, ARRAY_COUNT(inputElementDescs) };
				psoDesc.pRootSignature = mRootSignature.GetPtr();
				psoDesc.VS = CD3DX12_SHADER_BYTECODE(mVS.GetPtr());
				psoDesc.PS = CD3DX12_SHADER_BYTECODE(mPS.GetPtr());
				psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				psoDesc.DepthStencilState.DepthEnable = FALSE;
				psoDesc.DepthStencilState.StencilEnable = FALSE;
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				psoDesc.SampleDesc.Count = 1;
				CheckSucceeded(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState)));
			}
			// view and scissor
			{
				int width, height;
				WindowsWindowModule* wm = GET_MODULE(WindowsWindowModule);
				wm->GetWindowRect(width, height);
				mViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);
				mScissorRect = CD3DX12_RECT(0, 0, width, height);
			}
			
		}
		void InitResources()
		{
			//vertex buffer
			struct Vertex
			{
				float position[3];
				float uv[2];
			};
			float ratio = 0.7f;
			float zOffset = 1.0f;
			Vertex triangleVertices[] =
			{
				{{0, 0, zOffset}, {0, 0}},
				{{0, ratio, zOffset}, {0, ratio}},
				{{ratio, 0, zOffset}, {ratio, 0}},

				{{0, ratio, zOffset}, {0, ratio}},
				{{ratio, ratio, zOffset}, {ratio, ratio}},
				{{ratio, 0, zOffset}, {ratio, 0}},
			};

			{
				UINT vertexBufferSize = sizeof(triangleVertices);
				CheckSucceeded(mDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&mVertexBuffer)));

				UINT8* vertexBufferStart;
				CD3DX12_RANGE readRange(0, 0);
				CheckSucceeded(mVertexBuffer->Map(0, &readRange, (void**)&vertexBufferStart));
				memcpy(vertexBufferStart, triangleVertices, vertexBufferSize);
				mVertexBuffer->Unmap(0, nullptr);

				mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
				mVertexBufferView.StrideInBytes = sizeof(Vertex);
				mVertexBufferView.SizeInBytes = vertexBufferSize;
			}
			//shader descriptor heap
			{
				D3D12_DESCRIPTOR_HEAP_DESC cbHeapDesc = {};
				cbHeapDesc.NumDescriptors = 2;
				cbHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				cbHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				CheckSucceeded(mDevice->CreateDescriptorHeap(&cbHeapDesc, IID_PPV_ARGS(&mSRVHeap)));
			}
			//constant buffer
			{
				UINT constantBufferSize = CalcConstantBufferSize<ConstantData>();
				CheckSucceeded(mDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&mConstantBuffer)));

				D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
				desc.BufferLocation = mConstantBuffer->GetGPUVirtualAddress();
				desc.SizeInBytes = constantBufferSize;
				mDevice->CreateConstantBufferView(&desc, mSRVHeap->GetCPUDescriptorHandleForHeapStart());
			}
			//texture
			{
				InitTexture();
				D3D12_RESOURCE_DESC rdesc = mTexture->GetDesc();
				D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
				desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				desc.Format = rdesc.Format;
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				desc.Texture2D.MipLevels = 1;
				D3D12_CPU_DESCRIPTOR_HANDLE heapOffset = mSRVHeap->GetCPUDescriptorHandleForHeapStart();
				heapOffset.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				mDevice->CreateShaderResourceView(mTexture.GetPtr(), &desc, heapOffset);
			}
		}
		void InitTexture()
		{
			const UINT TextureWidth = 256;
			const UINT TexturePixelSize = 4;
			const UINT TextureHeight = 256;

			const UINT rowPitch = TextureWidth * TexturePixelSize;
			const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
			const UINT cellHeight = TextureWidth >> 3;    // The height of a cell in the checkerboard texture.
			const UINT textureSize = rowPitch * TextureHeight;

			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.Width = TextureWidth;
			textureDesc.Height = TextureHeight;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			CheckSucceeded(mDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&mTexture)));

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mTexture.GetPtr(), 0, 1);
			CheckSucceeded(mDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&mUploadHeap)));

			//generate data
			std::vector<UINT8> data(textureSize);
			UINT8* pData = &data[0];

			for (UINT n = 0; n < textureSize; n += TexturePixelSize)
			{
				UINT x = n % rowPitch;
				UINT y = n / rowPitch;
				UINT i = x / cellPitch;
				UINT j = y / cellHeight;

				if (i % 2 == j % 2)
				{
					pData[n] = 0x00;        // R
					pData[n + 1] = 0x00;    // G
					pData[n + 2] = 0x00;    // B
					pData[n + 3] = 0xff;    // A
				}
				else
				{
					pData[n] = 0xff;        // R
					pData[n + 1] = 0xff;    // G
					pData[n + 2] = 0xff;    // B
					pData[n + 3] = 0xff;    // A
				}
			}

			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = pData;
			textureData.RowPitch = rowPitch;
			textureData.SlicePitch = textureSize;

			mCommandList->Reset(mCommandAllocator.GetPtr(), nullptr);

			UpdateSubresources(
				mCommandList.GetPtr(),
				mTexture.GetPtr(),
				mUploadHeap.GetPtr(),
				0,
				0,
				1,
				&textureData);
			mCommandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mTexture.GetPtr(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
				);
			CheckSucceeded(mCommandList->Close());
			ID3D12CommandList* commandLists[] = { mCommandList.GetPtr() };
			mCommandQueue->ExecuteCommandLists(ARRAY_COUNT(commandLists), commandLists);
			SyncGPU();
			
		}

		template<typename ConstantBufferStruct>
		constexpr UINT CalcConstantBufferSize()
		{
			UINT sz = sizeof(ConstantBufferStruct);
			UINT step = 256;
			UINT mask = ~(step - 1);
			return (sz + step - 1) & mask;
		}
		void UpdateConstantBuffer(int f)
		{
			const int limit = 256;
			CD3DX12_RANGE readRange(0, 0);
			CheckSucceeded(mConstantBuffer->Map(0, &readRange, (void**)&mCBStart));
			ConstantData cd;
			f = f % limit;
			for (int i = 0; i < 8; ++i)
			{
				cd.param0[i] = (float)(f + i);
			}
			cd.param1[3] = (float)limit;
			memcpy(mCBStart, &cd, CalcConstantBufferSize<ConstantData>());
		}
		void SyncGPU()
		{
			UINT64 fence = mFenceValue;
			CheckSucceeded(mCommandQueue->Signal(mFence.GetPtr(), fence));
			++mFenceValue;
			if (mFence->GetCompletedValue() != fence)
			{
				CheckSucceeded(mFence->SetEventOnCompletion(fence, mFenceEvent));
				WaitForSingleObject(mFenceEvent, INFINITE);
			}
		}
		void PopulateCommandList()
		{
			CheckSucceeded(mCommandAllocator->Reset());
			CheckSucceeded(mCommandList->Reset(mCommandAllocator.GetPtr(), mPipelineState.GetPtr()));

			UINT frameIndex = mSwapChain->GetCurrentBackBufferIndex();
			mCommandList->SetGraphicsRootSignature(mRootSignature.GetPtr());
			ID3D12DescriptorHeap* ppHeaps[] = { mSRVHeap.GetPtr() };
			mCommandList->SetDescriptorHeaps(1, ppHeaps);
			mCommandList->SetGraphicsRootDescriptorTable(0, mSRVHeap->GetGPUDescriptorHandleForHeapStart());

			mCommandList->RSSetViewports(1, &mViewport);
			mCommandList->RSSetScissorRects(1, &mScissorRect);
			mCommandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mBackbuffers[frameIndex].GetPtr(),
					D3D12_RESOURCE_STATE_PRESENT,
					D3D12_RESOURCE_STATE_RENDER_TARGET));
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRTVHeap->GetCPUDescriptorHandleForHeapStart(), (INT)frameIndex, mRTVDescriptorSize);
			mCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
			mCommandList->ClearRenderTargetView(rtvHandle, mClearColor, 0, nullptr);
			mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
			mCommandList->DrawInstanced(6, 1, 0, 0);
			mCommandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					mBackbuffers[frameIndex].GetPtr(),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PRESENT));
			CheckSucceeded(mCommandList->Close());
		}

	public:
		virtual void InitializeModule(System* system) override
		{
			mFrameCounts = 3;
			mFrameIdx = 0;
			mBackbuffers = new COMRef<ID3D12Resource>[mFrameCounts];
			mClearColor[0] = 0.0f;
			mClearColor[1] = 0.2f;
			mClearColor[2] = 0.4f;
			mClearColor[3] = 1.0f;

			InitDevice();
			InitPipeline();
			InitResources();
			InitPSO();
			SyncGPU();
			TickModule* tickModule = GET_MODULE(TickModule);
			tickModule->AddTickable(this);
		}
		virtual void Tick() override
		{
			++mFrameIdx;
			UpdateConstantBuffer(mFrameIdx);
			PopulateCommandList();
			ID3D12CommandList* commandLists[] = { mCommandList.GetPtr() };
			mCommandQueue->ExecuteCommandLists(ARRAY_COUNT(commandLists), commandLists);
			CheckSucceeded(mSwapChain->Present(1, 0));
			SyncGPU();
		}
		DEFINE_MODULE_IN_CLASS(DX12DemoModule, DX12DemoModuleImp);
	};
	DEFINE_MODULE_REGISTRY(DX12DemoModule, DX12DemoModuleImp, 500);
}
