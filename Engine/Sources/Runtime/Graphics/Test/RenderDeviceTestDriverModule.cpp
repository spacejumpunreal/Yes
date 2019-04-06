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
		TRef<RenderDeviceShader> mShader;
		TRef<RenderDevicePSO> mPSO;
		TRef<RenderDeviceMesh> mMesh;
		static const size_t ConstantBufferSize = 512;
		static const size_t ConstantBufferSlots = 512 / 4;
		float* mConstantBuffer;
		int mFrame;
		bool mAllResourceReady = false;

	public:
		virtual void InitializeModule(System* system) override
		{
			TickModule* tickModule = GET_MODULE(TickModule);
			tickModule->AddTickable(this);
			mFileModule = GET_MODULE(FileModule);
			mConstantBuffer = new float[ConstantBufferSlots];
			mFrame = 0;
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
				size_t vertexStride = 8 * 4;//P3FN3FUV2F
				size_t indexStride = 4;
				size_t indexCount = ib->GetSize() / indexStride;
				mMesh = mDevice->CreateMeshSimple(vb, ib, vertexStride, indexCount, indexStride);
			}
			//Texture
			//ConstantBuffer
			{
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
			mAllResourceReady = true;
		}
		void Update()
		{
			//update constant
			{
				memset(mConstantBuffer, 0, ConstantBufferSize);
				for (int i = 0; i < ConstantBufferSlots; ++i)
				{
					mConstantBuffer[i] = ((float)(i)) + mFrame;
				}
			}
			mDevice->BeginFrame();
			RenderDevicePass* pass = mDevice->AllocPass();
			pass->SetOutput(pass->GetBackbuffer(), 0);
			pass->SetClearColor(V4F(0, 1, 0, 0), true, 0);
			pass->SetGlobalConstantBuffer(mConstantBuffer, ConstantBufferSize);

			RenderDeviceDrawcall* cmd = (RenderDeviceDrawcall*)pass->AddCommand(RenderCommandType::Drawcall);
			cmd->SetMesh(mMesh.GetPtr());
			cmd->SetPSO(mPSO.GetPtr());
			cmd->SetConstantBuffer(mConstantBuffer, ConstantBufferSize, pass);
			mDevice->ExecutePass(pass);
			mDevice->EndFrame();
		}
		DEFINE_MODULE_IN_CLASS(RenderDeviceTestDriverModule, RenderDeviceTestDriverModuleImp);
	};
	DEFINE_MODULE_REGISTRY(RenderDeviceTestDriverModule, RenderDeviceTestDriverModuleImp, 1000);
}