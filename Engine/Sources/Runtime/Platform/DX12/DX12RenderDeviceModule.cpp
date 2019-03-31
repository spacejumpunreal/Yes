#include "Platform/DX12/DX12RenderDeviceModule.h"
#include "Platform/DX12/DX12Parameters.h"
#include "Platform/DX12/DX12RenderDeviceResources.h"
#include "Platform/DX12/DX12FrameState.h"
#include "Platform/DX12/DX12MemAllocators.h"
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
	class DX12RenderDevicePass : public RenderDevicePass
	{
	public:
		void Init(DX12FrameState* state)
		{
			FrameState = state;
		}
		void Reset() override
		{
			CheckAlways(Commands.empty());
			Commands.clear();
			mName.clear();
			NeedClearColor = true;
			ClearColor = V3F(0, 0, 0);
			NeedClearDepth = true;
			ClearDepth = 1.0f;
			for (int i = 0; i < MaxRenderTargets; ++i)
			{
				OutputTarget[i] = nullptr;
			}
			FrameState = nullptr;
			ConstantBuffer = {};
		}
		void AddCommand(RenderDeviceCommand* cmd) override
		{
			Commands.push_back(cmd);
		}
		void SetOutput(TRef<RenderDeviceRenderTarget>& renderTarget, int idx) override
		{
			IDX12RenderDeviceRenderTarget* rt = dynamic_cast<IDX12RenderDeviceRenderTarget*>(renderTarget.GetPtr());
			OutputTarget[idx] = rt;
		}
		void SetDepthStencil(TRef<RenderDeviceDepthStencil>& depthStencil)
		{
		}
		void SetClearValue(bool needClearColor, const V3F& clearColor, bool needClearDepth, float depth)
		{
			NeedClearColor = needClearColor;
			NeedClearDepth = needClearDepth;
			ClearColor = clearColor;
			ClearDepth = depth;
		}
		void SetGlobalConstantBuffer(void* data, size_t size)
		{
			ConstantBuffer = FrameState->GetConstantBufferManager().Allocate(size);
			void* wptr;
			CheckSucceeded(ConstantBuffer.Buffer->Map(0, nullptr, &wptr));
			memcpy(wptr, data, size);
		}
	public:
		std::deque<RenderDeviceCommand*> Commands;
		TRef<IDX12RenderDeviceRenderTarget> OutputTarget[8];
		TRef<DX12RenderDeviceDepthStencil> DepthStencil;
		bool NeedClearColor;
		bool NeedClearDepth;
		V3F ClearColor;
		float ClearDepth;
		DX12FrameState* FrameState;
		AllocatedCBV ConstantBuffer;
	};
	class DX12RenderDeviceDrawcall : public RenderDeviceDrawcall
	{
	public:
		void Init(DX12FrameState* state)
		{
			FrameState = state;
		}
		void Reset()
		{
			Mesh = nullptr;
			ConstantBuffer = {};
			PSO = nullptr;
		}
		void SetMesh(RenderDeviceMesh* mesh) override
		{
			Mesh = mesh;
		}
		void SetTextures(int idx, RenderDeviceTexture* texture) override
		{}
		void SetConstantBuffer(void* data, size_t size)
		{
			ConstantBuffer = FrameState->GetConstantBufferManager().Allocate(size);
			void* wptr;
			CheckSucceeded(ConstantBuffer.Buffer->Map(0, nullptr, &wptr));
			memcpy(wptr, data, size);
		}
		void SetPSO(RenderDevicePSO* pso) override
		{
			PSO = pso;
		}
	public:
		TRef<DX12RenderDeviceMesh> Mesh;
		AllocatedCBV ConstantBuffer;
		TRef<DX12RenderDevicePSO> PSO;
		DX12FrameState* FrameState;
	};

	class DX12RenderDeviceModuleImp : public DX12RenderDeviceModule
	{
		//forward declarations
		
	public:
		//Resource related
		RenderDeviceResourceRef CreateConstantBufferSimple(size_t size) override
		{
			DX12RenderDeviceConstantBuffer* cb = new DX12RenderDeviceConstantBuffer(size);
			return cb; 
		}
		RenderDeviceResourceRef CreateMeshSimple(SharedBufferRef& vertex, SharedBufferRef& index) override
		{
			ISharedBuffer* buffers[] = {vertex.GetPtr(), index.GetPtr()};
			DX12RenderDeviceMesh* mesh = new DX12RenderDeviceMesh(mResourceManager, buffers);
			return mesh;
		}
		RenderDeviceResourceRef CreatePSOSimple(RenderDevicePSODesc& desc) override
		{
			DX12RenderDevicePSO* pso = new DX12RenderDevicePSO(mDevice.GetPtr(), desc);
			return pso;
		}
		RenderDeviceResourceRef CreateShaderSimple(SharedBufferRef& textBlob, const char* registeredName) override
		{
			DX12RenderDeviceShader* shader = new DX12RenderDeviceShader(mDevice.GetPtr(), (const char*)textBlob->GetData(), textBlob->GetSize(), registeredName);
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
			++mCurrentFrameIndex;
			mCurrentFrameIndex %= NFrames;
			mFrameStates[mCurrentFrameIndex]->WaitForFrame();
		}
		void EndFrame() override
		{
			mSwapChain->Present(1, 0);
		}
		RenderDevicePass* AllocPass() override
		{
			DX12RenderDevicePass* pass = mPassPool.Allocate();
			pass->Init(GetCurrentFrameState());
			return pass;
		}
		RenderDeviceDrawcall* AllocDrawcall() override
		{
			DX12RenderDeviceDrawcall* call = mDrawcallPool.Allocate();
			call->Init(GetCurrentFrameState());
			return call;
		}
		RenderDeviceBarrier* AllocBarrier() override
		{
			return nullptr;
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
				swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
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
			mResourceManager = new DX12DeviceResourceManager(mDevice.GetPtr());
			std::vector<DX12Backbuffer*> backBuffers;
			{
				for (int i = 0; i < mFrameCounts; ++i)
				{
					ID3D12Resource* resource;
					CheckSucceeded(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&resource)));
					DX12DescriptorHeapSpace space = mResourceManager->GetSyncDescriptorHeapAllocator(ResourceType::RenderTarget).Allocate(1);
					backBuffers.push_back(new DX12Backbuffer(resource, space));
				}
			}
			{//setup frame states
				for (int i = 0; i < NFrames; ++i)
				{
					mFrameStates[i] = new DX12FrameState(mDevice.GetPtr(), backBuffers[i]);
				}
			}
		}
		void ExecutePass(RenderDevicePass* renderPass) override
		{
			auto pass = (DX12RenderDevicePass*)renderPass;
			DX12FrameState* state = GetCurrentFrameState();
			ID3D12GraphicsCommandList* cmdList = state->GetCommandList();
			{//initial setup
				D3D12_CPU_DESCRIPTOR_HANDLE handles[MaxRenderTargets];
				int count = 0;
				for (int i = 0; i < MaxRenderTargets; ++i)
				{
					if (pass->OutputTarget[i].GetPtr() != nullptr)
					{
						handles[i] = pass->OutputTarget[i]->GetHandle();
					}
					else
					{
						count = i;
						break;
					}
				}
				cmdList->OMSetRenderTargets(count, handles, false, nullptr);
			}
			//GOONGOONGOON
			//when done, reset pass and free it
			pass->Reset();
			mPassPool.Deallocate(pass);
		}

		DX12RenderDeviceModuleImp()
			: mPassPool(16)
			, mDrawcallPool(128)
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
		ObjectPool<DX12RenderDevicePass> mPassPool;
		ObjectPool<DX12RenderDeviceDrawcall> mDrawcallPool;

		//submodules
		DX12DeviceResourceManager* mResourceManager;
		
		//descriptor consts
		UINT mRTVIncrementSize;
		UINT mSRVIncrementSize;

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
