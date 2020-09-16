#ifndef _PS_POSTPROCESS_HLSL_
#define _PS_POSTPROCESS_HLSL_

struct PostprocessConstantBuffer
{
	float exposure;
	float padding[63];
};

ConstantBuffer<PostprocessConstantBuffer> g_postprocessCB : register(b0);
Texture2D<float4> g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD0;
	uint textureID : TEXCOORD1;
};

float4 main(PixelInputType input) : SV_TARGET
{
	//return float4((float) input.textureID / 36.0f, 0, 0, 1);
	float3 color = g_texture.Sample(g_sampler, input.uv).rgb;
		
	const float FP16Scale = 0.0009765625f;
	color *= exp2(g_postprocessCB.exposure) / FP16Scale;
	return float4(color, 1.0f);
}

#endif //_PS_POSTPROCESS_HLSL_