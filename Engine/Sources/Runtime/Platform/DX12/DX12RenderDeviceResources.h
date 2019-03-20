#pragma once
#include "Yes.h"
#include "Graphics/RenderDevice.h"
#include "Misc/Utils.h"
#include "Concurrency/Thread.h"
#include "Concurrency/MultiThreadQueue.h"

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

	class IDX12ResourceCreateRequest
	{
	public:
		virtual void StartCreation() = 0;
		virtual void FinishCreation() = 0;
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
		IDX12GPUMemoryAllocator& GetTempBufferAllocator()
		{
			return *mUploadTempBufferAllocator;
		}
	private:
		ID3D12Device* mDevice;
		IDX12GPUMemoryAllocator* mUploadTempBufferAllocator;
		COMRef<ID3D12Fence> mFence;
		COMRef<ID3D12CommandQueue> mCopyCommandQueue;
		COMRef<ID3D12CommandAllocator> mCommandAllocator;
		COMRef<ID3D12GraphicsCommandList> mCommandList;
		std::vector<IDX12ResourceCreateRequest*> mPopResults;
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

	class DX12RenderDeviceMesh : public RenderDeviceMesh
	{
	public:
		DX12RenderDeviceMesh(ID3D12Device* dev, void* vb, size_t vbSize, void* ib, size_t ibSize, IDX12GPUMemoryRegion* mem);
		void Apply(ID3D12GraphicsCommandList* cmdList);
	private:
		COMRef<ID3D12Resource> mVertexBuffer;
		COMRef<ID3D12Resource> mIndexBuffer;
		bool mIsReady;

		friend class DX12RenderDeviceMeshCreateRequest;
	};
}