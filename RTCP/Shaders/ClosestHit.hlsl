#ifndef _CLOSEST_HIT_HLSL
#define _CLOSEST_HIT_HLSL

#include "Common.hlsl"

// ---[ Closest Hit Shader ]---

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	float3 hitPosition = HitWorldPosition();
	
	uint indexSizeInBytes = 4;
	uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;
	
	const uint3 indices_ = Load3x32BitIndices(baseIndex);

	float3 vertexNormals[3] =
	{
		vertices[indices_[0]].normal,
		vertices[indices_[1]].normal,
		vertices[indices_[2]].normal
	};
	
	float3 triangleNormal = HitAttribute(vertexNormals, attribs);
	
	//float4 diffuseColor = CalculateDiffuseLighting(hitPosition, triangleNormal);
	//float4 color = g_sceneCB.lightAmbientColor + diffuseColor;
	
	payload.normalWithDepth = float4(triangleNormal, RayTCurrent());
	
	float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
	float2 vertexUVs[3] =
	{
		vertices[indices_[0]].uv,
		vertices[indices_[1]].uv,
		vertices[indices_[2]].uv
	};
	
	float2 uv = barycentrics.x * vertexUVs[0] + barycentrics.y * vertexUVs[1] + barycentrics.z * vertexUVs[2];
	payload.albedo = float4(albedoTex.SampleLevel(g_sampler, uv, 0).xyz, 1);
}

#endif // _CLOSEST_HIT_HLSL
