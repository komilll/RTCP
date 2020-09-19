#ifndef _RT_LAMBERTIAN_HLSL_
#define _RT_LAMBERTIAN_HLSL_

#include "RT_HelperFunctions.hlsl"
#include "UniformDistribution.hlsl"
#include "SamplingHelper.hlsl"
#include "ALL_CommonBuffers.hlsl"

struct RayPayload
{
	float vis;
};

struct PayloadIndirect
{
	float3 color;
	uint rndSeed; // pixelIdx
	uint rndSeed2; // sampleSetIdx
	int pathLength;
};

struct LightConstantBuffer
{
	float4 lightDiffuseColor[16];
	float4 lightPositionAndType[16];
};

////

// Resources
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CameraConstantBuffer> g_cameraCB : register(b1);
ConstantBuffer<GiConstantBuffer> g_giCB : register(b2);
ConstantBuffer<LightConstantBuffer> g_lightCB : register(b3);

RWTexture2D<float4> RTOutput : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);
ByteAddressBuffer indices : register(t1);
StructuredBuffer<Vertex> vertices : register(t2);
Texture2D<float4> NormalTextureInput : register(t3);
Texture2D<float4> albedoTexture : register(t4);
Texture2D<float4> specularTexture : register(t5);
TextureCube<float4> skyboxTexture : register(t6);

SamplerState g_sampler : register(s0);
////

static float2 SamplePoint(in uint pixelIdx, inout uint setIdx)
{
	const uint permutation = setIdx * (DispatchRaysDimensions().x * DispatchRaysDimensions().y) + pixelIdx;
	setIdx += 1;
	return SampleCMJ2D(g_giCB.accFrames, g_giCB.sqrtMaxFrames, g_giCB.sqrtMaxFrames, permutation);
}

void loadHitData(out float3 normal, out float3 tangent, out float3 bitangent, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint indexSizeInBytes = 4;
	uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;
	
	const uint3 indices_ = indices.Load3(baseIndex);
	float3 vertexNormals[3] = { vertices[indices_[0]].normal, vertices[indices_[1]].normal, vertices[indices_[2]].normal };
	float3 vertexTangents[3] = { vertices[indices_[0]].tangent, vertices[indices_[1]].tangent, vertices[indices_[2]].tangent };
	float3 vertexBitangents[3] = { vertices[indices_[0]].binormal, vertices[indices_[1]].binormal, vertices[indices_[2]].binormal };
	
	normal = HitAttribute(vertexNormals, attribs);
	tangent = HitAttribute(vertexTangents, attribs);
	bitangent = HitAttribute(vertexBitangents, attribs);
}

float3 shootIndirectRay(float3 rayOrigin, float3 rayDir, float minT, PayloadIndirect payload)
{
	RayDesc rayColor = { rayOrigin, minT, rayDir, 1e+38f };
	TraceRay(SceneBVH, RAY_FLAG_FORCE_OPAQUE, 0xFF, 1, 2, 1, rayColor, payload);
	return payload.color;
}
	
float shadowRayVisibility(float3 origin, float3 dir, float tMin, float tMax)
{
	RayDesc visRay = { origin, tMin, dir, tMax };
	RayPayload payload;
	payload.vis = 0.0f;
	TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 2, 0, visRay, payload);
	
	return payload.vis;
}

#define SAMPLE_UNIFORM 0
#define SAMPLE_MJ 1
#define SAMPLE_RANDOM 2

[shader("raygeneration")]
void RayGen()
{
	// Accumulate for limited amount of frames
	if (g_giCB.maxFrames > 0 && g_giCB.accFrames >= g_giCB.maxFrames)
	{
		return;
	}
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;
	float4 normalAndDepth = NormalTextureInput.Load(int3(LaunchIndex, 0));

	// No geometry hit - skip pixel and use skybox data
	if (normalAndDepth.w == 0)
	{
		RTOutput[LaunchIndex] = albedoTexture.Load(int3(LaunchIndex, 0));
		return;
	}
	
	// Calculate primary ray direction
	uint seed = initRand(LaunchIndex.x + LaunchIndex.y * LaunchDimensions.x, g_sceneCB.frameCount, 16);
	uint seed2 = 0;
	float2 offset = float2(0, 0);
	if (g_giCB.samplingType == SAMPLE_UNIFORM)
	{
		seed2 = initRand(LaunchIndex.x + LaunchIndex.y * LaunchDimensions.x, g_sceneCB.frameCount, 17);
		offset = HammersleyDistribution(g_giCB.accFrames, g_giCB.maxFrames, uint2(seed, seed2));
	}
	else if (g_giCB.samplingType == SAMPLE_MJ)
	{
		const uint pixelIdx = LaunchIndex.y * LaunchDimensions.x + LaunchIndex.x;
		uint sampleSetIdx = 0;
		offset = SamplePoint(pixelIdx, sampleSetIdx);
		seed = pixelIdx;
		seed2 = sampleSetIdx;
	}
	
	float3 primaryRayOrigin = g_sceneCB.cameraPosition.xyz;
	float3 primaryRayDirection;
	GenerateCameraRay(LaunchIndex, LaunchDimensions, g_sceneCB.projectionToWorld, primaryRayOrigin, primaryRayDirection, offset);
	
	// Prepare payload
	PayloadIndirect indirectPayload;
	indirectPayload.color = float3(0, 0, 0);
	indirectPayload.rndSeed = seed;
	indirectPayload.rndSeed2 = seed2;
	indirectPayload.pathLength = 0;
		
	// Calculate pixel color in current pass and merge with previous frames
	float4 finalColor = float4(shootIndirectRay(primaryRayOrigin, primaryRayDirection, 1e-3f, indirectPayload), 1.0f);
	float4 prevScene = RTOutput[LaunchIndex];
	finalColor = ((float) g_giCB.accFrames * prevScene + finalColor) / ((float) g_giCB.accFrames + 1.0f);
	RTOutput[LaunchIndex] = finalColor;
}

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
	payload.vis = 1.0f;
}

[shader("closesthit")]
void ClosestHit(inout PayloadIndirect payload, in BuiltInTriangleIntersectionAttributes attribs)
{

}

[shader("miss")]
void MissIndirect(inout PayloadIndirect payload : SV_RayPayload)
{
	// Use skybox as contribution if ray failed to hit geometry (right now, disabled for debug purposes)
	float3 rayDir = WorldRayDirection();
	rayDir.z = -rayDir.z;
	if (g_giCB.useSkybox)
	{
		payload.color += skyboxTexture.SampleLevel(g_sampler, rayDir, 0).rgb;
	}
}

[shader("closesthit")]
void ClosestHitIndirect(inout PayloadIndirect payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	// Load hit data
	float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	float3 triangleNormal, triangleTangent, triangleBitangent;
	loadHitData(triangleNormal, triangleTangent, triangleBitangent, attribs);

	// Use white albedo for all textures (DEBUG version)
	float4 albedo = albedoTexture.Load(int3(DispatchRaysIndex().xy, 0));
	albedo = float4(1, 1, 1, 1);
	
	// Iterate over all lights
	float lightsCount = g_lightCB.lightPositionAndType[15].w;
	for (int i = 0; i < lightsCount; i++)
	{
		// Calculate each light data
		float3 lightColor = g_lightCB.lightDiffuseColor[i].rgb;
		float3 toLight = g_lightCB.lightPositionAndType[i].xyz - hitPos;
		float distToLight = length(toLight);
		toLight = normalize(toLight);
		
		// Check visibility
		float NoL = saturate(dot(triangleNormal.xyz, toLight));
		float visibility = shadowRayVisibility(hitPos, toLight, 1e-3f, distToLight);

		// Calculate light contribution to point in world (diffuse lambertian term)
		payload.color += visibility * NoL * albedo.rgb * INV_PI;
	}
	
	if (g_giCB.useIndirect == 1)
	{
		// Continue spawning rays if path left has not reached maximum
		if (payload.pathLength < g_giCB.bounceCount)
		{
			// Find next direction
			float3 rayDirWS = float3(0, 0, 0);
			if (g_giCB.samplingType == SAMPLE_UNIFORM)
			{
				float3x3 tangentToWorld = float3x3(triangleTangent, triangleBitangent, triangleNormal);
				float2 hammersley = HammersleyDistribution(payload.pathLength, g_giCB.bounceCount, uint2(payload.rndSeed, payload.rndSeed2));
				float3 rayDirTS = UniformSampleHemisphere(hammersley.x, hammersley.y);
				rayDirWS = normalize(mul(rayDirTS, tangentToWorld));
			}
			else if (g_giCB.samplingType == SAMPLE_MJ)
			{
				float3x3 tangentToWorld = float3x3(triangleTangent, triangleBitangent, triangleNormal);
				float2 brdfSample = SamplePoint(payload.rndSeed, payload.rndSeed2);
				float3 rayDirTS = SampleDirectionCosineHemisphere(brdfSample.x, brdfSample.y);
				rayDirWS = normalize(mul(rayDirTS, tangentToWorld));
			}
			else if (g_giCB.samplingType == SAMPLE_RANDOM)
			{
				rayDirWS = getCosHemisphereSample(payload.rndSeed, triangleNormal, triangleTangent, triangleBitangent);
				nextRand(payload.rndSeed);
			}
			
			// Prepare payload
			PayloadIndirect newPayload;
			newPayload.pathLength = payload.pathLength + 1;
			newPayload.rndSeed = payload.rndSeed;
			newPayload.rndSeed2 = payload.rndSeed2;
			newPayload.color = float3(0, 0, 0);
			
			// Calculate next ray bounce color contribution
			float3 bounceColor = shootIndirectRay(hitPos, rayDirWS, 1e-3f, newPayload);
			payload.color += bounceColor * albedo.rgb;
		}
	}
}

#endif //_RT_LAMBERTIAN_HLSL_