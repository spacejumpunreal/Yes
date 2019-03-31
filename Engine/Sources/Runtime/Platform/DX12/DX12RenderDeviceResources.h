#pragma once
#include "Yes.h"
#include "Graphics/RenderDevice.h"
#include "Misc/Utils.h"
#include "Concurrency/Thread.h"
#include "Concurrency/MultiThreadQueue.h"
#include "Platform/DX12/DX12MemAllocators.h"
#include "Platform/DX12/DX12DescriptorHeapAllocators.h"

#include <deque>
#include "Windows.h"
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

namespace Yes
{
	//forward declaration
	class IDX12GPUMemoryAllocator;
	class DX12DeviceResourceManager;

	static const size_t MemoryAllocatorTypeCount = 3;
	static const size_t DescriptorHeapAllocatorTypeCount = 4;


	class IDX12ResourceRequest
	{
	public:
		virtual void StartRequest(DX12DeviceResourceManager* creator) = 0;
		virtual void FinishRequest(DX12DeviceResourceManager* creator) = 0;
	};

	class DX12DeviceResourceManager
	{
		const size_t MAX_BATCH_SIZE = 16;
		const size_t AllocatorBlockSize = 16 * 1024 * 1024;
	public:
		static DX12DeviceResourceManager& GetDX12DeviceResourceManager()
		{
			return *Instance;
		}
		DX12DeviceResourceManager(ID3D12Device* dev);
		void AddRequest(IDX12ResourceRequest* request);
		static void Entry(void* s);
		void Loop();
		IDX12GPUMemoryAllocator& GetTempBufferAllocator(ResourceType resourceType);
		IDX12GPUMemoryAllocator& GetAsyncPersistentAllocator(ResourceType resourceType);
		IDX12GPUMemoryAllocator& GetSyncPersistentAllocator(ResourceType resourceType);
		IDX12DescriptorHeapAllocator& GetAsyncDescriptorHeapAllocator(ResourceType resourceType);
		IDX12DescriptorHeapAllocator& GetSyncDescriptorHeapAllocator(ResourceType resourceType);
		ID3D12GraphicsCommandList* GetCommandList() { return mCommandList.GetPtr(); }
		ID3D12Device* GetDevice()                       { return mDevice; }
	private:
		ID3D12Device*									mDevice;
		//async creator side allocators
		IDX12GPUMemoryAllocator*                        mUploadTempBufferAllocator[MemoryAllocatorTypeCount];
		IDX12GPUMemoryAllocator*                        mAsyncMemoryAllocator[MemoryAllocatorTypeCount];
		IDX12DescriptorHeapAllocator*					mAsyncDescriptorHeapAllocator[DescriptorHeapAllocatorTypeCount];
		//render thread side allocators
		IDX12GPUMemoryAllocator*                        mSyncMemoryAllocator[MemoryAllocatorTypeCount];
		IDX12DescriptorHeapAllocator*					mSyncDescriptorHeapAllocator[DescriptorHeapAllocatorTypeCount];

		//only used in async creator thread
		COMRef<ID3D12Fence>                             mFence;
		COMRef<ID3D12CommandQueue>                      mCopyCommandQueue;
		COMRef<ID3D12CommandAllocator>                  mCommandAllocator;
		COMRef<ID3D12GraphicsCommandList>               mCommandList;
		MultiThreadQueue<IDX12ResourceRequest*>			mStartQueue;
		UINT64											mExpectedFenceValue;
		HANDLE											mFenceEvent;

		Thread											mResourceWorker;
		static DX12DeviceResourceManager*				Instance;
	};

	//non GPU resources
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
		DX12RenderDeviceShader(ID3D12Device* dev, const char* body, size_t size, const char* name);
		bool IsReady() override
		{
			return true;
		}
		~DX12RenderDeviceShader() override;
		COMRef<ID3DBlob> mVS;
		COMRef<ID3DBlob> mPS;
		COMRef<ID3D12RootSignature> mRootSignature;
	};
	class DX12RenderDevicePSO : public RenderDevicePSO
	{
	public:
		DX12RenderDevicePSO(ID3D12Device* dev, RenderDevicePSODesc& desc);
		bool IsReady()
		{
			return true;
		}
		void Apply(ID3D12GraphicsCommandList* cmdList);
	private:
		COMRef<ID3D12PipelineState>           mPSO;
		COMRef<ID3D12RootSignature>           mRootSignature;
		CD3DX12_VIEWPORT                      mViewPort;
		CD3DX12_RECT mScissor;
	};

	//GPU resources: need to care about memory and maybe DescriptorHeap
	class DX12RenderDeviceMesh : public RenderDeviceMesh
	{
	public:
		DX12RenderDeviceMesh(DX12DeviceResourceManager* creator, ISharedBuffer* CPUData[2]);
		void Destroy() override;
		void Apply(ID3D12GraphicsCommandList* cmdList);
		bool IsReady() override { return mIsReady; }
	private:
		bool                                            mIsReady;
		ID3D12Resource*                                 mDeviceResource[2];
		DX12GPUMemoryRegion                             mMemoryRegion[2];
		DX12DescriptorHeapSpace                         mHeapSpace;
		friend class                                    DX12RenderDeviceMeshCreateRequest;
	};
	class IDX12RenderDeviceRenderTarget : public RenderDeviceRenderTarget
	{
	public:
		virtual ~IDX12RenderDeviceRenderTarget();
		void TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList);
		bool IsReady() override { return true; }
		D3D12_RESOURCE_STATES GetState() { return mState; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetHandle();
	protected:
		D3D12_RESOURCE_STATES mState;
		ID3D12Resource* mRenderTarget;
		DX12DescriptorHeapSpace	mHeapSpace;
	};
	class DX12RenderTarget : IDX12RenderDeviceRenderTarget
	{};
	class DX12Backbuffer : public IDX12RenderDeviceRenderTarget
	{
	public:
		DX12Backbuffer(ID3D12Resource* backbuffer, const DX12DescriptorHeapSpace& heapSpace)
		{
			mRenderTarget = backbuffer;
			mHeapSpace = heapSpace;
			mState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
		}
		void Destroy() override;
	};

	class DX12RenderDeviceDepthStencil : public RenderDeviceDepthStencil
	{};
}