#include "Public/Graphics/Test/RenderDeviceTestDriverModule.h"
#include "Public/Core/System.h"
#include "Public/Core/TickModule.h"
#include "Public/Core/FileModule.h"
#include "Public/Platform/IWindowModule.h"
#include "Public/Misc/Utils.h"
#include "Public/Misc/Debug.h"
#include "Public/Misc/Container.h"
#include "Public/Graphics/RenderDevice.h"
#include "Public/Platform/WindowsWindowModule.h"
#include "Public/Platform/DX12/DX12RenderDeviceModule.h"
#include "Public/Platform/InputState.h"
#include "Public/Graphics/Camera/Camera.h"

#define LOG_INPUT 0

namespace Yes
{
	class RenderDeviceTestDriverModuleImp : public RenderDeviceTestDriverModule, public ITickable
	{
	private:
		RenderDevice* mDevice = nullptr;
		FileModule* mFileModule = nullptr;
		IWindowModule* mWindowModule = nullptr;
		TRef<RenderDeviceShader> mShader;
		TRef<RenderDevicePSO> mPSO;
		TRef<RenderDeviceMesh> mMeshMonkey;
		TRef<RenderDeviceMesh> mMeshPlane;
		static const size_t ConstantBufferSize = 512;
		static const size_t ConstantBufferSlots = 512 / 4;
		float* mMonkeyConstantBuffer;
		float* mPlaneConstantBuffer;
		RenderDeviceDepthStencilRef mDepthStencil;
		int mFrame;
		V4F mClearColors[4];
		bool mAllResourceReady = false;
		float mYaw = 0;
		float mPitch = 0;
		V3F mPosition = V3F(0, 2.f, 0);
		PerspectiveCamera mCamera;

	public:
		RenderDeviceTestDriverModuleImp()
			: mCamera(3.14159f / 2, 1, 0.5, 200)
		{
		}
		virtual void InitializeModule(System* system) override
		{
			TickModule* tickModule = GET_MODULE(TickModule);
			tickModule->AddTickable(this);
			mFileModule = GET_MODULE(FileModule);
			mMonkeyConstantBuffer = new float[ConstantBufferSlots];
			mPlaneConstantBuffer = new float[ConstantBufferSlots];
			mFrame = 0;
			mClearColors[0] = V4F(1, 0, 0, 1);
			mClearColors[1] = V4F(0, 1, 0, 1);
			mClearColors[2] = V4F(0, 0, 1, 1);
			mClearColors[3] = V4F(0, 0, 0, 0);

			mWindowModule = GET_MODULE_AS(WindowsWindowModule, IWindowModule);
		}
		virtual void Tick() override
		{
			if (mDevice == nullptr)
			{
				DX12RenderDeviceModule* dev = GET_MODULE(DX12RenderDeviceModule);
				mDevice = dynamic_cast<RenderDevice*>(dev);
				Setup();
			}
			HandleInput();
			if (!mAllResourceReady)
				CheckResources();
			else
				RenderUpdate();
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
				size_t vertexStride = 8 * 4;//P3FN3FUV2F
				size_t indexStride = 4;
				{
					auto vb = mFileModule->ReadFileContent("Mesh/monkey_vb.bin");
					auto ib = mFileModule->ReadFileContent("Mesh/monkey_ib.bin");
					size_t indexCount = ib->GetSize() / indexStride;
					mMeshMonkey = mDevice->CreateMeshSimple(vb, ib, vertexStride, indexCount, indexStride);
				}
				{
					auto vb = mFileModule->ReadFileContent("Mesh/plane_vb.bin");
					auto ib = mFileModule->ReadFileContent("Mesh/plane_ib.bin");
					size_t indexCount = ib->GetSize() / indexStride;
					mMeshPlane = mDevice->CreateMeshSimple(vb, ib, vertexStride, indexCount, indexStride);
				}
			}
			{//RTs
				V2F size = mDevice->GetScreenSize();
				mDepthStencil = mDevice->CreateDepthStencilSimple((int)size.x, (int)size.y, TextureFormat::D24_UNORM_S8_UINT);
			}
		}
		void CheckResources()
		{
			if (!mShader->IsReady())
				return;
			if (!mPSO->IsReady())
				return;
			if (!mMeshMonkey->IsReady())
				return;
			if (!mMeshPlane->IsReady())
				return;
			mAllResourceReady = true;
		}
		void RenderUpdate()
		{
			//update constant
			{
				mCamera.Update(mPitch, mYaw, mPosition);
				const M44F& viewPerspective = mCamera.GetViewPerspectiveMatrix();
				{
					M44F world = M44F::Translate(V3F(0, 0, 20));
					M44F wvp = world * viewPerspective;
					memcpy(mMonkeyConstantBuffer, &wvp, sizeof(wvp));
				}
				{
					M44F world = M44F::Scale(V3F(25.0f)) * M44F::Translate(V3F(0, 1.5f, 0)) * M44F::Translate(V3F(0, 0, 20.0f));
					M44F wvp = world * viewPerspective;
					memcpy(mPlaneConstantBuffer, &wvp, sizeof(wvp));
				}
				
			}
			mDevice->BeginFrame();
			RenderDevicePass* pass = mDevice->AllocPass();
			pass->SetOutput(pass->GetBackbuffer(), 0);
			pass->SetClearColor(mClearColors[3], true, 0);
			pass->SetDepthStencil(mDepthStencil);
			pass->SetClearDepth(1.0f, 0, true, true);

			RenderDeviceDrawcall* cmd1 = (RenderDeviceDrawcall*)pass->AddCommand(RenderCommandType::Drawcall);
			cmd1->SetMesh(mMeshPlane.GetPtr());
			cmd1->SetPSO(mPSO.GetPtr());
			cmd1->SetConstantBuffer(mPlaneConstantBuffer, ConstantBufferSize, pass);

			RenderDeviceDrawcall* cmd0 = (RenderDeviceDrawcall*)pass->AddCommand(RenderCommandType::Drawcall);
			cmd0->SetMesh(mMeshMonkey.GetPtr());
			cmd0->SetPSO(mPSO.GetPtr());
			cmd0->SetConstantBuffer(mMonkeyConstantBuffer, ConstantBufferSize, pass);


			mDevice->ExecutePass(pass);
			mDevice->EndFrame();
			++mFrame;
		}
		void HandleInput()
		{
			const InputState* input = mWindowModule->GetInputState();
#if LOG_INPUT
			
			printf("MousePos:(%f,%f)\n",
				input->NormalizedMousePosition[0],
				input->NormalizedMousePosition[1]);
			std::vector<KeyCode> keycodes;
			for (int i = 0; i < (int)KeyCode::KeyCodeCount; ++i)
			{
				if (input->KeyStates[i])
				{
					keycodes.push_back((KeyCode)i);
				}
			}
			if (keycodes.size() > 0)
			{
				printf(">>>>>>>>>>>>>>\n");
				for (int i = 0; i < keycodes.size(); ++i)
				{
					printf("Pressed:%d\n", (KeyCode)keycodes[i]);
				}
				printf("<<<<<<<<<<<<<<\n");
			}
#endif
			if (!(input->KeyStates[(int)KeyCode::MouseLeft] || input->KeyStates[(int)KeyCode::MouseRight]))
				return;
			float rotateSpeed = 3.14159f;
			float moveSpeed = 0.5f;
			V4F xyzAxis[3];
			BuildAxis(mPitch, mYaw, xyzAxis);
			V3F zDir = V3F(xyzAxis[2].x, xyzAxis[2].y, xyzAxis[2].z);
			float forwardBackward = ((float)input->KeyStates[(int)KeyCode::Forward]) - (float)(input->KeyStates[(int)KeyCode::Backward]);
			mPosition += zDir * moveSpeed * forwardBackward;
			float upDown = ((float)input->KeyStates[(int)KeyCode::Up]) - (float)input->KeyStates[(int)KeyCode::Down];
			mPosition += (upDown * moveSpeed) * V3F(0, 1, 0);
			float leftRight = ((float)input->KeyStates[(int)KeyCode::Right]) - (float)input->KeyStates[(int)KeyCode::Left];
			mPosition += (leftRight * moveSpeed) * V3F(xyzAxis[0].x, xyzAxis[0].y, xyzAxis[0].z);
			mYaw += +input->DeltaNormalizedMousePosition[0] * rotateSpeed;
			mPitch += -input->DeltaNormalizedMousePosition[1] * rotateSpeed;
#if 0
			if (input->DeltaNormalizedMousePosition[0] != 0 || input->DeltaNormalizedMousePosition[1])
			{
				printf("delta normalized:(%f,%f)\n", input->DeltaNormalizedMousePosition[0], input->DeltaNormalizedMousePosition[1]);
			}
#endif
		}

		DEFINE_MODULE_IN_CLASS(RenderDeviceTestDriverModule, RenderDeviceTestDriverModuleImp);
	};
	DEFINE_MODULE_REGISTRY(RenderDeviceTestDriverModule, RenderDeviceTestDriverModuleImp, 1000);
}