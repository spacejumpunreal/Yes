#include "Platform/DX12/DX12RenderDeviceModule.h"
#include "Platform/WindowsWindowModule.h"
#include "Platform/DXUtils.h"
#include "DX12RenderDeviceResources.h"
#include "Memory/ObjectPool.h"
#include "Core/System.h"

#include "Windows.h"
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

#include <cstdlib>
#include <algorithm>

namespace Yes
{
	enum class MemoryAccessCase
	{
		GPUAccessOnly,
		CPUUpload,
	};
	static ID3D12Heap* CreateHeapWithConfig(UINT64 size, MemoryAccessCase accessFlag, ID3D12Device* device)
	{
		D3D12_HEAP_DESC desc;
		desc.SizeInBytes = size;
		switch (accessFlag)
		{
		case MemoryAccessCase::GPUAccessOnly:
			desc.Properties.Type = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT;
			desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY::D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
			desc.Properties.CreationNodeMask = 0;
			desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL::D3D12_MEMORY_POOL_L1;
			desc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
			break;
		case MemoryAccessCase::CPUUpload:
			desc.Properties.Type = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_UPLOAD;
			desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY::D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
			desc.Properties.CreationNodeMask = 0;
			desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL::D3D12_MEMORY_POOL_L0;
			desc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
			break;
		default:
			CheckAlways(false);
		}
		desc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
		ID3D12Heap* heap;
		CheckSucceeded(device->CreateHeap(&desc, IID_PPV_ARGS(&heap)));
		return heap;
	}
	struct HeapCreator
	{
	public:
		HeapCreator(MemoryAccessCase accessFlag, ID3D12Device* device)
			: mAccessFlag(accessFlag)
			, mDevice(device)
		{
		}
		HeapCreator(const HeapCreator& other) = default;
		ID3D12Heap* CreateHeap(UINT64 size)
		{
			return CreateHeapWithConfig(size, mAccessFlag, mDevice);
		}
		ID3D12Device* mDevice;
		MemoryAccessCase mAccessFlag;
	};
	class DX12FirstFitAllocator : public IDX12GPUMemoryAllocator
	{
		struct AvailableRange
		{
			ID3D12Heap* Heap;
			UINT64 Start;
		};
		using AvailableRangeTree = std::multimap<UINT64, AvailableRange>;
		using RangeTree = std::map<UINT64, UINT64>;
		using HeapMap = std::unordered_map<ID3D12Heap*, RangeTree>;
	private:
		HeapCreator mCreator;
		UINT64 mDefaultBlockSize;
		AvailableRangeTree mAvailableMem;
		HeapMap mHeapMap;
		ObjectPool<IDX12GPUMemoryRegion> mRegionPool;
	public:
		DX12FirstFitAllocator(HeapCreator creator, UINT64 defaultBlockSize)
			: mCreator(creator)
			, mDefaultBlockSize(defaultBlockSize)
			, mRegionPool(1024)
		{}
		virtual const IDX12GPUMemoryRegion* Allocate(UINT64 size, UINT64 alignment)
		{
			auto it = mAvailableMem.lower_bound(size);
			IDX12GPUMemoryRegion* ret = mRegionPool.Allocate();
			ret->Allocator = this;
			bool found = false;
			while (it != mAvailableMem.end())
			{
				UINT64 alignedSize = CalcAlignedSize(it->second.Start, alignment, size);
				if (alignedSize <= it->first)
				{
					UINT64 begin = ret->Start = it->second.Start;
					ret->Offset = GetNextAlignedOffset(it->second.Start, alignment);
					ret->Size = alignedSize;
					ID3D12Heap* heap = ret->Heap = it->second.Heap;
					UINT64 leftSize = it->first - alignedSize;
					mAvailableMem.erase(it);
					mHeapMap[heap].erase(begin);
					//add modified region
					if (leftSize > 0)
					{
						UINT64 newStart = ret->Start + alignedSize;
						mAvailableMem.insert(std::make_pair(leftSize, AvailableRange{ heap, newStart }));
						mHeapMap[heap].insert(std::make_pair(newStart, leftSize));
					}
					return ret;
				}
			}
			//no available block, need to add new Heap
			ret->Offset = 0;
			ret->Start = 0;
			ret->Size = size;
			ID3D12Heap* heap;
			if (size >= mDefaultBlockSize)
			{//no need to add to free list
				heap = mCreator.CreateHeap(size);
			}
			else
			{//need to add to free list
				heap = mCreator.CreateHeap(mDefaultBlockSize);
				//need add left range
				UINT64 leftSize = mDefaultBlockSize - size;
				mAvailableMem.insert(std::make_pair(leftSize, AvailableRange {heap, size}));
				mHeapMap[heap].insert(std::make_pair(size, leftSize));
			}
			ret->Heap = heap;
			return ret;
		}
		virtual void Free(const IDX12GPUMemoryRegion* region)
		{}

	};
	class DX12LinearBlockAllocator : public IDX12GPUMemoryAllocator
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
		HeapCreator mHeapCreator;
		ObjectPool<IDX12GPUMemoryRegion> mRegionPool;
	public:
		DX12LinearBlockAllocator(const HeapCreator& creator, UINT64 blockSize)
			: mBlockSize(blockSize)
			, mCurrentBlockLeftSize(0)
			, mHeapCreator(creator)
			, mRegionPool(1024)
		{}
		virtual const IDX12GPUMemoryRegion* Allocate(UINT64 size, UINT64 alignment)
		{
			CheckAlways(size <= mBlockSize);
			size = CalcAlignedSize(mBlockSize - mCurrentBlockLeftSize, alignment, size);
			if (size > mCurrentBlockLeftSize)
			{
				mHeaps.push_front(BlockData{ mHeapCreator.CreateHeap(size), 1 });
				mCurrentBlockLeftSize = mBlockSize;
			}
			IDX12GPUMemoryRegion* ret = mRegionPool.Allocate();
			ret->Heap = mHeaps.back().Heap;
			ret->Start = mBlockSize - mCurrentBlockLeftSize;
			ret->Offset = GetNextAlignedOffset(ret->Start, alignment);
			ret->Size = size;
			ret->Allocator = this;
			return ret;
		}
		virtual void Free(const IDX12GPUMemoryRegion* region)
		{
			for (auto it = mHeaps.begin(); it != mHeaps.end(); ++it)
			{
				if (it->Heap == region->Heap)
				{
					--it->References;
					if (it->References == 0)
					{
						it->Heap->Release();
						mHeaps.erase(it);
					}
					mRegionPool.Deallocate(const_cast<IDX12GPUMemoryRegion*>(region));
					return;
				}
			}
			CheckAlways(false, "freeing non existing");
		}
	};
	
	class DX12RenderDeviceModuleImp : DX12RenderDeviceModule
	{
		//forward declarations
		
	public:
		//Resource related
		virtual RenderDeviceResourceRef CreateConstantBufferSimple(size_t size) override
		{
			DX12RenderDeviceConstantBuffer* cb = new DX12RenderDeviceConstantBuffer(size);
			return cb;
		}
		virtual RenderDeviceResourceRef CreateMeshSimple(SharedBufferRef& vertex, SharedBufferRef& index) override
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
			{//allocator
				mFrameTempAllocator = new DX12LinearBlockAllocator(
					HeapCreator(MemoryAccessCase::CPUUpload, mDevice.GetPtr()), 
					mAllocatorBlockSize);
				mUploadTempBufferAllocator = new DX12LinearBlockAllocator(
					HeapCreator(MemoryAccessCase::CPUUpload, mDevice.GetPtr()),
					mAllocatorBlockSize);
				mPersistentAllocator = new DX12FirstFitAllocator(
					HeapCreator(MemoryAccessCase::GPUAccessOnly, mDevice.GetPtr()),
					mAllocatorBlockSize);
			}
			mAsyncResourceCreator = new DX12AsyncResourceCreator(mDevice.GetPtr());
		}
		DX12RenderDeviceModuleImp()
		{
			mFrameCounts = 3;
			mAllocatorBlockSize = 32 * 1024 * 1024;
		}
	private:
		COMRef<IDXGISwapChain3> mSwapChain;
		COMRef<ID3D12Device> mDevice;
		COMRef<ID3D12CommandQueue> m3DCommandQueue;

		COMRef<ID3D12DescriptorHeap> mBackbufferHeap;
		COMRef<ID3D12Resource> mBackBuffers[3];
		
		DX12LinearBlockAllocator* mFrameTempAllocator;
		DX12LinearBlockAllocator* mUploadTempBufferAllocator;
		DX12FirstFitAllocator* mPersistentAllocator;

		//submodules
		DX12AsyncResourceCreator* mAsyncResourceCreator;
		
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
