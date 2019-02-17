#include "Platform/DX12/DX12Module.h"
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
	DEFINE_MODULE(DX12Module), public ITickable
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

		//fence
		UINT64 mFenceValue;

		UINT mRTVDescriptorSize;
		float mClearColor[4];
		int mFrameCounts;
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
				CD3DX12_ROOT_SIGNATURE_DESC desc;
				desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				COMRef<ID3DBlob> signature;
				COMRef<ID3DBlob> error;
				CheckSucceeded(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
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
				&CD3DX12_RESOURCE_BARRIER::Transition(mBackbuffers[frameIndex].GetPtr(),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PRESENT));
			CheckSucceeded(mCommandList->Close());
		}

	public:
		virtual void InitializeModule() override
		{
			mFrameCounts = 3;
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
			PopulateCommandList();
			ID3D12CommandList* commandLists[] = { mCommandList.GetPtr() };
			mCommandQueue->ExecuteCommandLists(ARRAY_COUNT(commandLists), commandLists);
			CheckSucceeded(mSwapChain->Present(1, 0));
			SyncGPU();
		}
	};
	DEFINE_MODULE_CREATOR(DX12Module);
}
