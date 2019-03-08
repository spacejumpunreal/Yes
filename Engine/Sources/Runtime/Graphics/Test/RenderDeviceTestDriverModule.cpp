#include "Graphics/Test/RenderDeviceTestDriverModule.h"
#include "Core/System.h"
#include "Core/TickModule.h"
#include "Core/FileModule.h"
#include "Misc/Utils.h"
#include "Misc/Debug.h"
#include "Misc/Container.h"
#include "Graphics/RenderDevice.h"
#include "Platform/DX12/DX12RenderDeviceModule.h"

namespace Yes
{
	class Dx12RenderDeviceModule;
	DEFINE_MODULE(RenderDeviceTestDriverModule), public ITickable
	{
	private:
		RenderDevice* mDevice = nullptr;
		FileModule* mFileModule = nullptr;
		RenderDeviceResourceRef mShader;

	public:
		virtual void InitializeModule() override
		{
			TickModule* tickModule = GET_MODULE(TickModule);
			tickModule->AddTickable(this);
			mDevice = GET_MODULE_AS(RenderDevice, DX12RenderDeviceModule);
			mFileModule = GET_MODULE(FileModule);
		}
		virtual void Tick() override
		{
			if (mDevice == nullptr)
			{
				Setup();
			}
			Update();
		}
	private:
		void Setup()
		{
			//Shader
			{
				SharedBufferRef shaderContent = mFileModule->ReadFileContent("FirstStep.hlsl");
				mShader = mDevice->CreateShaderSimple(shaderContent, "FirstStep.hlsl");
			}
			//PSO
			//Mesh
			//Texture
			//ConstantBuffer
			
		}
		
		void Update()
		{
		}
	};
	DEFINE_MODULE_CREATOR(RenderDeviceTestDriverModule);
}