#ifndef _CLOSEST_HIT_HLSL
#define _CLOSEST_HIT_HLSL

#include "Common.hlsl"

// ---[ Closest Hit Shader ]---

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in Attributes attrib)
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
	
	float3 triangleNormal = HitAttribute(vertexNormals, attrib);
	
	//float4 diffuseColor = CalculateDiffuseLighting(hitPosition, triangleNormal);
	//float4 color = g_sceneCB.lightAmbientColor + diffuseColor;
	
	payload.worldPos = float4(hitPosition, 1);
	payload.normalWithDepth = float4(triangleNormal, RayTCurrent());
}

#endif // _CLOSEST_HIT_HLSL
