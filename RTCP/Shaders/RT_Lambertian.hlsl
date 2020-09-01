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
};

struct GiConstantBuffer
{
	int useIndirect;
	float padding[63];
};

////

// Resources
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CameraConstantBuffer> g_cameraCB : register(b1);
ConstantBuffer<GiConstantBuffer> g_giCB : register(b2);

RWTexture2D<float4> RTOutput : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);
ByteAddressBuffer indices : register(t1);
StructuredBuffer<Vertex> vertices : register(t2);
Texture2D<float4> NormalTextureInput : register(t3);
Texture2D<float4> albedoTexture : register(t4);
TextureCube<float4> skyboxTexture : register(t5);

SamplerState g_sampler : register(s0);
////

float3 shootIndirectRay(float3 rayOrigin, float3 rayDir, float minT, uint seed)
{
	// Setup shadow ray
	RayDesc rayColor = { rayOrigin, minT, rayDir, 1e+38f };;
	
	PayloadIndirect payload;
	payload.color = float3(0, 0, 0);
	payload.rndSeed = seed;

	TraceRay(SceneBVH, 0, 0xFF, 0, 1, 1, rayColor, payload);

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
	
	float3 shadeColor = albedo.xyz;
	// Lambert lighting
	{
		float3 lightColor = g_sceneCB.lightDiffuseColor.rgb;
		float3 toLight = g_sceneCB.lightPosition.xyz - pixelWorldSpacePosition;
		float distToLight = length(toLight);
		toLight = normalize(toLight);
		float NoL = saturate(dot(normalAndDepth.xyz, toLight));
	
		float visibility = shadowRayVisibility(pixelWorldSpacePosition, toLight, 1e-4f, distToLight);
		
		shadeColor += visibility * NoL * lightColor;
	}
	shadeColor *= albedo.rgb / PI;
	
	//if (g_giCB.useIndirect)
	//{
	//	// Select a random direction for our diffuse interreflection ray.
	//	float3 bounceDir;
	//	uint seed = initRand(LaunchIndex.x + LaunchIndex.y * LaunchDimensions.x, g_sceneCB.frameCount);
		
	////	//if (gCosSampling)
	//	bounceDir = getCosHemisphereSample(seed, normalAndDepth.xyz); // Use cosine sampling
	////	//else
	////	//	bounceDir = getUniformHemisphereSample(randSeed, worldNorm.xyz);  // Use uniform random samples

	////	// Get NdotL for our selected ray direction
	//	float NdotL = saturate(dot(normalAndDepth.xyz, bounceDir));

	////	// Shoot our indirect global illumination ray
	//	float3 bounceColor = shootIndirectRay(pixelWorldSpacePosition.xyz, bounceDir, 1e-4f, seed);

	////	//bounceColor = (NdotL > 0.50f) ? float3(0, 0, 0) : bounceColor;

	////	// Probability of selecting this ray ( cos/pi for cosine sampling, 1/2pi for uniform sampling )
	//	float sampleProb = (NdotL / PI);

	////	// Accumulate the color.  For performance, terms could (and should) be cancelled here.
	////	//shadeColor += (NdotL * bounceColor * difMatlColor.rgb / M_PI) / sampleProb;

	//	shadeColor += (NdotL * bounceColor * albedo.rgb / PI) / sampleProb;
	//	shadeColor = bounceColor;
	//}
	
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