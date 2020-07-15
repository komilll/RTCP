#ifndef _PIXEL_SHADER_HLSL
#define _PIXEL_SHADER_HLSL

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 color : COLOR;
};

float4 main(PixelInputType input) : SV_TARGET
{
	return float4(input.color, 1.0f);
}

#endif //_PIXEL_SHADER_HLSL