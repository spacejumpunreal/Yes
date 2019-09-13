#include "Toys/SWR/Public/SWR.h"
#include "Runtime/Public/Misc/Time.h"

#include <cstdio>

using namespace Yes::SWR;


struct VF_P3F_C3F
{
	float Position[3];
	float Color[3];
};

struct TestVSOutput
{
	float Position[4];
	float Color[3];
};


#define RED {1.0f, 0, 0}
#define GREEN {0, 1.0f, 0}
#define BLUE {0, 0, 1.0f}
#define WHITE {1.0f, 1.0f, 1.0f}

#define BL { -1.0f, 0.0f, -1.0f}
#define BR { +1.0f, 0.0f, -1.0f}
#define TL { -1.0f, 0.0f, +1.0f}
#define TR { -1.0f, 0.0f, +1.0f}

#define V0 { TL, RED }
#define V1 { TR, GREEN }
#define V2 { BR, BLUE }
#define V3 { BL, WHITE }

VF_P3F_C3F TestVB[] = 
{
	V0, V1, V2,
	V0, V2, V3,
};

void TestVertexShader(void* _vsInput, void* _vsOutput)
{
	auto& vsInput = *(VF_P3F_C3F*)_vsInput;
	auto& vsOutput = *(VF_P3F_C3F*)_vsOutput;
	vsOutput = vsInput;
}

bool TestPixelShader(void* _vsOutput, void* _psOutput)
{
	auto& vsOutput = *(VF_P3F_C3F*)_vsOutput;
	auto& psOutput = *(SWRPSOutput<1>*)_psOutput;
	memcpy(psOutput.Color, vsOutput.Color, sizeof(float[4]));
	return true;
}

int main2()
{
	DeviceDesc desc;
	desc.BackBufferHeight = 800;
	desc.BackBufferHeight = 600;
	desc.TileCountU = 8;
	desc.TileCountV = 4;
	desc.NumThreads = 8;
	auto dev = CreateSWRDevice(&desc);

	dev->Test(1104);
	dev->Test(2105);
	dev->Test(3003);
	dev->Test(-1);
	dev->Test(4002);
	dev->Test(5005);
	dev->Test(6005);
	dev->Test(-1);
	//dev->Test(-1);
	dev->Test(7003);

	Yes::Sleep(9999);
	return 0;
}

int main()
{
	DeviceDesc desc;
	desc.BackBufferHeight = 800;
	desc.BackBufferHeight = 600;
	desc.NumThreads = 8;
	auto dev = CreateSWRDevice(&desc);
	SWRHandle vb;
	{
		BufferDesc desc1;
		desc1.Size = sizeof(TestVB);
		desc1.InitialData = TestVB;
		vb = dev->CreateBuffer(&desc1);
		dev->IASetVertexBuffer(vb);
	}
	SWRHandle vs;
	{
		VertexShaderDesc desc2;
		desc2.InputStride = sizeof(VF_P3F_C3F);
		desc2.OutputStride = sizeof(VF_P3F_C3F);
		desc2.Function = TestVertexShader;
		vs = dev->CreateVertexShader(&desc2);
		dev->VSSetShader(vs);
	}
	SWRHandle ps;
	{
		PixelShaderDesc desc3;
		desc3.Function = TestPixelShader;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    
	}
	dev->Draw();
	dev->Present();
	Yes::Sleep(2);
	while (true)
		Yes::Sleep(99999);
	return 0;
}