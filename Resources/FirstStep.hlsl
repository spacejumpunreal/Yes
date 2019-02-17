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

//PSInput VSMain(VSInput input)
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
	return float4(input.wpos.xy, 0.0f, 1.0f);
	//return float4(1, 1, 1, 1);
}
