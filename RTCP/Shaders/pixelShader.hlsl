#ifndef _PIXEL_SHADER_HLSL
#define _PIXEL_SHADER_HLSL

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

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
	//return float4(1, 0, 0, 1);
	return float4(g_texture.Sample(g_sampler, input.uv).rgb, 1.0f);
}

#endif //_PIXEL_SHADER_HLSL