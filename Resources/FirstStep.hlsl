struct GlobalConstants
{
	float4x4 MCameraViewPerspective;
	float4x4 MShadowViewPerspective;
};
struct ObjectConstants
{
	float4x4 MWorld;
};

ConstantBuffer<GlobalConstants> cGlobal : register(b0);
ConstantBuffer<ObjectConstants> cObject: register(b1);

Texture2D tGlobal[1] : register(t0);
Texture2D tObject[1] : register(t1);

SamplerState mSampler : register(s0);

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
	float4 wpos = mul(cObject.MWorld, float4(input.position, 1));
	result.position = mul(cGlobal.MCameraViewPerspective, wpos);
	result.wpos = wpos.xyz;
	result.normal = input.normal;
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
	"DescriptorTable(SRV(t0, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE)),"\
	"DescriptorTable(SRV(t1, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE)),"\
	"StaticSampler(s0,"\
		"addressU=TEXTURE_ADDRESS_CLAMP,"\
		"addressV=TEXTURE_ADDRESS_CLAMP,"\
		"filter=FILTER_MIN_MAG_MIP_POINT,"\
		"visibility=SHADER_VISIBILITY_PIXEL)")]
float4 PSMain(PSInput input) : SV_TARGET
{
	const float bias = 0.001;
	float4 shadowPos = mul(cGlobal.MShadowViewPerspective, float4(input.wpos, 1));
	shadowPos /= shadowPos.w;
	float4 litColor = float4(1,1,1,1);
	if (all(abs(shadowPos.xy) <= float2(1,1)))
	{
		float2 shadowUV = (shadowPos.xy + float2(1, 1)) * 0.5;
		shadowUV.y = 1 - shadowUV.y;
		float lit = shadowPos.z < bias + tGlobal[0].SampleLevel(mSampler, shadowUV, 0).r;
		litColor *= lit;
	}
	else
	{
		litColor = float4(1, 0, 1, 0);
	}
	return tObject[0].SampleLevel(mSampler, float2(input.uv.x, 1 - input.uv.y), 0) * litColor;
	//return lit;
}
