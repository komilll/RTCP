#ifndef _PIXEL_SHADER_HLSL
#define _PIXEL_SHADER_HLSL

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD0;
};

float4 main(PixelInputType input) : SV_TARGET
{
	return float4(input.normal, 1.0f);
}

#endif //_PIXEL_SHADER_HLSL