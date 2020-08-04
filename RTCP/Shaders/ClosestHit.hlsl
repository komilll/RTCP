#ifndef _CLOSEST_HIT_HLSL
#define _CLOSEST_HIT_HLSL

#include "Common.hlsl"

// ---[ Closest Hit Shader ]---

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in Attributes attrib)
{
	//float3 hitPosition = HitWorldPosition();
	//float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
	
	//uint indexSizeInBytes = 2;
	//uint indicesPerTriangle = 3;
	//uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	//uint baseIndex = PrimitiveIndex() * triangleIndexStride;
	
	//const uint3 indices_ = Load3x32BitIndices(baseIndex);

	//float3 vertexNormals[3] =
	//{
	//	vertices[indices_[0]].normal,
	//	vertices[indices_[1]].normal,
	//	vertices[indices_[2]].normal
	//};
	
	//float3 triangleNormal = vertexNormals[0] * barycentrics.x + vertexNormals[1] * barycentrics.y + vertexNormals[2] * barycentrics.z;
	
	//float4 diffuseColor = CalculateDiffuseLighting(hitPosition, triangleNormal);
	float4 color = float4(1, 0, 1, 1); //	g_sceneCB.lightAmbientColor; //+ diffuseColor;
	
	payload.color = color;
}

#endif // _CLOSEST_HIT_HLSL
