#include "Platform/DX12/DX12RenderDeviceModule.h"
#include "Platform/WindowsWindowModule.h"

#include "Platform/DXUtils.h"
#include "Core/System.h"

#include "Windows.h"
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

namespace Yes
{
	class DX12RenderDeviceModule : public IModule, public RenderDevice
	{
	public:
		//Resource related
		virtual RenderDeviceResourceRef CreateMeshSimple(SharedBufferRef& meshBlob) override
		{
			return RenderDeviceResourceRef();
		}
		virtual RenderDeviceResourceRef CreatePSOSimple(VertexFormat vertexFormat, RenderDeviceResourceRef& vs, RenderDeviceResourceRef& ps) override
		{
			return RenderDeviceResourceRef();
		}
		virtual RenderDeviceResourceRef CreateShaderSimple(SharedBufferRef& textBlob) override
		{
			return RenderDeviceResourceRef();
		}
		virtual RenderDeviceResourceRef CreateRenderTarget() override
		{
			return RenderDeviceResourceRef();
		}
		virtual RenderDeviceResourceRef CreteTextureSimple() override
		{
			return RenderDeviceResourceRef();
		}

		//Command related
		virtual void BeginFrame() override
		{
		}
		virtual void EndFrame() override
		{
		}
		virtual RenderDevicePass * AllocPass() override
		{
			return nullptr;
		}
		virtual RenderDeviceDrawcall * AllocDrawcall() override
		{
			return nullptr;
		}
		virtual RenderDeviceBarrier * AllocBarrier() override
		{
			return nullptr;
		}
		virtual void Start() override
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
			{//init descriptor heap offsets
				mSRVIncrementSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				mRTVIncrementSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			}
			{//command queue: 2 queues, 1 for command, 1 for resource copy
				D3D12_COMMAND_QUEUE_DESC queueDesc = {};
				queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				CheckSucceeded(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m3DCommandQueue)));

				queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
				CheckSucceeded(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCopyCommandQueue)));
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
			{//backbuffersviews and descriptor heap
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.NumDescriptors = mFrameCounts;
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				CheckSucceeded(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mBackbufferHeap)));

				CD3DX12_CPU_DESCRIPTOR_HANDLE heapStart(mBackbufferHeap->GetCPUDescriptorHandleForHeapStart());
				for (int i = 0; i < mFrameCounts; ++i)
				{
					CheckSucceeded(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBackbuffers[i])));
				}

			}
			
		}
		DX12RenderDeviceModule()
		{
			mFrameCounts = 3;
		}
	private:
		COMRef<IDXGISwapChain3> mSwapChain;
		COMRef<ID3D12Device> mDevice;
		COMRef<ID3D12CommandQueue> m3DCommandQueue;
		COMRef<ID3D12CommandQueue> mCopyCommandQueue;

		COMRef<ID3D12DescriptorHeap> mBackbufferHeap;
		COMRef<ID3D12Resource> mBackbuffers;

		//descriptor consts
		UINT mRTVIncrementSize;
		UINT mSRVIncrementSize;

		//some consts
		int mFrameCounts;
		int mScreenWidth;
		int mScreenHeight;

	};
	IModule* CreateDX12RenderDeviceModule()
	{
		return new DX12RenderDeviceModule();
	}
	RenderDeviceResourceRef DX12RenderDeviceModule::CreateMeshSimple(SharedBufferRef& meshBlob)
	{
		return RenderDeviceResourceRef();
	}
	RenderDeviceResourceRef DX12RenderDeviceModule::CreatePSOSimple(VertexFormat vertexFormat, RenderDeviceResourceRef & vs, RenderDeviceResourceRef & ps)
	{
		return RenderDeviceResourceRef();
	}
	RenderDeviceResourceRef DX12RenderDeviceModule::CreateShaderSimple(SharedBufferRef& textBlob)
	{
		return RenderDeviceResourceRef();
	}
	RenderDeviceResourceRef DX12RenderDeviceModule::CreateRenderTarget()
	{
		return RenderDeviceResourceRef();
	}
	RenderDeviceResourceRef DX12RenderDeviceModule::CreteTextureSimple()
	{
		return RenderDeviceResourceRef();
	}
	void DX12RenderDeviceModule::BeginFrame()
	{
	}
	void DX12RenderDeviceModule::EndFrame()
	{
	}
}
