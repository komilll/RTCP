#ifndef _RT_AO_HLSL_
#define _RT_AO_HLSL_

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
	float T;
};

////

struct RaytracingAOBuffer
{
	float aoRadius;
	float minT;
	int accFrames;

	float padding[61];
};

// Resources
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CameraConstantBuffer> g_cameraCB : register(b1);
ConstantBuffer<RaytracingAOBuffer> g_aoCB : register(b2);

RWTexture2D<float4> RTOutput : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);
ByteAddressBuffer indices : register(t1);
StructuredBuffer<Vertex> vertices : register(t2);
Texture2D<float4> NormalTextureInput : register(t3);
Texture2D<float4> albedoTexture : register(t4);

////

inline void GenerateCameraRayAO(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + g_cameraCB.jitterCamera; // center in the middle of the pixel.
	float2 screenPos = (xy / DispatchRaysDimensions().xy) * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
	//screenPos.y = -screenPos.y;
	screenPos *= float2(0.5 * 16.0f / 9.0f, 0.5);

    // Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

	world.xyz /= world.w;
	direction = screenPos.x * g_cameraCB.cameraU.xyz + screenPos.y * g_cameraCB.cameraV.xyz + g_cameraCB.cameraW.xyz;
	direction /= length(g_cameraCB.cameraW.xyz);
	
	// Calculate random ray origin
	float3 focalPoint = g_sceneCB.cameraPosition.xyz + g_cameraCB.focalLength * direction;
	
	uint randSeed = initRand(index.x + index.y * DispatchRaysDimensions().x, g_sceneCB.frameCount);
	float2 rnd = float2(nextRand(randSeed) * PI * 2.0, nextRand(randSeed) * g_cameraCB.lensRadius);
	float2 uv = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);
	
	origin = g_sceneCB.cameraPosition.xyz + uv.x * normalize(g_cameraCB.cameraU.xyz) + uv.y * normalize(g_cameraCB.cameraV.xyz);
}

[shader("raygeneration")]
void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

	float4 normalAndDepth = NormalTextureInput.Load(int3(LaunchIndex, 0));
	
	if (normalAndDepth.w == 0)
	{
        // Terminate if primary ray didn't hit anything
		RTOutput[LaunchIndex] = float4(1.0f, 1.0f, 1.0f, 1.0f);
		return;
	}
	
	// Figure out pixel world space position (using length of a primary ray found in previous pass)
	float3 primaryRayOrigin = g_sceneCB.cameraPosition.xyz;
	float3 primaryRayDirection;
	GenerateCameraRay(LaunchIndex, LaunchDimensions, g_sceneCB.projectionToWorld, primaryRayOrigin, primaryRayDirection, g_sceneCB, g_cameraCB);
	
	// Calculate direction of raytracing for AO sample
	float3 pixelWorldSpacePosition = primaryRayOrigin + (primaryRayDirection * normalAndDepth.w);
	uint seed = initRand(LaunchIndex.x + LaunchIndex.y * LaunchDimensions.x, g_sceneCB.frameCount);
	float3 worldDir = getCosHemisphereSample(seed.x, normalAndDepth.xyz);
	
	// Prepare ray description and payload
	float ao = 0.0f;
	RayDesc aoRay = { pixelWorldSpacePosition, g_aoCB.minT, worldDir, g_aoCB.aoRadius };
	RayPayload payload;
	payload.T = g_aoCB.aoRadius; //< Set T to "maximum", to produce no occlusion in case ray doesn't hit anything (miss shader won't modify this value)

	// Trace the ray
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, aoRay, payload);
	ao = payload.T / g_aoCB.aoRadius;
	
	// Accumulate AO with previous frames
	float prevAO = RTOutput[LaunchIndex].x;
	ao = ((float) g_aoCB.accFrames * prevAO + ao) / ((float) g_aoCB.accFrames + 1.0f);
	RTOutput[LaunchIndex.xy] = float4(ao, ao, ao, 1.0f); //< Replace all cached AO with current result
	
	//// Lambert lighting
	//float3 lightColor = g_sceneCB.lightDiffuseColor.rgb;
	//float3 toLight = g_sceneCB.lightPosition.xyz - pixelWorldSpacePosition;
	//float distToLight = length(toLight);
	//toLight = normalize(toLight);

	//float NoL = saturate(dot(payload.normal, toLight));
	//float visibility = 
}

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
	// Empty
}

uint3 Load3x32BitIndices(uint offsetBytes)
{
	return indices.Load3(offsetBytes);
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attrib)
{
	return vertexAttribute[0] +
        attrib.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attrib.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

float2 HitAttribute(float2 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attrib)
{
	return vertexAttribute[0] +
        attrib.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attrib.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	//float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
	payload.T = RayTCurrent();
	
	//uint indexSizeInBytes = 4;
	//uint indicesPerTriangle = 3;
	//uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	//uint baseIndex = PrimitiveIndex() * triangleIndexStride;
	
	//const uint3 indices_ = Load3x32BitIndices(baseIndex);
	
	//float2 vertexUVs[3] =
	//{
	//	vertices[indices_[0]].uv,
	//	vertices[indices_[1]].uv,
	//	vertices[indices_[2]].uv
	//};
	
	//float2 uv = HitAttribute(vertexUVs, attribs);
	//uv = barycentrics.x * vertexUVs[0] + barycentrics.y * vertexUVs[1] + barycentrics.z * vertexUVs[2];
	
	//payload.albedo = albedoTexture.SampleLevel(g_sampler, uv, 0).rgb;
}

#endif //_RT_AO_HLSL_