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
		TRef<RenderDeviceDescriptorHeapRange> DescriptorTable;
		TRef<RenderDeviceConstantBuffer> LocalConstant;
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
		TRef<RenderDeviceConstantBuffer> mGlobalCB;
		TRef<RenderDeviceDescriptorHeapRange> mGlobalRanges[3];
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
			, mShadowCamera(Camera::BuildOrthogonalCamera(1, 50, 0, 100))
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
			{//RTs
				V2F size = mDevice->GetScreenSize();
				mDepthStencil = mDevice->CreateTexture2D(
					(int)size.x, (int)size.y,
					TextureFormat::D32_UNORM_S8_UINT, TextureUsage::DepthStencil,
					nullptr);
				for (int i = 0; i < 3; ++i)
				{
					size_t Size = 512;
					mShadowDepth[i] = mDevice->CreateTexture2D(
						Size, Size,
						TextureFormat::D32_UNORM_S8_UINT, TextureUsage::DepthStencil,
						nullptr);
				}
			}
			//RenderObject
			{
				size_t vertexStride = 8 * 4;//P3FN3FUV2F
				size_t indexStride = 4;
				size_t NStatue = 1;
				size_t NMonkey = 5;
				size_t NFloor = 1;
				size_t NShadowRT = 3;
				TRef<RenderDeviceDescriptorHeap> heap = mDevice->CreateDescriptorHeap(false, 1024);
				size_t availableStart = 0;
				for (int i = 0; i < NShadowRT; ++i)
				{
					mGlobalRanges[i] = heap->CreateRange(availableStart, 1);
					const RenderDeviceTexture* tex = mShadowDepth[i].GetPtr();
					mGlobalRanges[i]->SetRange(0, 1, &tex);
					availableStart += 1;
				}
				{
					auto vb = mFileModule->ReadFileContent("Model/MonkeyHead/MonkeyHead_vb.bin");
					auto ib = mFileModule->ReadFileContent("Model/MonkeyHead/MonkeyHead_ib.bin");
					size_t indexCount = ib->GetSize() / indexStride;
					RenderDeviceMeshRef mesh = mDevice->CreateMeshSimple(vb, ib, vertexStride, indexCount, indexStride);

					auto baseMapBlob = mFileModule->ReadFileContent("Model/MonkeyHead/MonkeyBody.png");
					TRef<RawImage> rimage = LoadRawImage(baseMapBlob.GetPtr());
					RenderDeviceTextureRef tex = mDevice->CreateTexture2D(
						0, 0, TextureFormat::R8G8B8A8_UNORM, TextureUsage::ShaderResource,
						rimage.GetPtr());
					auto range = heap->CreateRange(availableStart, 1);
					const RenderDeviceTexture* texx = tex.GetPtr();
					range->SetRange(0, 1, &texx);
					availableStart += 1;
					for (int i = 0; i < NMonkey; ++i)
					{
						mObjects.push_back({});
						RenderObject& ro = mObjects.back();
						ro.Mesh = mesh;
						ro.Texture = tex;
						ro.DescriptorTable = range;
						ro.Transform = M44F::Translate(V3F((float)(3 * i), 2, (float)(10 + 5 * i)));
					}
				}
				{
					auto vb = mFileModule->ReadFileContent("Model/Statue/Statue_vb.bin");
					auto ib = mFileModule->ReadFileContent("Model/Statue/Statue_ib.bin");
					size_t indexCount = ib->GetSize() / indexStride;
					RenderDeviceMeshRef mesh = mDevice->CreateMeshSimple(vb, ib, vertexStride, indexCount, indexStride);
					///*
					TRef<RawImage> rimage = BuildSingleColorTexture(V4F(1, 0, 0, 1), 16);
					RenderDeviceTextureRef tex = mDevice->CreateTexture2D(
						0, 0, TextureFormat::R8G8B8A8_UNORM, TextureUsage::ShaderResource,
						rimage.GetPtr());
					//*/
					/*
					auto baseMapBlob = mFileModule->ReadFileContent("Model/Statue/BaseColor.jpeg");
					TRef<RawImage> rimage = LoadRawImage(baseMapBlob.GetPtr());
					RenderDeviceTextureRef tex = mDevice->CreateTexture2D(
						0, 0, TextureFormat::R8G8B8A8_UNORM, TextureUsage::ShaderResource,
						rimage.GetPtr());
					*/
					auto range = heap->CreateRange(availableStart, 1);
					const RenderDeviceTexture* texx = tex.GetPtr();
					range->SetRange(0, 1, &texx);
					availableStart += 1;
					for (int i = 0; i < NStatue; ++i)
					{
						mObjects.push_back({});
						RenderObject& ro = mObjects.back();
						ro.Mesh = mesh;
						ro.Texture = tex;
						ro.DescriptorTable = range;
						ro.Transform = M44F::Translate(V3F((float)(3 * i), 0, (float)(10 + 5 * i)));
						ro.LocalConstant = mDevice->CreateConstantBuffer(false, ConstantBufferSize);
						memcpy(mTempBuffer, &ro.Transform, sizeof(M44F));
						ro.LocalConstant->Write(0, ConstantBufferSize, mTempBuffer);
						
					}
				}
				{
					auto vb = mFileModule->ReadFileContent("Model/Plane/Plane_vb.bin");
					auto ib = mFileModule->ReadFileContent("Model/Plane/Plane_ib.bin");
					size_t indexCount = ib->GetSize() / indexStride;
					TRef<RawImage> rimage = BuildSingleColorTexture(V4F(.5f, .5f, .5f, 1), 16);
					RenderDeviceTextureRef tex = mDevice->CreateTexture2D(
						0, 0,
						TextureFormat::R8G8B8A8_UNORM,
						TextureUsage::ShaderResource,
						rimage.GetPtr());
					auto range = heap->CreateRange(availableStart, 1);
					const RenderDeviceTexture* texx = tex.GetPtr();
					range->SetRange(0, 1, &texx);
					availableStart += 1;

					TRef<RawImage> rimage1 = BuildCheckTexture(V4F(0, 0, 0, 1), V4F(1, 1, 1, 1), 1024, 1);
					RenderDeviceTextureRef tex1 = mDevice->CreateTexture2D(
						0, 0,
						TextureFormat::R8G8B8A8_UNORM,
						TextureUsage::ShaderResource,
						rimage1.GetPtr());
					auto range1 = heap->CreateRange(availableStart, 1);
					const RenderDeviceTexture* texx1 = tex1.GetPtr();
					range1->SetRange(0, 1, &texx1);
					availableStart += 1;
					auto mesh = mDevice->CreateMeshSimple(vb, ib, vertexStride, indexCount, indexStride);
					{
						mObjects.push_back({});
						RenderObject& ro = mObjects.back();
						ro.Mesh = mesh;
						ro.Texture = tex;
						ro.DescriptorTable = range;
						ro.Transform = M44F::Scale(V3F(25.0f)) * M44F::Translate(V3F(0, 0, 30.0f));
					}
					{
						mObjects.push_back({});
						RenderObject& ro = mObjects.back();
						ro.Mesh = mesh;
						ro.Texture = tex1;
						ro.DescriptorTable = range1;
						ro.Transform = M44F::Scale(V3F(25.0f)) * M44F::RotateX(+3.1415f / 2) * M44F::Translate(V3F(0, 15, 30.0f));
					}
					const int N = 32;
					V3F startPoint = V3F(5, 0, 0);
					for (int i = 0; i < N; ++i)
					{
						float v = ((float)i) / (N - 1);
						float pv = std::powf(v, 1.0f / 2.2f);
						TRef<RawImage> rimage0 = BuildSingleColorTexture(V4F(v, v, v, 1), 4);
						TRef<RawImage> rimage1 = BuildSingleColorTexture(V4F(pv, pv, pv, 1), 4);
						RenderDeviceTextureRef tex0 = mDevice->CreateTexture2D(
							0, 0,
							TextureFormat::R8G8B8A8_UNORM,
							TextureUsage::ShaderResource,
							rimage0.GetPtr());
						RenderDeviceTextureRef tex1 = mDevice->CreateTexture2D(
							0, 0,
							TextureFormat::R8G8B8A8_UNORM,
							TextureUsage::ShaderResource,
							rimage1.GetPtr());
						auto range0 = heap->CreateRange(availableStart, 1);
						availableStart += 1;
						auto range1 = heap->CreateRange(availableStart, 1);
						availableStart += 1;
						const RenderDeviceTexture* texx0 = tex0.GetPtr();
						const RenderDeviceTexture* texx1 = tex1.GetPtr();
						range0->SetRange(0, 1, &texx0);
						range1->SetRange(0, 1, &texx1);
						{
							mObjects.push_back({});
							RenderObject& ro = mObjects.back();
							ro.Mesh = mesh;
							ro.Texture = tex0;
							ro.DescriptorTable = range0;
							ro.Transform = M44F::Scale(V3F(1.0f)) * M44F::RotateX(3.1415f / 2.0f) * M44F::RotateY(3.1415f / 2.0f) * M44F::Translate(startPoint + V3F(0, 0.0f, (float)4 * i));
						}
						{
							mObjects.push_back({});
							RenderObject& ro = mObjects.back();
							ro.Mesh = mesh;
							ro.Texture = tex1;
							ro.DescriptorTable = range1;
							ro.Transform = M44F::Scale(V3F(1)) * M44F::RotateX(3.1415f / 2.0f) * M44F::RotateY(3.1415f / 2.0f) * M44F::Translate(startPoint + V3F(0, 3, (float)4 * i));
						}
					}
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
			for (size_t i = 0; i < mObjects.size(); ++i)
			{
				if (mObjects[i].LocalConstant.GetPtr() == nullptr)
				{
					float rad = mObjects[i].Transform.Translation().z * 0.1f;
					float phase = (mFrame / 180.0f * 3.1415f * 2.0f + rad);
					mObjects[i].Transform.Translation() += V3F(0, 1.0f * std::sin(phase), 0);
				}
			}
		}
		void UpdateGlobals()
		{
			mGlobalCB = mDevice->CreateConstantBuffer(true, ConstantBufferSize);
			mGlobalCB->Write(0, ConstantBufferSize, mGlobalBuffer);
		}
		void RenderShadowPass()
		{
			int fi = mFrame % 3;
			RenderDevicePass* pass = mDevice->AllocPass(1);
			pass->SetDepthStencil(mShadowDepth[fi], true, 1.0f, true, 0, true, &mShadowDepth[fi]->GetDefaultViewport());
			pass->SetArgument(0, mGlobalCB.GetPtr());
			for (int i = 0; i < mObjects.size(); ++i)
			{
				RenderObject& ro = mObjects[i];
				auto* dc = (RenderDeviceDrawcall*)pass->AddCommand(RenderCommandType::Drawcall);
				if (ro.LocalConstant.GetPtr() != nullptr)
				{
					dc->SetArgument(0, ro.LocalConstant.GetPtr());
				}
				else
				{
					TRef<RenderDeviceConstantBuffer> cb = mDevice->CreateConstantBuffer(true, ConstantBufferSize);
					memcpy(mTempBuffer, &ro.Transform, sizeof(M44F));
					cb->Write(0, ConstantBufferSize, mTempBuffer);
					dc->SetArgument(0, cb.GetPtr());
				}
				dc->SetMesh(ro.Mesh.GetPtr());
				dc->SetPSO(mShadowPSO.GetPtr());
			}
			mDevice->ExecutePass(pass);
		}
		void RenderNormalPass()
		{
			int fi = mFrame % 3;
			RenderDevicePass* pass = mDevice->AllocPass(2);
			V2F screenSize = mDevice->GetScreenSize();
			pass->SetOutput(0, pass->GetBackbuffer(), true, mClearColors[3], pass->GetBackbuffer()->GetDefaultViewport());
			pass->SetDepthStencil(mDepthStencil, true, 1.0f, true, 0, false, nullptr);
			pass->SetArgument(0, mGlobalCB.GetPtr());
			{
				auto* barrier = (RenderDeviceBarrier*)pass->AddCommand(RenderCommandType::Barrier);
				TRef<RenderDeviceResource> ds = mShadowDepth[fi].GetPtr();
				barrier->AddResourceBarrier(ds, RenderDeviceResourceState::SHADER_RESOURCE);
				pass->SetArgument(1, mGlobalRanges[fi].GetPtr());
			}
			for (int i = 0; i < mObjects.size(); ++i)
			{
				RenderObject& ro = mObjects[i];
				auto* dc = (RenderDeviceDrawcall*)pass->AddCommand(RenderCommandType::Drawcall);
				if (ro.LocalConstant.GetPtr() != nullptr)
				{
					dc->SetArgument(0, ro.LocalConstant.GetPtr());
				}
				else
				{
					TRef<RenderDeviceConstantBuffer> cb = mDevice->CreateConstantBuffer(true, ConstantBufferSize);
					memcpy(mTempBuffer, &ro.Transform, sizeof(M44F));
					cb->Write(0, ConstantBufferSize, mTempBuffer);
					dc->SetArgument(0, cb.GetPtr());
				}
				dc->SetArgument(1, ro.DescriptorTable.GetPtr());
				dc->SetMesh(ro.Mesh.GetPtr());
				dc->SetPSO(mNormalPSO.GetPtr());
				dc->SetDescriptorHeap(ro.DescriptorTable->GetDescriptorHeap().GetPtr());
			}
			mDevice->ExecutePass(pass);
		}
		void RenderUpdate()
		{
			UpdateObjets();
			mDevice->BeginFrame();
			UpdateGlobals();
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