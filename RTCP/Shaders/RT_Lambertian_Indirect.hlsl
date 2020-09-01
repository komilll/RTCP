#ifndef _RT_LAMBERTIAN_INDIRECT_HLSL_
#define _RT_LAMBERTIAN_INDIRECT_HLSL_

#include "RT_HelperFunctions.hlsl"
#include "UniformDistribution.hlsl"
#include "SamplingHelper.hlsl"
#include "ALL_CommonBuffers.hlsl"
#include "RT_Lambertian.hlsl"

[shader("miss")]
void MissIndirect(inout PayloadIndirect payload : SV_RayPayload)
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;
	float3 rayDir;
	float3 origin = g_sceneCB.cameraPosition.xyz;
	GenerateCameraRay(LaunchIndex, LaunchDimensions, g_sceneCB.projectionToWorld, origin, rayDir);
	
	rayDir.z = -rayDir.z;
	
	payload.color = skyboxTexture.SampleLevel(g_sampler, rayDir, 0).rgb;
}

[shader("closesthit")]
void ClosestHitIndirect(inout PayloadIndirect payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	// Pick a random light from our scene to shoot a shadow ray towards	
	//int lightToSample = min(int(nextRand(rayData.rndSeed) * gLightsCount), gLightsCount - 1);

	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;
		
	float4 albedo = albedoTexture.Load(int3(LaunchIndex, 0));
	float4 normalAndDepth = NormalTextureInput.Load(int3(LaunchIndex, 0));
	// Figure out pixel world space position (using length of a primary ray found in previous pass)
	float3 primaryRayOrigin = g_sceneCB.cameraPosition.xyz;
	float3 primaryRayDirection;
	GenerateCameraRay(LaunchIndex, LaunchDimensions, g_sceneCB.projectionToWorld, primaryRayOrigin, primaryRayDirection);
	float3 pixelWorldSpacePosition = primaryRayOrigin + (primaryRayDirection * normalAndDepth.w);
		
	// Query the scene to find info about the randomly selected light
	float3 lightIntensity = 1.0f;
	float3 toLight = g_sceneCB.lightPosition.xyz - pixelWorldSpacePosition;
	float distToLight = length(toLight);
	//getLightData(lightToSample, shadeData.posW, toLight, lightIntensity, distToLight);

	// Compute our lambertion term (L dot N)
	float LdotN = saturate(dot(normalAndDepth.xyz, toLight));

	// Shoot our shadow ray to our randomly selected light
	float shadowMult = float(1) * shadowRayVisibility(pixelWorldSpacePosition, toLight, 1e-4f, distToLight);

	// Return the Lambertian shading color using the physically based Lambertian term (albedo / pi)
	payload.color = shadowMult; // * LdotN * lightIntensity * albedo.xyz / PI;
}

#endif //_RT_LAMBERTIAN_INDIRECT_HLSL_