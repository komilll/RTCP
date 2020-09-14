#ifndef _RT_GGX_HLSL_
#define _RT_GGX_HLSL_

#include "RT_HelperFunctions.hlsl"
#include "UniformDistribution.hlsl"
#include "SamplingHelper.hlsl"
#include "ALL_CommonBuffers.hlsl"
#include "PS_BRDF_Helper.hlsl"

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
	int accFrames;
	float roughness;
	float metallic;
	float padding[60];
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

float3 shootIndirectRay(float3 rayOrigin, float3 rayDir, float minT, uint seed)
{
	// Setup shadow ray
	RayDesc rayColor = { rayOrigin, minT, rayDir, 1e+38f };
	
	PayloadIndirect payload;
	payload.color = float3(0, 0, 0);
	payload.rndSeed = seed;
	
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

float3 ggxDirect(inout uint rndSeed, float3 hit, float3 N, float3 V, float3 dif, float3 spec, float rough)
{
	float lightsCount = g_lightCB.lightPositionAndType[15].w;
	int lightToSample = min(int(nextRand(rndSeed) * lightsCount), lightsCount - 1);
	
	float3 lightColor = g_lightCB.lightDiffuseColor[lightToSample].rgb;
	float3 toLight = g_lightCB.lightPositionAndType[lightToSample].xyz - hit;
	float distToLight = length(toLight);
	float3 L = normalize(toLight);
	float lightIntensity = 1.0f;
	
	// Compute our lambertion term (N dot L)
	float NoL = saturate(dot(N, L));

	// Shoot our shadow ray to our randomly selected light
	float shadowMult = /*float(lightsCount) **/shadowRayVisibility(hit, L, 1e-4f, distToLight);

	// Compute half vectors and additional dot products for GGX
	float3 H = normalize(V + L);
	float NoH = saturate(dot(N, H));
	float LoH = saturate(dot(L, H));
	float NoV = saturate(dot(N, V));

	// Evaluate terms for our GGX BRDF model
	float D = Specular_D_GGX(rough, NoH);
	float G = Specular_G_SchlickGGX(rough, NoL) * Specular_G_SchlickGGX(rough, NoV);
	float3 F = Specular_F_Schlick(spec, LoH);

	// Evaluate the Cook-Torrance Microfacet BRDF model
	//     Cancel out NdotL here & the next eq. to avoid catastrophic numerical precision issues.
	float3 ggxTerm = D * G * F / (4 * NoV /* * NdotL */);

	// Compute our final color (combining diffuse lobe plus specular GGX lobe)
	//return NoL * dif / PI;
	//return D * F * G / (4.0 * NoL * NoV);
	return shadowMult * lightIntensity * ( /* NdotL * */ggxTerm + NoL * dif / PI);
}

[shader("raygeneration")]
void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

	//float4 albedo = albedoTexture.Load(int3(LaunchIndex, 0));
	float4 albedo = float4(1.0f, 0.71f, 0.29f, 1.0f);
	float4 specular = specularTexture.Load(int3(LaunchIndex, 0));
	//float roughness = specular.g;
	//float metallic = specular.b;
	float roughness = g_giCB.roughness;//1.0f;
	roughness = clamp(roughness, 0.05f, 0.999f);
	float metallic = g_giCB.metallic;
	const float3 diffuseColor = saturate(albedo - albedo * metallic);
	const float3 specularColor = saturate(lerp(0.04f, albedo, metallic));
	float4 normalAndDepth = NormalTextureInput.Load(int3(LaunchIndex, 0));
	// Figure out pixel world space position (using length of a primary ray found in previous pass)
	float3 primaryRayOrigin = g_sceneCB.cameraPosition.xyz;
	float3 primaryRayDirection;
	GenerateCameraRay(LaunchIndex, LaunchDimensions, g_sceneCB.projectionToWorld, primaryRayOrigin, primaryRayDirection);
	
	float3 pixelWorldSpacePosition = primaryRayOrigin + (primaryRayDirection * normalAndDepth.w);
	if (normalAndDepth.w == 0)
	{
		RTOutput[LaunchIndex] = albedoTexture.Load(int3(LaunchIndex, 0));
		return;
	}
	
	float3 shadeColor = albedo.rgb / PI;
	float lightsCount = g_lightCB.lightPositionAndType[15].w;
	if (lightsCount > 0)
	{
		uint seed = initRand(LaunchIndex.x + LaunchIndex.y * LaunchDimensions.x, g_sceneCB.frameCount);
		//int lightToSample = min(int(nextRand(seed) * lightsCount), lightsCount - 1);
		
		//float3 lightColor = g_lightCB.lightDiffuseColor[lightToSample].rgb;
		//float3 toLight = g_lightCB.lightPositionAndType[lightToSample].xyz - pixelWorldSpacePosition;
		//float distToLight = length(toLight);
		//toLight = normalize(toLight);
		//float NoL = saturate(dot(normalAndDepth.xyz, toLight));
			
		//float shadowMult = shadowRayVisibility(pixelWorldSpacePosition, toLight, 1e-4f, distToLight);
		//shadeColor += shadowMult * NoL * albedo.rgb / PI;
		float3 V = normalize(g_sceneCB.cameraPosition.xyz - pixelWorldSpacePosition.xyz);
		shadeColor = ggxDirect(seed, pixelWorldSpacePosition, normalAndDepth.xyz, V, diffuseColor, specularColor, roughness);
		
		//if (g_giCB.useIndirect == 1)
		//{
		//	float3 bounceDir = getCosHemisphereSample(seed, normalAndDepth.xyz);
		//	float NoDir = saturate(dot(normalAndDepth.xyz, bounceDir));
		//	float3 bounceColor = shootIndirectRay(pixelWorldSpacePosition, bounceDir, 1e-4f, seed);
		//	float sampleProb = NoDir / PI;
		//	shadeColor += (NoDir * bounceColor * albedo.rgb / PI) / sampleProb;
		//}
	}
	
	float4 prevScene = RTOutput[LaunchIndex];
	float4 finalColor = float4(shadeColor, 1.0f);
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

//[shader("miss")]
//void MissIndirect(inout PayloadIndirect payload : SV_RayPayload)
//{
//	float3 rayDir = WorldRayDirection();
//	rayDir.z = -rayDir.z;
	
//	payload.color = skyboxTexture.SampleLevel(g_sampler, rayDir, 0).rgb;
//}

//[shader("closesthit")]
//void ClosestHitIndirect(inout PayloadIndirect payload, in BuiltInTriangleIntersectionAttributes attribs)
//{
//	float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	
//	uint indexSizeInBytes = 4;
//	uint indicesPerTriangle = 3;
//	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
//	uint baseIndex = PrimitiveIndex() * triangleIndexStride;
	
//	const uint3 indices_ = indices.Load3(baseIndex);
//	float3 vertexNormals[3] =
//	{
//		vertices[indices_[0]].normal,
//		vertices[indices_[1]].normal,
//		vertices[indices_[2]].normal
//	};
//	float3 triangleNormal = HitAttribute(vertexNormals, attribs);
	
//	//
//	float lightsCount = g_lightCB.lightPositionAndType[15].w;
//	int lightToSample = min(int(nextRand(payload.rndSeed) * lightsCount), lightsCount - 1);
	
//	float3 lightColor = g_lightCB.lightDiffuseColor[lightToSample].rgb;
//	float3 toLight = g_lightCB.lightPositionAndType[lightToSample].xyz - hitPos;
//	float distToLight = length(toLight);
//	toLight = normalize(toLight);
//	float NoL = saturate(dot(triangleNormal.xyz, toLight));
	
//	float visibility = shadowRayVisibility(hitPos, toLight, 1e-4f, 1e+38f);
//	float4 albedo = albedoTexture.Load(int3(DispatchRaysIndex().xy, 0));

//	payload.color = visibility * NoL * albedo.rgb / PI;
//}

#endif //_RT_GGX_HLSL_