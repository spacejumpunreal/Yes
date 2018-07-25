#pragma once
#include "Demo18.h"
#include "RendererObjects.h"
#include "FileModule.h"
#include "Debug.h"
#include "System.h"
#include "RenderDeviceModule.h"

namespace Demo18
{
	//Texture2D
	RendererTexture2D* CreateRendererTexture2D(const char* path)
	{
		auto ret = new RendererTexture2D();
		ret->mData = SharedBytes(new ByteBlob(GET_MODULE(File)->ReadFileContent(path)));
		return ret;
	}
	//Geometry

	//Shader
	RendererShader* CreateRendererShader(const char* path, const char* entry)
	{
		auto ret = new RendererShader();
		ret->mEntry = entry;
		ret->mPath = path;
		ret->mShaderCode = SharedBytes(new ByteBlob(GET_MODULE(File)->ReadFileContent(path)));
		return ret;
	}
	//RenderTarget
	//ConstantBuffer
}