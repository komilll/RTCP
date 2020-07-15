#ifndef _VERTEX_SHADER_HLSL
#define _VERTEX_SHADER_HLSL

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 color : COLOR;
};

struct VertexInputType
{
	float3 position : POSITION;
	float3 color : COLOR;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;
	
	output.position = float4(input.position, 1.0f);
	output.color = input.color;
	
	return output;
}

#endif //_VERTEX_SHADER_HLSL