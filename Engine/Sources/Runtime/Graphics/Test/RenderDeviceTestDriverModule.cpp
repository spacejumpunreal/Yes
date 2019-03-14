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
	class RenderDeviceTestDriverModuleImp : public RenderDeviceTestDriverModule, public ITickable
	{
	private:
		RenderDevice* mDevice = nullptr;
		FileModule* mFileModule = nullptr;
		RenderDeviceResourceRef mShader;
		RenderDeviceResourceRef mPSO;

	public:
		virtual void InitializeModule()
		{
			TickModule* tickModule = GET_MODULE(TickModule);
			tickModule->AddTickable(this);
			mDevice = GET_MODULE_AS(DX12RenderDeviceModule, RenderDevice);
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
			{
				RenderDevicePSODesc desc;
				desc.RTCount = 1;
				desc.RTs[0] = TextureFormat::R8G8B8A8_UNORM;
				desc.Shader = mShader;
				desc.StateKey = PSOStateKey::Default;
				desc.VF = VertexFormat::VF_P3F_T2F;
				mPSO = mDevice->CreatePSOSimple(desc);
			}
			//Mesh
			{

			}
			//Texture
			//ConstantBuffer
			
		}
		
		void Update()
		{
		}
		DEFINE_MODULE_IN_CLASS(RenderDeviceTestDriverModule, RenderDeviceTestDriverModuleImp);
	};
	DEFINE_MODULE_REGISTRY(RenderDeviceTestDriverModule, RenderDeviceTestDriverModuleImp, 1000);
}