#ifndef _RT_G_BUFFER_HLSL_
#define _RT_G_BUFFER_HLSL_

#include "RT_HelperFunctions.hlsl"
#include "UniformDistribution.hlsl"
#include "SamplingHelper.hlsl"
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

RWTexture2D<float4> RTOutputNormal : register(u0);
RWTexture2D<float4> RTOutputAlbedo : register(u1);

RaytracingAccelerationStructure SceneBVH : register(t0);
ByteAddressBuffer indices : register(t1);
StructuredBuffer<Vertex> vertices : register(t2);
Texture2DArray<float4> albedoTex : register(t3);
TextureCube<float4> skyboxTexture : register(t4);

SamplerState g_sampler : register(s0);

[shader("raygeneration")]
void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;
	float3 rayDir;
	float3 origin = g_sceneCB.cameraPosition.xyz;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	//GenerateCameraRay(LaunchIndex, LaunchDimensions, g_sceneCB.projectionToWorld, origin, rayDir, g_sceneCB, g_cameraCB);
	GenerateCameraRay(LaunchIndex, LaunchDimensions, g_sceneCB.projectionToWorld, origin, rayDir);

	RayDesc ray = { origin, 1e-4f, rayDir, 1e+38f };
	RayPayload payload;
	payload.albedo = float4(0, 0, 0, 0);
	payload.normalWithDepth = float4(0, 0, 0, 0);
	
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
	//RAY_FLAG_CULL_BACK_FACING_TRIANGLES
	
	RTOutputNormal[DispatchRaysIndex().xy] = payload.normalWithDepth;
	RTOutputAlbedo[DispatchRaysIndex().xy] = payload.albedo;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint indexSizeInBytes = 4;
	uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;
	
	const uint3 indices_ = indices.Load3(baseIndex);

	float3 vertexNormals[3] =
	{
		vertices[indices_[0]].normal,
		vertices[indices_[1]].normal,
		vertices[indices_[2]].normal
	};
	
	float3 triangleNormal = HitAttribute(vertexNormals, attribs);
	
	payload.normalWithDepth = float4(triangleNormal, RayTCurrent());
	
	float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
	float2 vertexUVs[3] =
	{
		vertices[indices_[0]].uv,
		vertices[indices_[1]].uv,
		vertices[indices_[2]].uv
	};
	
	float2 uv = barycentrics.x * vertexUVs[0] + barycentrics.y * vertexUVs[1] + barycentrics.z * vertexUVs[2];
	payload.albedo = float4(albedoTex.SampleLevel(g_sampler, float3(uv, vertices[indices_[0]].textureID), 0).xyz, 1);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
	float3 rayDir = WorldRayDirection();
	rayDir.z = -rayDir.z;
	
	payload.albedo = float4(skyboxTexture.SampleLevel(g_sampler, rayDir, 0).rgb, 0);
	payload.normalWithDepth = float4(0, 0, 0, 0);
}

#endif //_RT_G_BUFFER_HLSL_