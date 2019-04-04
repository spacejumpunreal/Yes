#include "Platform/DX12/DX12RenderDeviceModule.h"
#include "Platform/DX12/DX12Parameters.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Platform/DX12/DX12FrameState.h"
#include "Platform/DX12/DX12MemAllocators.h"
#include "Platform/DX12/DX12RenderPass.h"
#include "Platform/DX12/DX12ExecuteContext.h"
#include "Platform/DX12/DX12RenderCommand.h"
#include "Platform/WindowsWindowModule.h"
#include "Platform/DXUtils.h"
#include "Memory/ObjectPool.h"
#include "Core/System.h"
#include "Misc/Debug.h"

#include "Windows.h"
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace Yes
{
	class DX12RenderDeviceModuleImp : public DX12RenderDeviceModule
	{
		//forward declarations
		
	public:
		//Resource related
		RenderDeviceResourceRef CreateConstantBufferSimple(size_t size) override
		{
			DX12ConstantBuffer* cb = new DX12ConstantBuffer(size);
			return cb; 
		}
		RenderDeviceResourceRef CreateMeshSimple(SharedBufferRef& vertex, SharedBufferRef& index, size_t vertexStride, size_t indexCount, size_t indexStride) override
		{
			ISharedBuffer* buffers[] = {vertex.GetPtr(), index.GetPtr()};
			DX12Mesh* mesh = new DX12Mesh(mResourceManager, buffers, vertexStride, indexCount, indexStride);
			return mesh;
		}
		RenderDeviceResourceRef CreatePSOSimple(RenderDevicePSODesc& desc) override
		{
			DX12PSO* pso = new DX12PSO(mDevice.GetPtr(), desc);
			return pso;
		}
		RenderDeviceResourceRef CreateShaderSimple(SharedBufferRef& textBlob, const char* registeredName) override
		{
			DX12Shader* shader = new DX12Shader(mDevice.GetPtr(), (const char*)textBlob->GetData(), textBlob->GetSize(), registeredName);
			return shader;
		}
		RenderDeviceResourceRef CreateRenderTarget() override
		{
			return RenderDeviceResourceRef();
		}
		RenderDeviceResourceRef CreteTextureSimple() override
		{
			return RenderDeviceResourceRef();
		}
		//Command related
		void BeginFrame() override
		{
			mCurrentFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
			mFrameStates[mCurrentFrameIndex]->WaitForFrame();
		}
		void EndFrame() override
		{
			//TODO: need transit backbuffer to present
			DX12FrameState* fs = mFrameStates[mCurrentFrameIndex];
			ID3D12GraphicsCommandList* cmdList = fs->ResetAndGetCommandList();
			fs->GetBackbuffer()->TransitToState(D3D12_RESOURCE_STATE_PRESENT, cmdList);
			fs->CloseAndExecuteCommandList();
			mSwapChain->Present(1, 0);
		}
		RenderDevicePass* AllocPass() override
		{
			DX12Pass* pass = mPassPool.Allocate();
			pass->Init(GetCurrentFrameState(), &mRenderCommandPool);
			return pass;
		}
		void Start(System* system) override
		{
			COMRef<IDXGIFactory4> factory;
			HWND wh;
			{//device
				WindowsWindowModule* wm = GET_MODULE(WindowsWindowModule);
				wh = *(HWND*)wm->GetWindowHandle();
				wm->GetWindowRect(mScreenWidth, mScreenHeight);
				UINT dxgiFactoryFlags = 0;
				COMRef<ID3D12Debug> debugController;
				if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
				{
					debugController->EnableDebugLayer();
					dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
				}
				
				CheckSucceeded(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
				COMRef<IDXGIAdapter1> hardwareAdapter;
				auto featureLevel = D3D_FEATURE_LEVEL_12_1;
				GetHardwareAdapter(factory.GetPtr(), &hardwareAdapter, featureLevel);
				CheckSucceeded(D3D12CreateDevice(hardwareAdapter.GetPtr(), featureLevel, IID_PPV_ARGS(&mDevice)));
			}
			InitDX12RuntimeParameters(mDevice.GetPtr());
			{//command queue: 2 queues, 1 for command, 1 for resource copy
				D3D12_COMMAND_QUEUE_DESC queueDesc = {};
				queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				CheckSucceeded(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m3DCommandQueue)));

			}
			{//swapchain
				DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
				swapChainDesc.BufferCount = mFrameCounts;
				swapChainDesc.Width = mScreenWidth;
				swapChainDesc.Height = mScreenHeight;
				swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
				swapChainDesc.SampleDesc.Count = 1;
				COMRef<IDXGISwapChain1> sc;
				CheckSucceeded(
					factory->CreateSwapChainForHwnd(
					m3DCommandQueue.GetPtr(),
					wh,
					&swapChainDesc,
					nullptr,
					nullptr,
					&sc));
				sc.As(mSwapChain);
				CheckSucceeded(factory->MakeWindowAssociation(wh, DXGI_MWA_NO_ALT_ENTER));
			}
			mResourceManager = new DX12ResourceManager(mDevice.GetPtr());
			std::vector<DX12Backbuffer*> backBuffers;
			{
				for (int i = 0; i < mFrameCounts; ++i)
				{
					ID3D12Resource* resource;
					CheckSucceeded(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&resource)));
					backBuffers.push_back(new DX12Backbuffer(resource, mDevice.GetPtr()));
				}
			}
			{//setup frame states
				for (int i = 0; i < NFrames; ++i)
				{
					mFrameStates[i] = new DX12FrameState(mDevice.GetPtr(), backBuffers[i], m3DCommandQueue.GetPtr());
				}
			}
		}
		void ExecutePass(RenderDevicePass* renderPass) override
		{
			DX12Pass* pass = (DX12Pass*)renderPass;
			DX12FrameState* state = GetCurrentFrameState();
			DX12RenderPassContext ctx;
			ctx.CommandList = state->ResetAndGetCommandList();
			ctx.HeapAllocator = &state->GetTempDescriptorHeapAllocator();
			ctx.Backbuffer = state->GetBackbuffer();
			ctx.DefaultViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)mScreenWidth, (float)mScreenHeight);
			ctx.DefaultScissor = CD3DX12_RECT(0, 0, mScreenWidth, mScreenHeight);
			pass->Execute(ctx);
			//when done, reset pass and free it
			state->CloseAndExecuteCommandList();
			pass->Reset();
			mPassPool.Deallocate(pass);
		}

		DX12RenderDeviceModuleImp()
			: mPassPool(16)
		{
			mFrameCounts = 3;
			mAllocatorBlockSize = 32 * 1024 * 1024;
		}
		//roll the wheel
		DX12FrameState* GetCurrentFrameState()
		{
			return mFrameStates[mCurrentFrameIndex];
		}

	private:
		COMRef<IDXGISwapChain3> mSwapChain;
		COMRef<ID3D12Device> mDevice;
		COMRef<ID3D12CommandQueue> m3DCommandQueue;
		
		//frame stats
		int mCurrentFrameIndex = 0;
		static const int NFrames = 3;
		DX12FrameState* mFrameStates[NFrames];

		//pass and drawcall
		ObjectPool<DX12Pass> mPassPool;

		//submodules
		DX12ResourceManager* mResourceManager;
		DX12RenderCommandPool mRenderCommandPool;
		//some consts
		int mFrameCounts;
		int mScreenWidth;
		int mScreenHeight;
		size_t mAllocatorBlockSize;

	private:
	public:
		DEFINE_MODULE_IN_CLASS(DX12RenderDeviceModule, DX12RenderDeviceModuleImp);
	};
	DEFINE_MODULE_REGISTRY(DX12RenderDeviceModule, DX12RenderDeviceModuleImp, 500);


}
