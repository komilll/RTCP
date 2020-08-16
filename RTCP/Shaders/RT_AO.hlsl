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
	int missShader;
	float3 normal;
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
	float2 jitterCamera;
	
	int accFrames;
	float fNumber;
	float focalLength;
	float lensRadius;
	
	float4 cameraU;
	float4 cameraV;
	float4 cameraW;
};

// Resources
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<CubeConstantBuffer> g_cubeCB : register(b1);
ConstantBuffer<RaytracingAOBuffer> g_aoCB : register(b2);

RWTexture2D<float4> RTOutput : register(u0);

RaytracingAccelerationStructure SceneBVH : register(t0);
ByteAddressBuffer indices : register(t1);
StructuredBuffer<Vertex> vertices : register(t2);
Texture2D<float4> NormalTextureInput : register(t3);
Texture2D<float4> PositionTextureInput : register(t4);

SamplerState g_sampler : register(s0);
////

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

inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + g_aoCB.jitterCamera; // center in the middle of the pixel.
	//float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;
	float2 screenPos = (xy / DispatchRaysDimensions().xy) * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
	//screenPos.y = -screenPos.y;
	screenPos *= float2(0.5 * 16.0f / 9.0f, 0.5);

    // Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

	world.xyz /= world.w;
	//origin = g_sceneCB.cameraPosition.xyz;
	//direction = normalize(world.xyz - origin);
	direction = screenPos.x * g_aoCB.cameraU.xyz + screenPos.y * g_aoCB.cameraV.xyz + g_aoCB.cameraW.xyz;
	direction /= length(g_aoCB.cameraW.xyz);
	
	// Calculate random ray origin
	float3 focalPoint = g_sceneCB.cameraPosition.xyz + g_aoCB.focalLength * direction;
	
	uint randSeed = initRand(index.x + index.y * DispatchRaysDimensions().x, g_cubeCB.frameCount);
	float2 rnd = float2(nextRand(randSeed) * PI * 2.0, nextRand(randSeed) * g_aoCB.lensRadius);
	float2 uv = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);
	
	origin = g_sceneCB.cameraPosition.xyz + uv.x * normalize(g_aoCB.cameraU.xyz) + uv.y * normalize(g_aoCB.cameraV.xyz);
	//direction = normalize(world.xyz - origin);
}


[shader("raygeneration")]
void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

	// Figure out pixel world space position (using length of a primary ray found in previous pass)
	float3 primaryRayOrigin, primaryRayDirection; 
	GenerateCameraRay(LaunchIndex, primaryRayOrigin, primaryRayDirection);
	
	RayDesc distRay = { primaryRayOrigin, 1e-4f, primaryRayDirection, 1e+38f };
	RayPayload payload;
	payload.T = g_aoCB.aoRadius;
	payload.missShader = 0;
	payload.normal = 0;
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, distRay, payload);
	
	float2 pixelCenter = (float2) LaunchIndex / (float2) LaunchDimensions + float2(0.5f / (float) LaunchDimensions.x, 0.5f / (float) LaunchDimensions.y);
	float4 normalAndDepth;// = NormalTextureInput.SampleLevel(g_sampler, pixelCenter, 0);
	normalAndDepth.xyz = payload.normal;
	normalAndDepth.w = payload.T;
	float3 pixelWorldSpacePosition = primaryRayOrigin + (primaryRayDirection * normalAndDepth.w);

	if (payload.missShader == 1)
	{
        // Terminate if primary ray didn't hit anything
		RTOutput[LaunchIndex] = float4(1.0f, 1.0f, 1.0f, 1.0f);
		return;
	}
	
	uint seed = initRand(LaunchIndex.x + LaunchIndex.y * LaunchDimensions.x, g_cubeCB.frameCount);
	float3 worldDir = getCosHemisphereSample(seed.x, normalAndDepth.xyz);
	
	RayDesc aoRay;
	//RayPayload payload;
	float3 aoSampleDirection = worldDir;
	float ao = 0.0f;

	aoRay.Origin = pixelWorldSpacePosition;
	aoRay.TMin = g_aoCB.minT;
	aoRay.TMax = g_aoCB.aoRadius; //< Set max ray length to AO radius for early termination
	aoRay.Direction = aoSampleDirection;
	
	payload.T = g_aoCB.aoRadius; //< Set T to "maximum", to produce no occlusion in case ray doesn't hit anything (miss shader won't modify this value)

	// Trace the ray
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, aoRay, payload);
	ao = payload.T / g_aoCB.aoRadius;
	
	float prevAO = RTOutput[LaunchIndex].x;
	ao = ((float) g_aoCB.accFrames * prevAO + ao) / ((float) g_aoCB.accFrames + 1.0f);
	RTOutput[LaunchIndex.xy] = float4(ao, ao, ao, 1.0f); //< Replace all cached AO with current result
}

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
	// Empty
	payload.missShader = 1;
}

uint3 Load3x32BitIndices(uint offsetBytes)
{
	return indices.Load3(offsetBytes);
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], Attributes attrib)
{
	return vertexAttribute[0] +
        attrib.bary.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attrib.bary.y * (vertexAttribute[2] - vertexAttribute[0]);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in Attributes attrib)
{
	payload.T = RayTCurrent();
	
	uint indexSizeInBytes = 4;
	uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;
	
	const uint3 indices_ = Load3x32BitIndices(baseIndex);
	
	float3 vertexNormals[3] =
	{
		vertices[indices_[0]].normal,
		vertices[indices_[1]].normal,
		vertices[indices_[2]].normal
	};
	
	float3 triangleNormal = HitAttribute(vertexNormals, attrib);
	payload.normal = triangleNormal;
}

#endif //_RT_AO_HLSL_