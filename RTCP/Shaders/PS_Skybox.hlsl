//#include <All_UberBuffer.hlsl>

cbuffer UberBuffer : register(b0)
{
	float3 g_viewerPosition;
	float g_uberPad[61];
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 positionWS : TEXCOORD0;
};

SamplerState baseSampler : register(s0);
TextureCube<float4> skyboxTexture : register(t0);

float4 main(PixelInputType input) : SV_TARGET
{
	//return float4(g_viewerPosition, 1.0f);
    //return float4(normalize(g_viewerPosition.xyz - input.positionWS) * float3(1, 1, -1), 1.0f);
	return skyboxTexture.Sample(baseSampler, normalize(input.positionWS - g_viewerPosition.xyz) * float3(1, 1, -1));
}