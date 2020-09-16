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
	uint rndSeed;
	int pathLength;
};

struct GiConstantBuffer
{
	int useDirect;
	int useIndirect;
	int accFrames;
	float roughness;
	float metallic;
	int bounceCount;
	int maxFrames;
	float padding[57];
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

float3 shootIndirectRay(float3 rayOrigin, float3 rayDir, float minT, PayloadIndirect payload)
{
	// Setup shadow ray
	RayDesc rayColor = { rayOrigin, minT, rayDir, 1e+38f };
	
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 2, 1, rayColor, payload);

	return payload.color;
}
	
float shadowRayVisibility(float3 origin, float3 dir, float tMin, float tMax)
{
	RayDesc visRay = { origin, tMin, dir, tMax };
	RayPayload payload;
	payload.vis = 0.0f;
	TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 1, 0, visRay, payload);
	
	return payload.vis;
}

[shader("raygeneration")]
void RayGen()
{
	if (g_giCB.maxFrames > 0 && g_giCB.accFrames >= g_giCB.maxFrames)
	{
		return;
	}
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
		RTOutput[LaunchIndex] = albedo;
		return;
	}
	
	float3 shadeColor = 0;// g_giCB.useDirect == 1 ? albedo.rgb / PI : 0;
	float lightsCount = g_lightCB.lightPositionAndType[15].w;
	uint seed = initRand(LaunchIndex.x + LaunchIndex.y * LaunchDimensions.x, g_sceneCB.frameCount);
	
	PayloadIndirect indirectPayload;
	indirectPayload.color = float3(0, 0, 0);
	indirectPayload.rndSeed = seed;
	indirectPayload.pathLength = 0;
	
	if (lightsCount > 0)
	{
		//int lightToSample = min(int(nextRand(seed) * lightsCount), lightsCount - 1);
		
		//float3 lightColor = g_lightCB.lightDiffuseColor[lightToSample].rgb;
		//float3 toLight = g_lightCB.lightPositionAndType[lightToSample].xyz - pixelWorldSpacePosition;
		//float distToLight = length(toLight);
		//toLight = normalize(toLight);
		//float NoL = saturate(dot(normalAndDepth.xyz, toLight));
			
		//float shadowMult = shadowRayVisibility(pixelWorldSpacePosition, toLight, 1e-4f, distToLight);
		//if (g_giCB.useDirect == 1)
		//{
		//	shadeColor += shadowMult * NoL * albedo.rgb / PI;
		//}
		
		//if (g_giCB.useIndirect == 1)
		//{
		//	float3 bounceDir = getCosHemisphereSample(seed, normalAndDepth.xyz);
		//	float NoDir = saturate(dot(normalAndDepth.xyz, bounceDir));

		//	float3 bounceColor = shootIndirectRay(pixelWorldSpacePosition, bounceDir, 1e-4f, indirectPayload);
		//	float sampleProb = NoDir / PI;
		//	shadeColor += (NoDir * bounceColor * albedo.rgb / PI) / sampleProb;
		//}
	}
	float3 bounceDir = getCosHemisphereSample(seed, normalAndDepth.xyz);
	float3 color = shootIndirectRay(pixelWorldSpacePosition, bounceDir, 1e-4f, indirectPayload);
	
	float4 prevScene = RTOutput[LaunchIndex];
	float4 finalColor = float4(color, 1.0f);
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
	float3 rayDir = WorldRayDirection();
	rayDir.z = -rayDir.z;
	
	//payload.color += skyboxTexture.SampleLevel(g_sampler, rayDir, 0).rgb;
	//payload.pathLength++;
}

[shader("closesthit")]
void ClosestHitIndirect(inout PayloadIndirect payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	
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
	float4 albedo = albedoTexture.Load(int3(DispatchRaysIndex().xy, 0));
	
	//
	float lightsCount = g_lightCB.lightPositionAndType[15].w;
	for (int i = 0; i < lightsCount; i++)
	{
		//int lightToSample = min(int(nextRand(payload.rndSeed) * lightsCount), lightsCount - 1);
	
		float3 lightColor = g_lightCB.lightDiffuseColor[i].rgb;
		float3 toLight = g_lightCB.lightPositionAndType[i].xyz - hitPos;
		float distToLight = length(toLight);
		toLight = normalize(toLight);
		float NoL = saturate(dot(triangleNormal.xyz, toLight));
	
		float visibility = shadowRayVisibility(hitPos, toLight, 1e-4f, 1e+38f);

		//if (g_giCB.useDirect == 1)
		{
			//payload.color += visibility * NoL * lightColor * albedo.rgb / PI;
			payload.color += visibility * albedo.rgb * INV_PI;
		}
	}
		
	if (g_giCB.useIndirect == 1)
	{
		if (payload.pathLength < g_giCB.bounceCount)
		{
			PayloadIndirect newPayload;
			newPayload.pathLength = payload.pathLength + 1;
			newPayload.rndSeed = (uint) nextRand(payload.rndSeed);
			newPayload.color = float3(0, 0, 0);
			
			float3 bounceDir = getCosHemisphereSample(payload.rndSeed, triangleNormal);
			payload.color += shootIndirectRay(hitPos, bounceDir, 1e-4f, newPayload) * albedo.rgb;
		}
		else
		{
			//float visibility = shadowRayVisibility(hitPos, toLight, 1e-4f, 1e+38f);
			//float3 rayDir = WorldRayDirection();
			//rayDir.z = -rayDir.z;
	
			//payload.color += skyboxTexture.SampleLevel(g_sampler, rayDir, 0).rgb;
			//payload.pathLength++;
		}
	}
}

#endif //_RT_LAMBERTIAN_HLSL_