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
	struct IDX12GPUMemoryRegion
	{
		ID3D12Heap* Heap;
		UINT64* Offset;
		UINT64 Size;
	};
	enum class MemoryAccessCase
	{
		GPUAccessOnly,
		CPUUpload,
	};
	class IDX12GPUMemoryAllocator
	{
	public:
		virtual const IDX12GPUMemoryRegion* Allocate(UINT64 size) = 0;
		virtual void Free(const IDX12GPUMemoryRegion* region) = 0;
	protected:
		ID3D12Heap
	};
	class DX12LinearBlockAllocator
	{
		struct BlockData
		{
			ID3D12Heap* Heap;
			int References;
		};
	private:
		UINT64 mBlockSize;
		std::list<BlockData> mHeaps;
		UINT64 mCurrentBlockLeftSize;
	public:
		DX12LinearBlockAllocator(UINT64 blockSize)
			: mBlockSize(blockSize)
			, mCurrentBlockLeftSize(0)
		{
		}
		virtual const IDX12GPUMemoryRegion* Allocate(UINT64 size)
		{}
		virtual void Free(const IDX12GPUMemoryRegion* region)
		{}
	};
	class DX12RenderDeviceModuleImp : DX12RenderDeviceModule
	{
		//forward declarations
		class DX12RenderDeviceShader;
	public:
		//Resource related
		virtual RenderDeviceResourceRef CreateMeshSimple(SharedBufferRef& meshBlob) override
		{
			return RenderDeviceResourceRef();
		}
		virtual RenderDeviceResourceRef CreatePSOSimple(RenderDevicePSODesc& desc) override
		{
			DX12RenderDevicePSO* pso = new DX12RenderDevicePSO(mDevice.GetPtr(), desc);
			return pso;
		}
		virtual RenderDeviceResourceRef CreateShaderSimple(SharedBufferRef& textBlob, const char* registeredName) override
		{
			DX12RenderDeviceShader* shader = new DX12RenderDeviceShader(mDevice.GetPtr(), (const char*)textBlob->GetData(), textBlob->GetSize(), registeredName);
			return shader;
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
		virtual void SetViewPort() override
		{}
		virtual void SetScissor() override
		{}
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
		virtual void Start(System* system) override
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

				CD3DX12_CPU_DESCRIPTOR_HANDLE heapPtr(mBackbufferHeap->GetCPUDescriptorHandleForHeapStart(), mRTVIncrementSize);
				for (int i = 0; i < mFrameCounts; ++i)
				{
					CheckSucceeded(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBackBuffers[i])));
					mDevice->CreateRenderTargetView(mBackBuffers[i].GetPtr(), nullptr, heapPtr);
				}
			}
			
		}
		DX12RenderDeviceModuleImp()
		{
			mFrameCounts = 3;
		}
	private:
		COMRef<IDXGISwapChain3> mSwapChain;
		COMRef<ID3D12Device> mDevice;
		COMRef<ID3D12CommandQueue> m3DCommandQueue;
		COMRef<ID3D12CommandQueue> mCopyCommandQueue;

		COMRef<ID3D12DescriptorHeap> mBackbufferHeap;
		COMRef<ID3D12Resource> mBackBuffers[3];


		static const int wa= sizeof(D3D12_SHADER_BYTECODE);

		//descriptor consts
		UINT mRTVIncrementSize;
		UINT mSRVIncrementSize;

		//some consts
		int mFrameCounts;
		int mScreenWidth;
		int mScreenHeight;


	private:
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
				psoDesc.InputLayout = { layout, count};
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

			
		DEFINE_MODULE_IN_CLASS(DX12RenderDeviceModule, DX12RenderDeviceModuleImp);
	};
	DEFINE_MODULE_REGISTRY(DX12RenderDeviceModule, DX12RenderDeviceModuleImp, 500);
}
