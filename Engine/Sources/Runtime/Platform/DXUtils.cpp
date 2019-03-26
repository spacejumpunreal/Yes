#include "Platform/DXUtils.h"

#include "Misc/Utils.h"
#include "Misc/Debug.h"
#include <d3dcompiler.h>



namespace Yes
{
	void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter, D3D_FEATURE_LEVEL featureLevel)
	{
		COMRef<IDXGIAdapter1> adapter;
		*ppAdapter = nullptr;

		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see if the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.GetPtr(), featureLevel, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
		*ppAdapter = adapter.Detach();
	}
	COMRef<ID3DBlob> DoCompileShader(const char* text, size_t size, const char* shaderName, const char* entry, const char* target, UINT flags1)
	{
		COMRef<ID3DBlob> blob;
#if 0
		CheckSucceeded(D3DCompile(text, size, shaderName, nullptr, nullptr, entry, target, flags1, 0, &blob, nullptr));
#else
		COMRef<ID3DBlob> error;
		if (FAILED(D3DCompile(text, size, shaderName, nullptr, nullptr, entry, target, flags1, 0, &blob, &error)))
		{
			const char* errorMsg = (const char*)error->GetBufferPointer();
			CheckAlways(false);
		}
#endif
		return blob;
	}
}