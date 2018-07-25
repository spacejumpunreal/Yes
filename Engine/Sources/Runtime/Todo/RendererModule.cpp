#include "Demo18.h"
#include "System.h"
#include "RendererModule.h"
#include "RendererObjects.h"
#include "VertexFormat.h"
#include "Math.h"

namespace Demo18
{
	struct TestVSConstants
	{
		M44F World;
		M44F WorldView;
		M44F WorldViewPerspective;
	};
	struct TestPSConstants
	{
		V4F Color;
	};
	struct RendererModuleImp : public RendererModule
	{
	public:
		virtual void Start()
		{
			TestStart();
		}
		virtual void RunRenderer(RendererScene* scene)
		{
			TestRunRenderer(scene);
		}
	protected:
		void TestStart()
		{
			mTestDrawcalls = new RendererDrawcall();
			mTestDrawcalls->Next = nullptr;
			{
				auto vertexShader = CreateRendererShader("Shader/Simple.hlsl", "VS");
				mTestDrawcalls->VertexShader = IRef<RendererShader>(vertexShader);
				auto pixelShader = CreateRendererShader("Shader/Simple.hlsl", "PSFinal");
				mTestDrawcalls->PixelShader = IRef<RendererShader>(pixelShader);
			}
			{
				float A = 3.0f;
				VF_P3F_N3F_T2F vb[] = {
					{ { -A, +A, -A },{ 0, 0, 0 },{ 0, 0 } },
					{ { -A, +A, +A },{ 0, 0, 0 },{ 1, 0 } },
					{ { +A, +A, +A },{ 0, 0, 0 },{ 0, 0 } },
					{ { +A, +A, -A },{ 0, 0, 0 },{ 1, 0 } },
					{ { -A, -A, -A },{ 0, 0, 0 },{ 0, 1 } },
					{ { -A, -A, +A },{ 0, 0, 0 },{ 1, 1 } },
					{ { +A, -A, +A },{ 0, 0, 0 },{ 0, 1 } },
					{ { +A, -A, -A },{ 0, 0, 0 },{ 1, 1 } },
				};
				//index buffer
				uint16_t ib[] = {
					0, 3, 4,
					3, 7, 4,
					1, 0, 5,
					5, 0, 4,
					2, 1, 5,
					2, 5, 6,
					3, 2, 6,
					3, 6, 7,
					1, 2, 0,
					2, 3, 0,
					6, 5, 7,
					5, 4, 7,
				};
				auto geometry = CreateRendererGeometry(vb, ib);
				mTestDrawcalls->Geometry = IRef<RendererGeometry>(geometry);
			}
			{
				auto texture = CreateRendererTexture2D("SimpleTest/UVChecker.dds");
				std::vector<IRef<RendererTexture2D>> textures;
				textures.emplace_back(texture);
				mTestDrawcalls->Textures = std::move(textures);
			}
			{
				auto sampler = CreateRendererSamplerState();
				mTestDrawcalls->Samplers.emplace_back(sampler);
			}
			{
				mTestVSConstant.World = M44F::Identity();
				mTestVSConstant.WorldView = M44F::Identity();
				mTestVSConstant.WorldViewPerspective = M44F::Identity();
				mTestPSConstant.Color = V4F(1, 1, 1, 1);
				auto cvs = mTestVSConstantBuffer = CreateRendererConstantBuffer(sizeof(mTestVSConstant));
				mTestDrawcalls->VSConstants[0] = IRef<RendererConstantBuffer>(cvs);
				auto cps = mTestPSConstantBuffer = CreateRendererConstantBuffer(sizeof(mTestPSConstant));
				mTestDrawcalls->PSConstants[0] = IRef<RendererConstantBuffer>(cps);
			}
			mTestPass = new RendererPass(mTestDrawcalls, V4F(0, 0, 0, 1), 1.0f, 0);
		}
		void TestRunRenderer(RendererScene* scene)
		{
			auto rdm = GET_MODULE(RenderDevice);
			rdm->BeginFrame();
			{
				mTestVSConstant.World = M44F::Identity();
				mTestVSConstant.WorldView = scene->Camera.MWorld2View;
				mTestVSConstant.WorldViewPerspective = scene->Camera.MWorld2View * scene->Camera.MPerspective;
				mTestVSConstantBuffer->Update(mTestVSConstant);
			}
			{
				mTestPSConstant.Color += V4F(0.01f, 0.01f, 0.01f, 0.01f);
				if (mTestPSConstant.Color.w > 1.0)
					mTestPSConstant.Color = V4F(1, 1, 1, 1);
				mTestPSConstantBuffer->Update(mTestPSConstant);
			}
			rdm->RenderPass(mTestPass);
			rdm->EndFrame();
		}

		
	protected:
		//>>>>>>>>>> these are for test
		RendererPass* mTestPass;
		RendererDrawcall* mTestDrawcalls;
		TestVSConstants mTestVSConstant;
		TestPSConstants mTestPSConstant;
		RendererConstantBuffer* mTestVSConstantBuffer;
		RendererConstantBuffer* mTestPSConstantBuffer;

		//<<<<<<<<<< these are for test
	};
	RendererModule* CreateRendererModule()
	{
		return new RendererModuleImp();
	}
}