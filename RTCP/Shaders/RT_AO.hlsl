#ifndef _RT_AO_HLSL_
#define _RT_AO_HLSL_

#include "RT_HelperFunctions.hlsl"
#include "UniformDistribution.hlsl"

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

struct RaytracingAOBuffer
{
	float aoRadius;
	float minT;
	float2 padding;
};

// Resources
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CubeConstantBuffer> g_cubeCB : register(b1);
ConstantBuffer<RaytracingAOBuffer> g_aoCB : register(b2);

RWTexture2D<float4> RTOutput : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);
ByteAddressBuffer indices : register(t1);
StructuredBuffer<Vertex> vertices : register(t2);
Texture2D NormalTextureInput : register(t3);
Texture2D PositionTextureInput : register(t4);

SamplerState g_sampler : register(s0);
////

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

float shootAmbientOcclusionRay(float3 origin, float3 dir, float minT, float maxT)
{
	//RayPayload rayPayload = { 0.0f };
	//RayDesc rayAO = { origin, minT, dir, maxT };

 // // We're going to tell our ray to never run the closest-hit shader and to
 // //     stop as soon as we find *any* intersection
	//uint rayFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
 //                 RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;

	//TraceRay(SceneBVH, rayFlags, 0xFF, 0, 1, 0, rayAO, rayPayload);
	//return rayPayload.aoVal;
}

// NVIDIA function - https://github.com/NVIDIAGameWorks/GettingStartedWithRTXRayTracing/blob/master/11-OneShadowRayPerPixel/Data/Tutorial11/diffusePlus1ShadowUtils.hlsli
uint initRand(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

// NVIDIA function - https://github.com/NVIDIAGameWorks/GettingStartedWithRTXRayTracing/blob/master/11-OneShadowRayPerPixel/Data/Tutorial11/diffusePlus1ShadowUtils.hlsli
float nextRand(inout uint s)
{
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

// NVIDIA function - https://github.com/NVIDIAGameWorks/GettingStartedWithRTXRayTracing/blob/master/11-OneShadowRayPerPixel/Data/Tutorial11/diffusePlus1ShadowUtils.hlsli
float3 getPerpendicularVector(float3 u)
{
	float3 a = abs(u);
	uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
	uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(u, float3(xm, ym, zm));
}

// NVIDIA function - https://github.com/NVIDIAGameWorks/GettingStartedWithRTXRayTracing/blob/master/11-OneShadowRayPerPixel/Data/Tutorial11/diffusePlus1ShadowUtils.hlsli
float3 getCosHemisphereSample(inout uint randSeed, float3 hitNorm)
{
	// Get 2 random numbers to select our sample with
	float2 randVal = float2(nextRand(randSeed), nextRand(randSeed));

	// Cosine weighted hemisphere sample from RNG
	float3 bitangent = getPerpendicularVector(hitNorm);
	float3 tangent = cross(bitangent, hitNorm);
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
	return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(1 - randVal.x);
}

static float3 aoSamples[8] =
{
	float3(-0.573172f, -0.532465f, 0.62286f),
	float3(0.587828f, 0.380267f, 0.714042f),
	float3(-0.857373f, -0.448367f, 0.252742f),
	float3(0.322927f, -0.208062f, 0.92327f),
	float3(-0.770474f, 0.378665f, 0.512818f),
	float3(0.367633f, -0.868896f, 0.33146f),
	float3(-0.073854f, 0.628146f, 0.774583f),
	float3(-0.383834f, -0.922466f, 0.0415685f),
};

[shader("raygeneration")]
void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

	// Figure out pixel world space position (using length of a primary ray found in previous pass)
	float3 primaryRayOrigin, primaryRayDirection; 
	GenerateCameraRay(LaunchIndex, primaryRayOrigin, primaryRayDirection);
	
	float2 samplePos = (float2) LaunchIndex / (float2) LaunchDimensions + float2(0.5f / (float) LaunchDimensions.x, 0.5f / (float) LaunchDimensions.y);
	float4 normalAndDepth = NormalTextureInput.SampleLevel(g_sampler, samplePos, 0);
	float4 worldPosition = PositionTextureInput.SampleLevel(g_sampler, samplePos, 0);
	float3 pixelWorldSpacePosition = primaryRayOrigin + (primaryRayDirection * normalAndDepth.w);

	if (normalAndDepth.w == 0.0f)
	{
        // Terminate if primary ray didn't hit anything
		RTOutput[LaunchIndex] = float4(0.0f, 0.2f, 0.4f, 1.0f);
		return;
	}
	
	float3 n = normalize(normalAndDepth.xyz);
	float3 rvec = primaryRayDirection;
	float3 b1 = normalize(rvec - n * dot(rvec, n));
	float3 b2 = cross(n, b1);
	float3x3 tbn = float3x3(b1, b2, n);
	
	uint seed = initRand(LaunchIndex.x + LaunchIndex.y * LaunchDimensions.x, g_cubeCB.frameCount);
	float3 worldDir = getCosHemisphereSample(seed, normalAndDepth.xyz);
	
	RayDesc aoRay;
	RayPayload payload;
	float3 aoSampleDirection = worldDir; //mul(worldDir, tbn);
	float ao = 0.0f;

	aoRay.Origin = pixelWorldSpacePosition;
	aoRay.TMin = g_aoCB.minT;
	aoRay.TMax = g_aoCB.aoRadius; //< Set max ray length to AO radius for early termination
	aoRay.Direction = aoSampleDirection;
	
	payload.T = g_aoCB.aoRadius; //< Set T to "maximum", to produce no occlusion in case ray doesn't hit anything (miss shader won't modify this value)

	// Trace the ray
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, aoRay, payload);
	ao = payload.T / g_aoCB.aoRadius;
	
	RTOutput[LaunchIndex.xy] = float4(ao, ao, ao, ao); //< Replace all cached AO with current result
	
	// Calculate AO
	//uint seed = initRand(LaunchIndex.x + LaunchIndex.y * LaunchDimensions.x, g_cubeCB.frameCount);
	//float3 worldDir = getCosHemisphereSample(seed, normalAndDepth.xyz);
	
	//float aoVal = shootAmbientOcclusionRay(pixelWorldSpacePosition, worldDir, g_aoCB.minT, g_aoCB.aoRadius);
	//RTOutput[DispatchRaysIndex().xy] = float4(aoVal, aoVal, aoVal, 1.0f);
	

	// Pick subset of samples to use based on "frame number % 4" and position on screen within a block of 3x3 pixels
	//int pixelIdx = dot(int2(fmod(float2(LaunchIndex), 3)), int2(1, 3));
	//int currentSamplesStartIndex = sampleStartIndex + (pixelIdx * samplesCount) + int(frameNumber * samplesCount * 9);

	//// Construct TBN matrix to orient sampling hemisphere along the surface normal
	//float3 n = normalize(normalAndDepth.xyz);
   
	//float3 rvec = primaryRayDirection;
	//float3 b1 = normalize(rvec - n * dot(rvec, n));
	//float3 b2 = cross(n, b1);
	//float3x3 tbn = float3x3(b1, b2, n);

	//RayDesc aoRay;
	//HitInfo aoHitData;

	//aoRay.Origin = pixelWorldSpacePosition;
	//aoRay.TMin = 0.1f;
	//aoRay.TMax = aoRadius; //< Set max ray length to AO radius for early termination

	//float ao = 0.0f;

	//[unroll(4)]
	//for (int i = 0; i < samplesCount; i++)
	//{
	//	float3 aoSampleDirection = mul(aoSamplesTexture.Load(int2(currentSamplesStartIndex + i, 0)).rgb, tbn);

	//	// Setup the ray
	//	aoRay.Direction = aoSampleDirection;
	//	aoHitData.T = aoRadius; //< Set T to "maximum", to produce no occlusion in case ray doesn't hit anything (miss shader won't modify this value)

	//	// Trace the ray
	//	TraceRay(
	//		SceneBVH,
	//		RAY_FLAG_NONE,
	//		0xFF,
	//		0,
	//		0,
	//		0,
	//		aoRay,
	//		aoHitData);

	//	ao += aoHitData.T / aoRadius;
	//}

	//ao /= float(samplesCount);

	//// Reverse reprojection
	//float4 previousPixelViewSpacePosition = mul(previousViewMatrix, float4(pixelWorldSpacePosition, 1.0f));
	//float previousReprojectedLinearDepth = length(previousPixelViewSpacePosition.xyz);
	//float4 previousPixelScreenSpacePosition = mul(motionVectorsMatrix, float4(pixelWorldSpacePosition, 1.0f));
	//previousPixelScreenSpacePosition.xyz /= previousPixelScreenSpacePosition.w;

	//float2 previousUvs = (previousPixelScreenSpacePosition.xy * float2(0.5f, -0.5f)) + 0.5f;

	//bool isReprojectionValid = true;

	//// Discard invalid reprojection (outside of the frame)
	//if (previousUvs.x > 1.0f || previousUvs.x < 0.0f ||
 //       previousUvs.y > 1.0f || previousUvs.y < 0.0f)
	//	isReprojectionValid = false;

	//// Discard invalid reprojection (depth mismatch)
	//const float maxReprojectionDepthDifference = 0.03f;
	//float previousSampledLinearDepth = normalAndDepthsPrevious.SampleLevel(linearClampSampler, previousUvs.xy, 0).a;

	//if (abs(1.0f - (previousReprojectedLinearDepth / previousSampledLinearDepth)) > maxReprojectionDepthDifference)
	//	isReprojectionValid = false;

 //   // Store AO to history cache
	//if (isReprojectionValid) 
	//	aoOutput[LaunchIndex.xy] = float4(ao, aoPrevious.SampleLevel(linearClampSampler, previousUvs.xy, 0).rgb); //< Store current AO in history cache
	//else
	//	aoOutput[LaunchIndex.xy] = float4(ao, ao, ao, ao); //< Replace all cached AO with current result
	
	
	
	//
	
	//uint2 pixIdx = DispatchRaysIndex().xy;
	//uint2 numPix = DispatchRaysDimensions().xy;
	
	//// Center position of sampled pixel in textures
	//float2 samplePos = (float2) pixIdx / (float2) numPix + float2(0.5f / (float) numPix.x, 0.5f / (float) numPix.y);
	
	//float3 worldNormal = NormalTextureInput.SampleLevel(g_sampler, samplePos, 0).rgb;
	//float4 worldPosition = PositionTextureInput.SampleLevel(g_sampler, samplePos, 0);
	
	//float aoVal = 1.0f;
	
	//if (worldPosition.w != 0.0f)
	//{
	//	uint seed = initRand(pixIdx.x + pixIdx.y * numPix.x, g_cubeCB.frameCount);
	//	float3 worldDir = getCosHemisphereSample(seed, worldNormal);
	
	//	aoVal = shootAmbientOcclusionRay(worldPosition.xyz, worldDir, g_aoCB.minT, g_aoCB.aoRadius);
	//	RTOutput[DispatchRaysIndex().xy] = float4(aoVal, aoVal, aoVal, 1.0f);
	//}
	//else
	//{
	//	RTOutput[DispatchRaysIndex().xy] = float4(worldNormal.xyz, 1.0f);
	//}
	
	//RTOutput[DispatchRaysIndex().xy] = float4(g_aoCB.aoRadius, g_aoCB.aoRadius, g_aoCB.aoRadius, 1.0f);
	}

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
	
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in Attributes attrib)
{
	payload.T = RayTCurrent();
}

#endif //_RT_AO_HLSL_