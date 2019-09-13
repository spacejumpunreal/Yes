#include "Runtime/Public/Platform/DX12/DX12RenderDeviceModule.h"
#include "Runtime/Platform/DX12/DX12Parameters.h"
#include "Runtime/Platform/DX12/DX12RenderDeviceResources.h"
#include "Runtime/Platform/DX12/DX12FrameState.h"
#include "Runtime/Platform/DX12/DX12MemAllocators.h"
#include "Runtime/Platform/DX12/DX12RenderPass.h"
#include "Runtime/Platform/DX12/DX12ExecuteContext.h"
#include "Runtime/Platform/DX12/DX12RenderCommand.h"
#include "Runtime/Platform/DX12/DX12DrawcallArgument.h"
#include "Runtime/Platform/DX12/DX12Device.h"
#include "Runtime/Public/Platform/WindowsWindowModule.h"
#include "Runtime/Public/Platform/DXUtils.h"
#include "Runtime/Public/Memory/ObjectPool.h"
#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Misc/Debug.h"

#include "Runtime/Public/Misc/BeginExternalIncludeGuard.h"
#include "Windows.h"
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "Runtime/Public/Misc/EndExternalIncludeGuard.h"

#define VALIDATION_LEVEL 1

namespace Yes
{
	class DX12RenderDeviceModuleImp : public DX12RenderDeviceModule
	{
		//forward declarations
	public:
		//Resource related
		TRef<RenderDeviceConstantBuffer> CreateConstantBuffer(bool isTemp, size_t size) override
		{
			const size_t testMask = 255;
			CheckAlways((size & testMask) == 0, "constant buffer must be 256 bytes aligned");
			IDX12GPUBufferAllocator* allocator;
			if (isTemp)
			{
				DX12FrameState* fs = mFrameStates[mCurrentBackbufferIndex];
				allocator = fs->GetConstantBufferAllocator();
			}
			else
			{
				allocator = &mResourceManager->GetPersistentBufferAllocator();
			}
			auto region = allocator->Allocate(size, 0);
			DX12ConstantBufferArgument* arg = new DX12ConstantBufferArgument(region, isTemp ? nullptr : allocator);
			return arg;
		}
		TRef<RenderDeviceDescriptorHeap> CreateDescriptorHeap(bool isTemp, size_t size) override
		{
			IDX12DescriptorHeapAllocator* allocator;
			if (isTemp)
			{
				DX12FrameState* fs = mFrameStates[mCurrentBackbufferIndex];
				allocator = &fs->GetTempDescriptorHeapAllocator();
			}
			else
			{
				//we need SRV heap and visible
				allocator = &mResourceManager->GetPersistentDescriptorHeapAllocator(ResourceType::Buffer, true);
			}
			auto region = allocator->Allocate(size);
			RenderDeviceDescriptorHeap* arg = new DX12DescriptorHeap(region, isTemp ? nullptr : allocator);
			return arg;
		}
		RenderDeviceMeshRef CreateMeshSimple(SharedBufferRef& vertex, SharedBufferRef& index, size_t vertexStride, size_t indexCount, size_t indexStride) override
		{
			D3D12_RESOURCE_DESC descs[2];
			size_t streamSizes[] = { vertex->GetSize(), index->GetSize(), };
			ISharedBuffer* buffers[] = { vertex.GetPtr(), index.GetPtr() };
			size_t strides[] = { vertexStride, indexStride };
			DX12Mesh* mesh = new DX12Mesh(streamSizes,strides, indexCount, descs);
			mesh->AsyncInit(buffers, vertexStride, indexStride, descs);
			return mesh;
		}
		RenderDeviceTextureRef CreateTexture2D(size_t width, size_t height, TextureFormat format, TextureUsage usage, RawImage* image)
		{
			if (image != nullptr && width == 0 && height == 0)
			{
				width = image->GetWidth();
				height = image->GetHeight();
			}
			D3D12_RESOURCE_DESC desc;
			DX12Texture2D* texture = new DX12Texture2D(width, height, format, usage, &desc);
			if (image != nullptr)
			{
				texture->AsyncInit(image, &desc);
			}
			return texture;
		}
		RenderDevicePSORef CreatePSOSimple(RenderDevicePSODesc& desc) override
		{
			DX12PSO* pso = new DX12PSO(mDevice.GetPtr(), desc);
			return pso;
		}
		RenderDeviceShaderRef CreateShaderSimple(SharedBufferRef& textBlob, const char* registeredName) override
		{
			DX12Shader* shader = new DX12Shader(mDevice.GetPtr(), (const char*)textBlob->GetData(), textBlob->GetSize(), registeredName);
			return shader;
		}

		//Command related
		void BeginFrame() override
		{
			++mCurrentFrameIndex;
			mCurrentBackbufferIndex = mSwapChain->GetCurrentBackBufferIndex();
			mFrameStates[mCurrentBackbufferIndex]->WaitGPUAndCleanup();
		}
		void EndFrame() override
		{
			//TODO: need transit backbuffer to present
			DX12FrameState* fs = mFrameStates[mCurrentBackbufferIndex];
			fs->CPUFinish();
			mSwapChain->Present(1, 0);
		}
		RenderDevicePass* AllocPass(size_t argsCount) override
		{
			DX12RenderPass* pass = mPassPool.Allocate();
			pass->Init(GetCurrentFrameState(), &mRenderCommandPool, argsCount);
			return pass;
		}
		V2F GetScreenSize() override
		{
			return V2F((float)mScreenWidth, (float)mScreenHeight);
		}
		int GetFrameIndex() override
		{
			return mCurrentFrameIndex;
		}
		void Start(System*) override
		{
			COMRef<IDXGIFactory4> factory;
			HWND wh;
			{//device
				WindowsWindowModule* wm = GET_MODULE(WindowsWindowModule);
				wh = *(HWND*)wm->GetWindowHandle();
				wm->GetWindowRect(mScreenWidth, mScreenHeight);
				UINT dxgiFactoryFlags = 0;
				COMRef<ID3D12Debug> debugController;
				COMRef<ID3D12Debug1> debugController1;
				if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
				{
#if VALIDATION_LEVEL >= 1
					debugController->EnableDebugLayer();
					dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
#if VALIDATION_LEVEL >= 2
					debugController->QueryInterface(IID_PPV_ARGS(&debugController1));
					debugController1->SetEnableGPUBasedValidation(true);
#endif
				}
				CheckSucceeded(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
				COMRef<IDXGIAdapter1> hardwareAdapter;
				auto featureLevel = D3D_FEATURE_LEVEL_12_1;
				GetHardwareAdapter(factory.GetPtr(), &hardwareAdapter, featureLevel);
				CheckSucceeded(D3D12CreateDevice(hardwareAdapter.GetPtr(), featureLevel, IID_PPV_ARGS(&mDevice)));
				RegisterDX12Device(mDevice.GetPtr());
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
				swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
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
			std::vector<DX12Texture2D*> backBuffers;
			{
				for (int i = 0; i < mFrameCounts; ++i)
				{
					ID3D12Resource* resource;
					CheckSucceeded(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&resource)));

					wchar_t tempNameBuffer[1024];
					swprintf(tempNameBuffer, 1024, L"BackBuffer%d", i);
					DX12Texture2D* bb = new DX12Texture2D(resource, TextureFormat::R8G8B8A8_UNORM, TextureUsage::RenderTarget);
					bb->SetName(tempNameBuffer);
					backBuffers.push_back(bb);
				}
			}
			{//setup frame states
				for (int i = 0; i < NFrames; ++i)
				{
					mFrameStates[i] = new DX12FrameState(mDevice.GetPtr(), backBuffers[i], m3DCommandQueue.GetPtr(), i);
				}
			}
		}
		void ExecutePass(RenderDevicePass* renderPass) override
		{
			DX12RenderPass* pass = (DX12RenderPass*)renderPass;
			DX12FrameState* state = GetCurrentFrameState();
			DX12RenderPassContext ctx(state->GetCommandManager().ResetAndGetCommandList(), pass);
			pass->Execute(ctx);
			state->GetCommandManager().CloseAndExecuteCommandList();
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
			return mFrameStates[mCurrentBackbufferIndex];
		}

	private:
		COMRef<IDXGISwapChain3> mSwapChain;
		COMRef<ID3D12Device> mDevice;
		COMRef<ID3D12CommandQueue> m3DCommandQueue;
		
		//frame stats
		int mCurrentBackbufferIndex = 0;
		int mCurrentFrameIndex =  - 1;
		static const int NFrames = 3;
		DX12FrameState* mFrameStates[NFrames];

		//pass and drawcall
		ObjectPool<DX12RenderPass> mPassPool;

		//submodules
		DX12ResourceManager* mResourceManager;
		DX12RenderCommandPool mRenderCommandPool;
		//some consts
		int mFrameCounts;
		int mScreenWidth;
		int mScreenHeight;
		size_t mAllocatorBlockSize;

	public:
		DEFINE_MODULE_IN_CLASS(DX12RenderDeviceModule, DX12RenderDeviceModuleImp);
	};
	DEFINE_MODULE_REGISTRY(DX12RenderDeviceModule, DX12RenderDeviceModuleImp, 500);


}
