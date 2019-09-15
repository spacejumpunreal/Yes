#pragma once
#include "Runtime/Public/Yes.h"

#include "Runtime/Public/Misc/Utils.h"
#include "Runtime/Platform/DX12/DX12BufferAllocator.h"
#include "Runtime/Platform/DX12/DX12MemAllocators.h"
#include "Runtime/Platform/DX12/DX12RenderDeviceResources.h"
#include "Runtime/Platform/DX12/DX12ResourceSpace.h"

#include "Runtime/Public/Misc/BeginExternalIncludeGuard.h"
#include "Runtime/Platform/DX12/d3dx12.h"
#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include "Runtime/Public/Misc/EndExternalIncludeGuard.h"

#include <deque>

struct ID3D12GraphicsCommandList;
struct ID3D12CommandAllocator;

namespace Yes
{
	class IDX12GPUMemoryAllocator;
	class IDX12DescriptorHeapAllocator;
	class IDX12GPUBufferAllocator;

	class DX12CommandManager
	{
	public:
		DX12CommandManager(ID3D12Device* dev, ID3D12CommandQueue* cq, int backbufferIndex);
		void Reset();
		ID3D12GraphicsCommandList* ResetAndGetCommandList();
		void CloseAndExecuteCommandList();
	private:
		ID3D12CommandAllocator* mAllocator;
		ID3D12GraphicsCommandList* mCommandList;
		ID3D12CommandQueue* mCommandQueue;
	};
	class DX12FrameState
	{
	public:
		DX12FrameState(ID3D12Device* dev, DX12Texture2D* frameBuffer, ID3D12CommandQueue* cq, int backBufferIndex);
		~DX12FrameState();
		void CPUFinish();
		IDX12GPUBufferAllocator* GetConstantBufferAllocator()
		{
			return mConstantBufferAllocator;
		}
		IDX12DescriptorHeapAllocator& GetTempDescriptorHeapAllocator()
		{
			return *mLinearDescriptorHeapAllocator;
		}
		DX12CommandManager& GetCommandManager()
		{
			return mCommandManager;
		}
		void WaitGPUAndCleanup();
		DX12Texture2D* GetBackbuffer() { return mFrameBuffer.GetPtr(); }
	private:
		COMRef<ID3D12Fence> mFence;
		UINT64 mExpectedValue;
		HANDLE mEvent;
		IDX12GPUBufferAllocator* mConstantBufferAllocator;
		IDX12DescriptorHeapAllocator* mLinearDescriptorHeapAllocator;
		DX12CommandManager mCommandManager;
		TRef<DX12Texture2D> mFrameBuffer;
		ID3D12CommandQueue* mCommandQueue;
		int mBackBufferIndex;
	};
}