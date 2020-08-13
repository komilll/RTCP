#ifndef _COMMON_HLSL
#define _COMMON_HLSL

#include "RT_HelperFunctions.hlsl"

// Structures
struct Vertex
{
	float3 position;
	float3 normal;
	float3 tangent;
	float3 binormal;
	float2 uv;
};

struct RayPayload
{
	float4 worldPos;
	float4 normalWithDepth;
};

struct Attributes 
{
	float2 bary;
};
////

// Constant Buffers
struct SceneConstantBuffer
{
	matrix projectionToWorld;
	float4 cameraPosition;
	float4 lightPosition;
	float4 lightAmbientColor;
	float4 lightDiffuseColor;
};
////
struct CubeConstantBuffer
{
	float3 albedo;
	int frameCount;
};

// Resources
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CubeConstantBuffer> g_cubeCB : register(b1);

RWTexture2D<float4> RTOutputNormal				  : register(u0);
RWTexture2D<float4> RTOutputPosition			  : register(u1);

RaytracingAccelerationStructure SceneBVH      : register(t0);
ByteAddressBuffer indices					  : register(t1);
StructuredBuffer<Vertex> vertices			  : register(t2);
////

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], Attributes attrib)
{
	return vertexAttribute[0] +
        attrib.bary.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attrib.bary.y * (vertexAttribute[2] - vertexAttribute[0]);
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

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + float2(0.5f, 0.5f); // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

	world.xyz /= world.w;
	origin = g_sceneCB.cameraPosition.xyz;
	direction = normalize(world.xyz - origin);
}

// Get hit position in world-space
inline float3 HitWorldPosition()
{
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}


#endif // _COMMON_HLSL