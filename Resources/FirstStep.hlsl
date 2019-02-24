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

cbuffer ConstnatBuffer : register(b0)
{
	float4 param0;
	float4 param1;
};

Texture2D m_texture : register(t0);
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

float4 PSMain(PSInput input) : SV_TARGET
{
	//return param0 / param1.w;
	float2 uvOffset = input.uv;
	float t = 0.01;
	uvOffset.x = frac(sin(param0.x * t) + uvOffset.x);
	uvOffset.y = frac(cos(param0.y * t) + uvOffset.y);
	return m_texture.Sample(m_sampler, uvOffset);
}
