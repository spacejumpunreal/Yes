#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Graphics/RenderDevice.h"
#include "Runtime/Public/Misc/Utils.h"
#include "Runtime/Public/Concurrency/Thread.h"
#include "Runtime/Public/Concurrency/MultiThreadQueue.h"
#include "Runtime/Platform/DX12/DX12MemAllocators.h"
#include "Runtime/Platform/DX12/DX12BufferAllocator.h"
#include "Runtime/Platform/DX12/DX12DescriptorHeapAllocators.h"
#include "Runtime/Platform/DX12/DX12ResourceSpace.h"

#include <deque>
#include "Runtime/Public/Misc/BeginExternalIncludeGuard.h"
#include "Windows.h"
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3d12.h"
#include "d3dx12.h"
#include "Runtime/Public/Misc/EndExternalIncludeGuard.h"

namespace Yes
{
	//forward declaration
	class IDX12GPUMemoryAllocator;
	class DX12ResourceManager;

	static const size_t MemoryAllocatorTypeCount = 3;
	static const size_t DescriptorHeapAllocatorTypeCount = 3;

	D3D12_RESOURCE_STATES StateAbstract2Device(RenderDeviceResourceState st);
	RenderDeviceResourceState StateDevice2Abstract(D3D12_RESOURCE_STATES st);

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
		//used by async uploader only
		IDX12GPUMemoryAllocator& GetAsyncUploadBufferAllocator(ResourceType resourceType);
		//used by render thread
		IDX12GPUBufferAllocator& GetPersistentBufferAllocator();
		IDX12GPUMemoryAllocator& GetPersistentMemAllocator(ResourceType resourceType);
		IDX12DescriptorHeapAllocator& GetPersistentDescriptorHeapAllocator(ResourceType resourceType, bool shaderVisible);
		ID3D12GraphicsCommandList* GetCommandList() { return mCommandList.GetPtr(); }
		ID3D12Device* GetDevice()                       { return mDevice; }
	private:
		ID3D12Device*									mDevice;
		//async uploader side allocators, actually only need for buffer
		IDX12GPUMemoryAllocator*                        mUploadTempBufferAllocator[MemoryAllocatorTypeCount];
		//render thread side allocators
		IDX12GPUBufferAllocator*						mPersistentBufferAllocator;
		IDX12GPUMemoryAllocator*                        mPersistentMemoryAllocator[MemoryAllocatorTypeCount];
		//(ShaderVisible/Invisible) * DescriptorHeapAllocatorCount, all persistent
		IDX12DescriptorHeapAllocator*					mPersistentDescriptorHeapAllocator[2][DescriptorHeapAllocatorTypeCount];
		//only used in async uploader thread
		COMRef<ID3D12Fence>                             mFence;
		COMRef<ID3D12CommandQueue>                      mCopyCommandQueue;
		COMRef<ID3D12CommandAllocator>                  mCommandAllocator;
		COMRef<ID3D12GraphicsCommandList>               mCommandList;
		MultiThreadQueue<IDX12ResourceRequest*>			mStartQueue;
		UINT64											mExpectedFenceValue;
		HANDLE											mFenceEvent;

		Thread											mResourceWorker;
		static DX12ResourceManager*				        Instance;
	};

	class DX12ResourceStateHelper //helper for you to implement some interfaces in RenderDeviceResource
	{
	public:
		DX12ResourceStateHelper();
		RenderDeviceResourceState GetAbstractState();
		void SetAbstractState(RenderDeviceResourceState state);
	protected:
		D3D12_RESOURCE_STATES mDeviceState;
	};
	class DX12Shader : public RenderDeviceShader
	{
	public:
		DX12Shader(ID3D12Device* dev, const char* body, size_t size, const char* name);
		bool IsReady()
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
	class DX12Mesh : public RenderDeviceMesh
	{
	public:
		DX12Mesh(size_t streamSizes[], size_t strides[], size_t indexCount, D3D12_RESOURCE_DESC desc[]);
		void Destroy() override;
		void SetName(wchar_t* name) override;
		bool IsReady() override { return mIsReady; }
	public:
		void Apply(ID3D12GraphicsCommandList* cmdList);
		UINT GetIndexCount() { return mIndexCount; }
		void AsyncInit(ISharedBuffer* bufferData[], size_t vertexStride, size_t indexStride, D3D12_RESOURCE_DESC desc[]);
	private:
		bool                                            mIsReady;
		ID3D12Resource*                                 mDeviceResource[2];
		DX12GPUMemoryRegion                             mMemoryRegion[2];
		D3D12_VERTEX_BUFFER_VIEW						mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW							mIndexBufferView;
		UINT											mIndexCount;
		friend class                                    DX12RenderDeviceMeshCopyRequest;
	};
	class DX12Texture2D : public RenderDeviceTexture, private DX12ResourceStateHelper
	{
	public:
		DX12Texture2D(size_t width, size_t height, TextureFormat format, TextureUsage usage, D3D12_RESOURCE_DESC* desc);
		DX12Texture2D(ID3D12Resource* resource, TextureFormat format, TextureUsage usage);
		void Destroy() override;
		void SetName(wchar_t* name);
		bool IsReady() override { return mIsReady; }
		void SetState(RenderDeviceResourceState state) override;
		RenderDeviceResourceState GetState() override;
		void* GetTransitionTarget() override;
		V3I GetSize() override;
	public:
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(TextureUsage usage);
		void AsyncInit(RawImage* image, D3D12_RESOURCE_DESC* desc);
		void TransitToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList);
		
	protected:
		void InitTexture(size_t width, size_t height, TextureFormat format, D3D12_RESOURCE_DESC* desc);
		void InitHandles();
	private:
		ID3D12Resource* mTexture;
		V3I mSize;
		DX12DescriptorHeapSpace1 mHeapSpace[3];
		TextureFormat mFormat;
		TextureUsage mUsage;
		DX12GPUMemoryRegion mMemoryRegion;
		
		bool mIsReady;
		friend class DX12RenderDeviceTexture2DCopyRequest;
	};
}