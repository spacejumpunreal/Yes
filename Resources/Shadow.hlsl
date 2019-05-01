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

struct VSInput
{
	float3 position : POSITION;
	float3 normal: NORMAL;
	float2 uv : TEXCOORD;
};

struct PSInput
{
	float4 position : SV_POSITION;
};

PSInput VSMain(VSInput input)
{
	PSInput result;
	float4 wpos = mul(cObject.MWorld, float4(input.position, 1));
	result.position = mul(cGlobal.MShadowViewPerspective, wpos);
	return result;
}

[RootSignature(
	"RootFlags("\
	"ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT|"\
	"DENY_HULL_SHADER_ROOT_ACCESS|"\
	"DENY_DOMAIN_SHADER_ROOT_ACCESS|"\
	"DENY_GEOMETRY_SHADER_ROOT_ACCESS),"\
	"CBV(b0, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE),"\
	"CBV(b1, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE)")]
void PSMain(PSInput input) 
{
}
