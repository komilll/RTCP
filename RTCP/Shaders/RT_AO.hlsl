#ifndef _RT_AO_HLSL_
#define _RT_AO_HLSL_

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
	float aoVal;
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

RWTexture2D<float4> RTOutput : register(u0);
Texture2D<float4> NormalTextureInput : register(u1);

RaytracingAccelerationStructure SceneBVH : register(t0);
ByteAddressBuffer indices : register(t1);
StructuredBuffer<Vertex> vertices : register(t2);
////

[shader("raygeneration")]
void RayGen()
{
	float3 rayDir, origin, world;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

	RayDesc ray = { g_sceneCB.cameraPosition.xyz, 0.0f, rayDir, 1e+38f };
	RayPayload payload = { float4(0, 0, 0, 0) };
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
	//RAY_FLAG_CULL_BACK_FACING_TRIANGLES
    // Write the raytraced color to the output texture.
	
	uint2 seed = randomTea(asuint(rayDir.xy * rayDir.z * DispatchRaysDimensions().xy) + g_cubeCB.frameCount * float2(3, 8), 8);
	float2 Xi = HammersleyDistribution(g_cubeCB.frameCount, 8, seed);
	float3 L = UniformSampleSphere(Xi.x, Xi.y);
	
	RTOutput[DispatchRaysIndex().xy] = payload.color;
}

#endif //_RT_AO_HLSL_