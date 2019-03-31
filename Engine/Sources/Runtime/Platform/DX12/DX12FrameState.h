#pragma once
#include "Yes.h"

#include "Misc/Utils.h"
#include "Platform/DX12/DX12MemAllocators.h"

#include "Windows.h"
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

#include <deque>

struct ID3D12GraphicsCommandList;
struct ID3D12CommandAllocator;

namespace Yes
{
	class IDX12GPUMemoryAllocator;
	class IDX12DescriptorHeapAllocator;
	class DX12Backbuffer;
	class IDX12RenderDeviceRenderTarget;

	struct AllocatedCBV
	{
		ID3D12Resource* Buffer;
	};
	class DX12ConstantBufferManager
	{
	public:
		DX12ConstantBufferManager(ID3D12Device* dev);
		AllocatedCBV Allocate(size_t size);
		void Reset();
	private:
		std::deque<ID3D12Resource*> mAllocatedResources;
		IDX12GPUMemoryAllocator* mAllocator;
		ID3D12Device* mDevice;
	};
	class DX12CommandManager
	{
	public:
		DX12CommandManager(ID3D12Device* dev);
		void Reset();
		ID3D12GraphicsCommandList* GetCommandList() { return mCommandList; }
	private:
		ID3D12CommandAllocator* mAllocator;
		ID3D12GraphicsCommandList* mCommandList;
	};
	class DX12FrameState
	{
	public:
		DX12FrameState(ID3D12Device* dev, DX12Backbuffer* frameBuffer);
		~DX12FrameState();
		void Finish();
		DX12ConstantBufferManager& GetConstantBufferManager()
		{
			return mConstantBufferManager;
		}
		IDX12DescriptorHeapAllocator& GetTempDescriptorHeapAllocator()
		{
			return *mLinearDescriptorHeapAllocator;
		}
		ID3D12GraphicsCommandList* GetCommandList()
		{
			return mCommandManager.GetCommandList();
		}
		void WaitForFrame();
		IDX12RenderDeviceRenderTarget* GetBackbuffer() { return (IDX12RenderDeviceRenderTarget*)mFrameBuffer; }
	private:
		COMRef<ID3D12Fence> mFence;
		UINT64 mExpectedValue;
		HANDLE mEvent;
		DX12ConstantBufferManager mConstantBufferManager;
		IDX12DescriptorHeapAllocator* mLinearDescriptorHeapAllocator;
		DX12CommandManager mCommandManager;
		DX12Backbuffer* mFrameBuffer;
	};
}