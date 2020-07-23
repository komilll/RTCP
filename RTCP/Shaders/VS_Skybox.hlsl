struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 positionWS : TEXCOORD0;
};

cbuffer MatrixBuffer : register(b0)
{
	matrix g_worldMatrix;
	matrix g_viewMatrix;
	matrix g_projectionMatrix;
	matrix g_paddingMatrix;
};

struct VertexInputType
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD0;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;
	
	float4 worldPos = mul(float4(input.position.xyz, 1.0f), g_worldMatrix);
	output.positionWS = worldPos.xyz;

	output.position = mul(worldPos, g_viewMatrix);
	output.position = mul(output.position, g_projectionMatrix).xyww;

	return output;
}