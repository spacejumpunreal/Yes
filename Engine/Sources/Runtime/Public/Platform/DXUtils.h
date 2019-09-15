#pragma once

#include "Runtime/Public/Yes.h"

#include "Runtime/Public/Misc/Utils.h"
#include <d3d12.h>
#include <dxgi1_2.h>

namespace Yes
{
	void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter, D3D_FEATURE_LEVEL featureLevel);
	COMRef<ID3DBlob> DoCompileShader(const char* text, size_t size, const char* shaderName, const char* entry, const char* target, UINT flags1);
}