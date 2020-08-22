#ifndef _COMMON_HLSL
#define _COMMON_HLSL

#include "RT_HelperFunctions.hlsl"
#include "ALL_CommonBuffers.hlsl"

struct RayPayload
{
	float4 albedo;
	float4 normalWithDepth;
};

////

// Resources
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CameraConstantBuffer> g_cameraCB : register(b1);

RWTexture2D<float4> RTOutputNormal				  : register(u0);
RWTexture2D<float4> RTOutputAlbedo   			  : register(u1);

RaytracingAccelerationStructure SceneBVH      : register(t0);
ByteAddressBuffer indices					  : register(t1);
StructuredBuffer<Vertex> vertices			  : register(t2);
Texture2D<float4> albedoTex					  : register(t3);

SamplerState g_sampler : register(s0);
////

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attrib)
{
	return vertexAttribute[0] +
        attrib.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attrib.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

uint3 Load3x32BitIndices(uint offsetBytes)
{
	return indices.Load3(offsetBytes);
}

// Load three 16 bit indices from a byte addressed buffer.
uint3 Load3x16BitIndices(uint offsetBytes)
{
	uint3 indices_;
    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
	const uint dwordAlignedOffset = offsetBytes & ~3;
	const uint2 four16BitIndices = indices.Load2(dwordAlignedOffset);
 
    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
	if (dwordAlignedOffset == offsetBytes)
	{
		indices_.x = four16BitIndices.x & 0xffff;
		indices_.y = (four16BitIndices.x >> 16) & 0xffff;
		indices_.z = four16BitIndices.y & 0xffff;
	}
	else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
	{
		indices_.x = (four16BitIndices.x >> 16) & 0xffff;
		indices_.y = four16BitIndices.y & 0xffff;
		indices_.z = (four16BitIndices.y >> 16) & 0xffff;
	}

	return indices_;
}

// Get hit position in world-space
inline float3 HitWorldPosition()
{
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}


#endif // _COMMON_HLSL