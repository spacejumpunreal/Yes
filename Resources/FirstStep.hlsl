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
	float4x4 wvp;
};

ConstantBuffer<GlobalConstants> cGlobal : register(b0);
ConstantBuffer<ObjectConstants> cObject: register(b1);

Texture2D mTexture[16] : register(t0);
SamplerState m_sampler : register(s0);

struct VSInput
{
	float3 position : POSITION;
	float3 normal: NORMAL;
	float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
	float3 wpos : TEXCOORD0;
	float3 normal: TEXCOORD1;
    float2 uv : TEXCOORD2;
};

PSInput VSMain(VSInput input)
{
    PSInput result;
	result.position = mul(cObject.wvp, float4(input.position, 1));
	result.wpos = input.position;
	result.normal = mul(cObject.wvp, float4(input.normal, 1));
	result.uv = input.uv;
    return result;
}

[RootSignature(
	"RootFlags("\
		"ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT|"\
		"DENY_HULL_SHADER_ROOT_ACCESS|"\
		"DENY_DOMAIN_SHADER_ROOT_ACCESS|"\
		"DENY_GEOMETRY_SHADER_ROOT_ACCESS),"\
	"CBV(b0, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE),"\
	"CBV(b1, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE),"\
	"DescriptorTable(SRV(t0, numDescriptors=16, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE)),"\
	"StaticSampler(s0,"\
		"addressU=TEXTURE_ADDRESS_BORDER,"\
		"addressV=TEXTURE_ADDRESS_BORDER,"\
		"filter=FILTER_MIN_MAG_MIP_LINEAR,"\
		"visibility=SHADER_VISIBILITY_PIXEL)")]
float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(input.normal, 1);
	//return float4(1, 0, 0, 1);
}
