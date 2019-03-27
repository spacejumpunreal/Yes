#pragma once
#include "Yes.h"
#include "Graphics/RenderDevice.h"
#include "Misc/Utils.h"
#include "Concurrency/Thread.h"
#include "Concurrency/MultiThreadQueue.h"
#include "Platform/DX12/DX12MemAllocators.h"

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
	class IDX12GPUMemoryRegion;
	class DX12AsyncResourceCreator;

	class IDX12ResourceCreateRequest
	{
	public:
		virtual void StartCreation(DX12AsyncResourceCreator* creator) = 0;
		virtual void FinishCreation(DX12AsyncResourceCreator* creator) = 0;
	};

	class DX12AsyncResourceCreator
	{
		const size_t MAX_BATCH_SIZE = 16;
		const size_t AllocatorBlockSize = 16 * 1024 * 1024;
	public:
		DX12AsyncResourceCreator(ID3D12Device* dev);
		void AddRequest(IDX12ResourceCreateRequest* request);
		static void Entry(void* s);
		void Loop();
		IDX12GPUMemoryAllocator& GetTempBufferAllocator(ResourceType resourceType)
		{
			return *mUploadTempBufferAllocator[(int)resourceType];
		}
		IDX12GPUMemoryAllocator& GetPersistentAllocator(ResourceType resourceType)
		{
			return *mPersistentAllocator[(int)resourceType];
		}
		ID3D12GraphicsCommandList* GetCommandList()
		{
			return mCommandList.GetPtr();
		}
		ID3D12Device* GetDevice() { return mDevice; }
	private:
		ID3D12Device* mDevice;
		IDX12GPUMemoryAllocator* mUploadTempBufferAllocator[3];
		IDX12GPUMemoryAllocator* mPersistentAllocator[3];
		COMRef<ID3D12Fence> mFence;
		COMRef<ID3D12CommandQueue> mCopyCommandQueue;
		COMRef<ID3D12CommandAllocator> mCommandAllocator;
		COMRef<ID3D12GraphicsCommandList> mCommandList;
		
		MultiThreadQueue<IDX12ResourceCreateRequest*> mStartQueue;
		UINT64 mExpectedFenceValue;
		HANDLE mFenceEvent;
		Thread mResourceWorker;
	};

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
		COMRef<ID3D12PipelineState> mPSO;
		COMRef<ID3D12RootSignature> mRootSignature;
		CD3DX12_VIEWPORT mViewPort;
		CD3DX12_RECT mScissor;
	};

	struct DX12DeviceResourceWithMemoryRegion
	{
		DX12DeviceResourceWithMemoryRegion(ID3D12Resource* resource, const IDX12GPUMemoryRegion* region);
		DX12DeviceResourceWithMemoryRegion()
			: Resource(nullptr)
			, MemoryRegion(nullptr)
		{}
		~DX12DeviceResourceWithMemoryRegion();
		ID3D12Resource* Resource;
		const IDX12GPUMemoryRegion* MemoryRegion;
	};

	class DX12RenderDeviceMesh : public RenderDeviceMesh
	{
	public:
		DX12RenderDeviceMesh(DX12AsyncResourceCreator* creator, ISharedBuffer* CPUData[2]);
		void Apply(ID3D12GraphicsCommandList* cmdList);
		bool IsReady() override { return mIsReady; }
	private:
		bool mIsReady;
		DX12DeviceResourceWithMemoryRegion mVertexBuffer;
		DX12DeviceResourceWithMemoryRegion mIndexBuffer;
		friend class DX12RenderDeviceMeshCreateRequest;
	};
	class DX12RenderDeviceRenderTarget : public RenderDeviceRenderTarget
	{
	public:
		DX12RenderDeviceRenderTarget(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_RESOURCE_STATES state)
			: mRenderTarget(resource)
			, mHandle(handle)
			, mState(state)
		{}
		~DX12RenderDeviceRenderTarget();
		void TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList);
		bool IsReady() override { return true; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() { return mHandle; }
		D3D12_RESOURCE_STATES mState;
	private:
		ID3D12Resource* mRenderTarget;
		D3D12_CPU_DESCRIPTOR_HANDLE mHandle;
	};

	class DX12RenderDeviceDepthStencil : public RenderDeviceDepthStencil
	{};
}