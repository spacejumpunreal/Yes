#include "Public/Graphics/Test/RenderDeviceTestDriverModule.h"
#include "Public/Core/System.h"
#include "Public/Core/TickModule.h"
#include "Public/Core/FileModule.h"
#include "Graphics/ImageUtil.h"
#include "Public/Platform/IWindowModule.h"
#include "Public/Misc/Utils.h"
#include "Public/Misc/Debug.h"
#include "Public/Misc/Container.h"
#include "Public/Graphics/RenderDevice.h"
#include "Public/Platform/WindowsWindowModule.h"
#include "Public/Platform/DX12/DX12RenderDeviceModule.h"
#include "Public/Platform/InputState.h"
#include "Public/Graphics/Camera/Camera.h"

#include <vector>

#define LOG_INPUT 0

namespace Yes
{
	struct RenderObject
	{
		TRef<RenderDeviceMesh> Mesh;
		TRef<RenderDeviceTexture> Texture;
		M44F Transform;
	};

	class RenderDeviceTestDriverModuleImp : public RenderDeviceTestDriverModule, public ITickable
	{
	private:
		RenderDevice* mDevice = nullptr;
		FileModule* mFileModule = nullptr;
		IWindowModule* mWindowModule = nullptr;
		TRef<RenderDevicePSO> mNormalPSO;
		TRef<RenderDevicePSO> mShadowPSO;
		static const size_t ConstantBufferSize = 512;
		static const size_t ConstantBufferSlots = 512 / 4;
		float* mTempBuffer;
		uint8* mGlobalBuffer;
		TRef<RenderDeviceTexture> mDepthStencil;
		TRef<RenderDeviceTexture> mShadowDepth[3];
		std::vector<RenderObject> mObjects;
		int mFrame;
		V4F mClearColors[4];
		bool mAllResourceReady = false;
		float mYaw = 0;
		float mPitch = 0;
		V3F mPosition = V3F(0, 2.f, 0);
		Camera mEyeCamera;
		Camera mShadowCamera;
	public:
		RenderDeviceTestDriverModuleImp()
			: mEyeCamera(Camera::BuildPerspectiveCamera(3.14159f / 2, 1, 0.5, 200))
			//: mEyeCamera(Camera::BuildOrthogonalCamera(1, 50, 0, 100))
			, mShadowCamera(Camera::BuildOrthogonalCamera(1, 8, 0, 100))
		{
		}
		virtual void InitializeModule(System* system) override
		{
			TickModule* tickModule = GET_MODULE(TickModule);
			tickModule->AddTickable(this);
			mFileModule = GET_MODULE(FileModule);
			mTempBuffer = new float[ConstantBufferSlots];
			mGlobalBuffer = (byte*)new float[ConstantBufferSlots];
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
			//Shader & PSO
			{
				SharedBufferRef shaderContent = mFileModule->ReadFileContent("FirstStep.hlsl");
				TRef<RenderDeviceShader> normalShader = mDevice->CreateShaderSimple(shaderContent, "FirstStep.hlsl");
				shaderContent = mFileModule->ReadFileContent("Shadow.hlsl");
				TRef<RenderDeviceShader> shadowShader = mDevice->CreateShaderSimple(shaderContent, "Shadow.hlsl");
				RenderDevicePSODesc desc;

				desc.RTCount = 1;
				desc.RTFormats[0] = TextureFormat::R8G8B8A8_UNORM;
				desc.DSFormat = TextureFormat::D32_UNORM_S8_UINT;
				desc.Shader = normalShader;
				desc.StateKey = PSOStateKey::Normal;
				desc.VF = VertexFormat::VF_P3F_T2F;
				mNormalPSO = mDevice->CreatePSOSimple(desc);

				desc.RTCount = 0;
				desc.DSFormat = TextureFormat::D32_UNORM_S8_UINT;
				desc.Shader = shadowShader;
				desc.StateKey = PSOStateKey::Normal;
				mShadowPSO = mDevice->CreatePSOSimple(desc);
			}
			//RenderObject
			{
				size_t vertexStride = 8 * 4;//P3FN3FUV2F
				size_t indexStride = 4;
				{
					mObjects.push_back({});
					RenderObject& ro = mObjects.back();
					auto vb = mFileModule->ReadFileContent("Model/MonkeyHead/MonkeyHead_vb.bin");
					auto ib = mFileModule->ReadFileContent("Model/MonkeyHead/MonkeyHead_ib.bin");
					size_t indexCount = ib->GetSize() / indexStride;
					ro.Mesh = mDevice->CreateMeshSimple(vb, ib, vertexStride, indexCount, indexStride);

					auto baseMapBlob = mFileModule->ReadFileContent("Model/MonkeyHead/MonkeyBody.png");
					TRef<RawImage> rimage = LoadRawImage(baseMapBlob.GetPtr());
					ro.Texture = mDevice->CreateTexture2D(
						0, 0,
						TextureFormat::R8G8B8A8_UNORM,
						TextureUsage::ShaderResource,
						rimage.GetPtr());
					ro.Transform = M44F::Translate(V3F(0, 3, 20));
				}
				{
					mObjects.push_back({});
					RenderObject& ro = mObjects.back();
					auto vb = mFileModule->ReadFileContent("Mesh/plane_vb.bin");
					auto ib = mFileModule->ReadFileContent("Mesh/plane_ib.bin");
					size_t indexCount = ib->GetSize() / indexStride;
					ro.Mesh = mDevice->CreateMeshSimple(vb, ib, vertexStride, indexCount, indexStride);
					auto baseMapBlob = mFileModule->ReadFileContent("Model/Plane/Plane_basemap.png");
					TRef<RawImage> rimage = LoadRawImage(baseMapBlob.GetPtr());
					ro.Texture = mDevice->CreateTexture2D(
						0, 0, TextureFormat::R8G8B8A8_UNORM,
						TextureUsage::ShaderResource, rimage.GetPtr());
					ro.Transform = M44F::Scale(V3F(25.0f)) * M44F::Translate(V3F(0, 0, 20.0f));
				}
			}
			{//RTs
				V2F size = mDevice->GetScreenSize();
				mDepthStencil = mDevice->CreateTexture2D(
					(int)size.x, (int)size.y, 
					TextureFormat::D32_UNORM_S8_UINT, TextureUsage::DepthStencil, 
					nullptr);
				for (int i = 0; i < 3; ++i)
				{
					mShadowDepth[i] = mDevice->CreateTexture2D(
						1024, 1024,
						TextureFormat::D32_UNORM_S8_UINT, TextureUsage::DepthStencil,
						nullptr);
				}
			}
		}
		void CheckResources()
		{
			for (int i = 0; i < mObjects.size(); ++i)
			{
				if (!mObjects[i].Mesh->IsReady())
				{
					return;
				}
				if (!mObjects[i].Texture->IsReady())
				{
					return;
				}
			}
			mAllResourceReady = true;
		}
		void UpdateObjets()
		{
			mEyeCamera.UpdateView(mPitch, mYaw, mPosition);
			auto v2w = M44F::LookAt(V3F(0, -1, 0), V3F(1, 0, 0), V3F(0, 50, 20));
			auto w2v = v2w.Inverse();
			mShadowCamera.UpdateView(w2v);
			{
				auto mvp = mShadowCamera.GetMVPMatrix();
				V4F v4 = V4F(0, 0, 0, 1);
				V4F v42 = V4F(0, 3, 20, 1);
				V4F transformedV4 = v4 * mvp;
				transformedV4 = v42 * mvp;
				auto transformedV44 = v42 * w2v;
				transformedV44 = v42 * w2v;
			}
			
			memset(mGlobalBuffer, 0, ConstantBufferSize);
			memcpy(mGlobalBuffer, &mEyeCamera.GetMVPMatrix(), sizeof(M44F));
			memcpy(mGlobalBuffer + sizeof(M44F), &mShadowCamera.GetMVPMatrix(), sizeof(M44F));
		}
		void RenderNormalPass()
		{
			RenderDevicePass* pass = mDevice->AllocPass();
			pass->SetOutput(pass->GetBackbuffer(), 0);
			pass->SetClearColor(mClearColors[3], true, 0);
			pass->SetDepthStencil(mDepthStencil);
			pass->SetClearDepth(1.0f, 0, true, true);
			pass->SetGlobalConstantBuffer(mGlobalBuffer, ConstantBufferSize);
			for (int i = 0; i < mObjects.size(); ++i)
			{
				RenderObject& ro = mObjects[i];
				auto* dc = (RenderDeviceDrawcall*)pass->AddCommand(RenderCommandType::Drawcall);
				memcpy(mTempBuffer, &ro.Transform, sizeof(M44F));
				//auto tmp = ro.Transform * mEyeCamera.GetMVPMatrix();
				//memcpy(mTempBuffer, &tmp, sizeof(M44F));
				dc->SetConstantBuffer(mTempBuffer, ConstantBufferSize, pass);
				dc->SetMesh(ro.Mesh.GetPtr());
				dc->SetTextures(0, ro.Texture.GetPtr());
				dc->SetPSO(mNormalPSO.GetPtr());
			}
			mDevice->ExecutePass(pass);
		}
		void RenderShadowPass()
		{
			RenderDevicePass* pass = mDevice->AllocPass();
			int fi = mDevice->GetFrameIndex();
			fi %= 3;
			pass->SetDepthStencil(mShadowDepth[fi]);
			pass->SetClearDepth(1.0, 0, true, false);
			pass->SetGlobalConstantBuffer(mGlobalBuffer, ConstantBufferSize);
			for (int i = 0; i < mObjects.size(); ++i)
			{
				RenderObject& ro = mObjects[i];
				auto* dc = (RenderDeviceDrawcall*)pass->AddCommand(RenderCommandType::Drawcall);
				memcpy(mTempBuffer, &ro.Transform, sizeof(M44F));
				dc->SetConstantBuffer(mTempBuffer, ConstantBufferSize, pass);
				dc->SetMesh(ro.Mesh.GetPtr());
				dc->SetPSO(mShadowPSO.GetPtr());
			}
			mDevice->ExecutePass(pass);
		}
		void RenderUpdate()
		{
			UpdateObjets();
			mDevice->BeginFrame();
			RenderShadowPass();
			RenderNormalPass();
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