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
		RenderDeviceResourceRef mMesh;
		RenderDeviceResourceRef mConstantBuffer;
		bool mAllResourceReady = false;

	public:
		virtual void InitializeModule(System* system) override
		{
			TickModule* tickModule = GET_MODULE(TickModule);
			tickModule->AddTickable(this);
			mFileModule = GET_MODULE(FileModule);
		}
		virtual void Tick() override
		{
			if (mDevice == nullptr)
			{
				DX12RenderDeviceModule* dev = GET_MODULE(DX12RenderDeviceModule);
				mDevice = dynamic_cast<RenderDevice*>(dev);
				Setup();
			}
			if (mAllResourceReady)
				CheckResources();
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
				auto vb = mFileModule->ReadFileContent("Mesh/box_vb.bin");
				auto ib = mFileModule->ReadFileContent("Mesh/box_ib.bin");
				mMesh = mDevice->CreateMeshSimple(vb, ib);
			}
			//Texture
			//ConstantBuffer
			{
				mConstantBuffer = mDevice->CreateConstantBufferSimple(1024);
			}
		}
		void CheckResources()
		{
			if (!mShader->IsReady())
				return;
			if (!mPSO->IsReady())
				return;
			if (!mMesh->IsReady())
				return;
			if (!mConstantBuffer->IsReady())
				return;
			mAllResourceReady = true;
		}
		void Update()
		{
			RenderDevicePass* pass = mDevice->AllocPass();
		}
		DEFINE_MODULE_IN_CLASS(RenderDeviceTestDriverModule, RenderDeviceTestDriverModuleImp);
	};
	DEFINE_MODULE_REGISTRY(RenderDeviceTestDriverModule, RenderDeviceTestDriverModuleImp, 1000);
}