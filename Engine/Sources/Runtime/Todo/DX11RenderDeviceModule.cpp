#include "Demo18.h"
#include "RenderDeviceModule.h"
#include "DX11RenderDeviceResource.h"
#include "System.h"
#include "WindowModule.h"
#include "Debug.h"
#include "Utils.h"

#include "windows.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include "ThirdParty/DDSTextureLoader/DDSTextureLoader.h"	

namespace Demo18
{
	
	class DX11RenderDeviceModule : public RenderDeviceModule
	{
	public:
		virtual void InitializeModule()
		{
			_InitD3D();
			RegisterVertexFormats();
		}
		virtual void Start() override
		{
			GET_MODULE(Window)->GetWindowHandle(&mWindowHandle);
		}
		void ScheduleRelease(IRenderDeviceResource* resource)
		{
			mToDestroy.push_back(resource);
		}
		virtual void RenderPass(RendererPass* pass) override
		{
			if (pass->mRT)
			{
				ApplyRenderTarget(mDevice, mDeviceContext, mDepthStencilView, *pass->mRT);
			}
			else
			{
				mDeviceContext->OMSetRenderTargets(1, &mBackBufferView, mDepthStencilView);
			}
			mDeviceContext->ClearRenderTargetView(mBackBufferView, (float*)&pass->mClearColor);
			UINT flag = 0;
			flag |= pass->mNeedClearDepth ? D3D11_CLEAR_DEPTH : 0;
			flag |= pass->mNeedClearStencil ? D3D11_CLEAR_STENCIL : 0;
			mDeviceContext->ClearDepthStencilView(mDepthStencilView, flag, pass->mClearDepth, pass->mClearStencil);
			
			auto dc = pass->mDrawcalls;
			while (dc)
			{
				if (dc->VertexShader)
					ApplyShader<VSStage>(mDevice, mDeviceContext, *dc->VertexShader);
				if (dc->PixelShader)
					ApplyShader<PSStage>(mDevice, mDeviceContext, *dc->PixelShader);
				if (dc->Geometry)
				{
					ApplyGeometry(mDevice, mDeviceContext, *dc->Geometry, 0);
					ApplyLayout(mDevice, mDeviceContext, &*dc->VertexShader, &*dc->Geometry);
					
				}
				for (uint i = 0; i < dc->Textures.size(); i++)
				{
					if (dc->Textures[i])
						ApplyTexture2D<PSStage>(mDevice, mDeviceContext, *dc->Textures[i], i);
				}
				for (uint i = 0; i < dc->Samplers.size(); i++)
				{
					if (dc->Samplers[i])
						ApplySamplerState<PSStage>(mDevice, mDeviceContext, *dc->Samplers[i], i);
				}
				ApplyConstant<VSStage>(mDevice, mDeviceContext, *dc->VSConstants[0], 0);
				ApplyConstant<PSStage>(mDevice, mDeviceContext, *dc->PSConstants[0], 0);
				mDeviceContext->DrawIndexed(dc->Geometry->mIndexCount, 0, 0);
				dc = dc->Next;
			}
		}
		virtual void BeginFrame() override
		{
			//update clientSize, recreate backbuffer and stencil if necessary
			auto newClientSize = GET_MODULE(Window)->GetClientSize();
			if (newClientSize != mClientSize)
			{
				mClientSize = newClientSize;
				_CreateSwapChain();
				_CreateBackBufferDepthStencil();
				_SetViewport();
			}
		}
		virtual void EndFrame() override
		{
			mSwapChain->Present(0, 0);
		}
	protected:
		void _InitD3D()
		{
			//device and device context
			D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, };
			HRESULT ret;
			ret = D3D11CreateDevice(
				nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
				D3D11_CREATE_DEVICE_DEBUG,
				featureLevels,
				sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL),
				D3D11_SDK_VERSION,
				&mDevice,
				nullptr,
				&mDeviceContext);
			Check(SUCCEEDED(ret));
		}
		void _CreateSwapChain()
		{
			//swapchain
			ReleaseCOM(mSwapChain);
			DXGI_SWAP_CHAIN_DESC sd;
			sd.BufferDesc.Width = mClientSize.x;
			sd.BufferDesc.Height = mClientSize.y;
			sd.BufferDesc.RefreshRate.Numerator = 60;
			sd.BufferDesc.RefreshRate.Denominator = 1;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.BufferCount = 1;
			sd.OutputWindow = mWindowHandle;
			sd.Windowed = true;
			sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			sd.Flags = 0;

			IDXGIDevice* dxgiDevice;
			HRESULT hr;
			hr = mDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
			Check(SUCCEEDED(hr));
			IDXGIAdapter* dxgiAdapter;
			hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);
			Check(SUCCEEDED(hr));
			IDXGIFactory* dxgiFactory;
			hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory);
			Check(SUCCEEDED(hr));
			hr = dxgiFactory->CreateSwapChain(mDevice, &sd, &mSwapChain);
			Check(SUCCEEDED(hr));
			ReleaseCOM(dxgiDevice);
			ReleaseCOM(dxgiAdapter);
		}
		void _CreateBackBufferDepthStencil()
		{
			HRESULT hr;
			ReleaseCOM(mBackBufferView);
			{
				ID3D11Texture2D* backBuffer;
				hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
				Check(SUCCEEDED(hr));
				hr = mDevice->CreateRenderTargetView(backBuffer, 0, &mBackBufferView);
				Check(SUCCEEDED(hr));
				ReleaseCOM(backBuffer);
			}
			ReleaseCOM(mDepthStencilView);
			{
				D3D11_TEXTURE2D_DESC td;
				td.Width = mClientSize.x;
				td.Height = mClientSize.y;
				td.MipLevels = 1;
				td.ArraySize = 1;
				td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				td.SampleDesc.Count = 1;
				td.SampleDesc.Quality = 0;
				td.Usage = D3D11_USAGE_DEFAULT;
				td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
				td.CPUAccessFlags = 0;
				td.MiscFlags = 0;
				ID3D11Texture2D* buffer;
				hr = mDevice->CreateTexture2D(&td, nullptr, &buffer);
				Check(SUCCEEDED(hr));
				hr = mDevice->CreateDepthStencilView(buffer, nullptr, &mDepthStencilView);
				Check(SUCCEEDED(hr));
			}
		}
		void _SetViewport()
		{
			D3D11_VIEWPORT vp;
			vp.TopLeftX = 0;
			vp.TopLeftY = 0;
			vp.Width = (float)mClientSize.x;
			vp.Height = (float)mClientSize.y;
			vp.MinDepth = 0;
			vp.MaxDepth = 1;
			mDeviceContext->RSSetViewports(1, &vp);
		}
	protected:
		//logic
		std::vector<IRenderDeviceResource*> mToDestroy;
		//window
		HWND mWindowHandle = 0;
		V2I mClientSize = V2I(0, 0);
		//d3d device
		ID3D11Device* mDevice = nullptr;
		ID3D11DeviceContext* mDeviceContext = nullptr;
		IDXGISwapChain* mSwapChain = nullptr;
		//d3d resource
		ID3D11RenderTargetView* mBackBufferView = nullptr;
		ID3D11DepthStencilView* mDepthStencilView = nullptr;
	};
	DX11RenderDeviceModule* GRenderDeviceModule;
	RenderDeviceModule* CreateRenderDeviceModule()
	{
		auto m = new DX11RenderDeviceModule();
		GRenderDeviceModule = m;
		return m;
	}
	void _ScheduleRelease(IRenderDeviceResource* resource)
	{
		GRenderDeviceModule->ScheduleRelease(resource);
	}
}