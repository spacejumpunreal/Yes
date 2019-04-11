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
	class DX12ResourceManager;

	static const size_t MemoryAllocatorTypeCount = 3;
	static const size_t DescriptorHeapAllocatorTypeCount = 3;


	class IDX12ResourceRequest
	{
	public:
		virtual void StartRequest(DX12ResourceManager* creator) = 0;
		virtual void FinishRequest(DX12ResourceManager* creator) = 0;
	};

	class DX12ResourceManager
	{
		static const size_t MAX_BATCH_SIZE = 16;
		static const size_t AllocatorBlockSize = 16 * 1024 * 1024;
	public:
		static DX12ResourceManager& GetDX12DeviceResourceManager()
		{
			return *Instance;
		}
		DX12ResourceManager(ID3D12Device* dev);
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
		static DX12ResourceManager*				Instance;
	};

	class DX12ResourceBase
	{
	public:
		DX12ResourceBase();
		virtual void SetName(wchar_t* name) = 0;
		virtual void TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList) {};
		D3D12_RESOURCE_STATES GetState() { return mState; }
	protected:
		D3D12_RESOURCE_STATES mState;
	};

	//non GPU resources
	class DX12ConstantBuffer : public RenderDeviceConstantBuffer
	{
	public:
		DX12ConstantBuffer(size_t size)
			: mData(malloc(size))
		{
		}
		~DX12ConstantBuffer()
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
	class DX12Shader : public RenderDeviceShader
	{
	public:
		DX12Shader(ID3D12Device* dev, const char* body, size_t size, const char* name);
		bool IsReady() override
		{
			return true;
		}
		~DX12Shader() override;
		COMRef<ID3DBlob> mVS;
		COMRef<ID3DBlob> mPS;
		COMRef<ID3D12RootSignature> mRootSignature;
	};
	class DX12PSO : public RenderDevicePSO
	{
	public:
		DX12PSO(ID3D12Device* dev, RenderDevicePSODesc& desc);
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
	class DX12Mesh : public RenderDeviceMesh, public DX12ResourceBase
	{
	public:
		DX12Mesh(DX12ResourceManager* creator, ISharedBuffer* CPUData[2], size_t vertexStride, size_t indexCount, size_t indexStride);
		void Destroy() override;
		void Apply(ID3D12GraphicsCommandList* cmdList);
		UINT GetIndexCount() { return mIndexCount; }
		bool IsReady() override { return mIsReady; }
		void SetName(wchar_t* name) override;
	private:
		bool                                            mIsReady;
		ID3D12Resource*                                 mDeviceResource[2];
		DX12GPUMemoryRegion                             mMemoryRegion[2];
		D3D12_VERTEX_BUFFER_VIEW						mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW							mIndexBufferView;
		UINT											mIndexCount;
		friend class                                    DX12RenderDeviceMeshCreateRequest;
	};
	class IDX12RenderTarget : public RenderDeviceRenderTarget, public DX12ResourceBase
	{
	public:
		virtual ~IDX12RenderTarget();
		bool IsReady() override { return true; }
		void TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList) override;
		
		D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(int i);
		void SetName(wchar_t* name) override;
	protected:
		IDX12RenderTarget();
	protected:
		ID3D12Resource* mRenderTarget;
		DX12DescriptorHeapSpace1	mHeapSpace[2];//SRV + RTV
	};
	class DX12RenderTarget : public IDX12RenderTarget
	{
	public:
		DX12RenderTarget(size_t width, size_t height, TextureFormat format, ID3D12Device* device);
		void Destroy() override;
		DX12GPUMemoryRegion mMemoryRegion;
	};
	class DX12Backbuffer : public IDX12RenderTarget
	{
	public:
		DX12Backbuffer(ID3D12Resource* backbuffer, ID3D12Device* device, wchar_t* name);
		void Destroy() override;
	};

	class DX12DepthStencil : public RenderDeviceDepthStencil, public DX12ResourceBase
	{
	public:
		DX12DepthStencil(size_t width, size_t height, TextureFormat format, ID3D12Device* device);
		void Destroy() override;
		bool IsReady() override { return true; }
		void SetName(wchar_t* name) override;
		void TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList) override;
		ID3D12Resource* mBuffer;
		DX12DescriptorHeapSpace1	mHeapSpace;
		DX12GPUMemoryRegion mMemoryRegion;
	};
}