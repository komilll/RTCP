#ifndef _RT_LAMBERTIAN_HLSL_
#define _RT_LAMBERTIAN_HLSL_

#include "RT_HelperFunctions.hlsl"
#include "UniformDistribution.hlsl"
#include "SamplingHelper.hlsl"
#include "ALL_CommonBuffers.hlsl"

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
	float vis;
};

////

// Resources
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CameraConstantBuffer> g_cameraCB : register(b1);

RWTexture2D<float4> RTOutput : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);
ByteAddressBuffer indices : register(t1);
StructuredBuffer<Vertex> vertices : register(t2);
Texture2D<float4> NormalTextureInput : register(t3);
Texture2D<float4> albedoTexture : register(t4);
////

[shader("raygeneration")]
void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

	float4 albedo = albedoTexture.Load(int3(LaunchIndex, 0));
	float4 normalAndDepth = NormalTextureInput.Load(int3(LaunchIndex, 0));
	// Figure out pixel world space position (using length of a primary ray found in previous pass)
	float3 primaryRayOrigin = g_sceneCB.cameraPosition.xyz;
	float3 primaryRayDirection;
	GenerateCameraRay(LaunchIndex, LaunchDimensions, g_sceneCB.projectionToWorld, primaryRayOrigin, primaryRayDirection);
	
	float3 pixelWorldSpacePosition = primaryRayOrigin + (primaryRayDirection * normalAndDepth.w);
	if (normalAndDepth.w == 0)
	{
		RTOutput[LaunchIndex] = float4(0, 0, 0, 1.0f);
		return;
	}
	
	float3 shadeColor = albedo.xyz;
	// Lambert lighting
	{
		float3 lightColor = g_sceneCB.lightDiffuseColor.rgb;
		float3 toLight = g_sceneCB.lightPosition.xyz - pixelWorldSpacePosition;
		float distToLight = length(toLight);
		toLight = normalize(toLight);
		float NoL = saturate(dot(normalAndDepth.xyz, toLight));
	
		RayDesc visRay = { pixelWorldSpacePosition, 1e-4f, toLight, distToLight };
		RayPayload payload;
		payload.vis = 0.0f;
		TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 1, 0, visRay, payload);
	
		float visibility = payload.vis;
		
		shadeColor += visibility * NoL * lightColor;
	}
	shadeColor *= albedo.rgb / PI;
	RTOutput[LaunchIndex] = float4(shadeColor, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
	payload.vis = 1.0f;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	
}

#endif //_RT_LAMBERTIAN_HLSL_