#ifndef _CLOSEST_HIT_HLSL
#define _CLOSEST_HIT_HLSL

#include "Common.hlsl"

// ---[ Closest Hit Shader ]---

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in Attributes attrib)
{
	float3 hitPosition = HitWorldPosition();
	
	uint indexSizeInBytes = 2;
	uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;
	
	const uint3 indices_ = Load3x16BitIndices(baseIndex);

	float3 vertexNormals[3] =
	{
		vertices[indices_[0]].normal,
		vertices[indices_[1]].normal,
		vertices[indices_[2]].normal
	};
	
	float3 triangleNormal = HitAttribute(vertexNormals, attrib);
	
	float4 diffuseColor = CalculateDiffuseLighting(hitPosition, triangleNormal);
	float4 color = float4(triangleNormal, 1.0f); //float4(1, 0, 0, 1); // g_sceneCB.lightAmbientColor + diffuseColor;
	
	payload.color = color;
}

#endif // _CLOSEST_HIT_HLSL
