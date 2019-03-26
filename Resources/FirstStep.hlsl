//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct GlobalConstants
{
	float4 param;
};
struct ObjectConstants
{
	float4 param;
};

ConstantBuffer<GlobalConstants> cGlobal[] : register(b0);
ConstantBuffer<ObjectConstants> cObject[]: register(b1);

Texture2D mTexture[16] : register(t0);
SamplerState m_sampler : register(s0);

struct VSInput
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
	float3 wpos : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD)
{
    PSInput result;
    result.position = float4(position, 1.0f);
	result.wpos = position;
	result.uv = uv;

    return result;
}

[RootSignature(
	"RootFlags("\
		"ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT|"\
		"DENY_HULL_SHADER_ROOT_ACCESS|"\
		"DENY_DOMAIN_SHADER_ROOT_ACCESS|"\
		"DENY_GEOMETRY_SHADER_ROOT_ACCESS),"\
	"CBV(b0, flags=DATA_STATIC),"\
	"CBV(b1, flags=DATA_STATIC),"\
	"DescriptorTable(SRV(t0, numDescriptors=16, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE)),"\
	"StaticSampler(s0,"\
		"addressU=TEXTURE_ADDRESS_BORDER,"\
		"addressV=TEXTURE_ADDRESS_BORDER,"\
		"filter=FILTER_MIN_MAG_MIP_LINEAR,"\
		"visibility=SHADER_VISIBILITY_PIXEL)")]
float4 PSMain(PSInput input) : SV_TARGET
{
	//return param0 / param1.w;
	float2 uvOffset = input.uv;
	float t = 0.01;
	uvOffset.x = frac(sin(cGlobal[0].param.x * t) + uvOffset.x);
	//uvOffset.y = frac(cos(mVSPrivate.param0.x * t) + uvOffset.y);
	uvOffset.y = frac(uvOffset.y);
	return mTexture[0].Sample(m_sampler, uvOffset);
}
