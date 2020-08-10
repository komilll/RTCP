#ifndef _RAYGEN_HLSL
#define _RAYGEN_HLSL

#include "Common.hlsl"
#include "UniformDistribution.hlsl"

[shader("raygeneration")]
void RayGen()
{
	float3 rayDir, origin, world;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

	RayDesc ray = { g_sceneCB.cameraPosition.xyz, 0.0f, rayDir, 1e+38f };
	RayPayload payload = { float4(0, 0, 0, 0) };
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
	//RAY_FLAG_CULL_BACK_FACING_TRIANGLES
    // Write the raytraced color to the output texture.
	
	uint2 seed = randomTea(asuint(rayDir.xy * rayDir.z * DispatchRaysDimensions().xy) + g_cubeCB.frameCount * float2(3, 8), 8);
	float2 Xi = HammersleyDistribution(g_cubeCB.frameCount, 8, seed);
	float3 L = UniformSampleSphere(Xi.x, Xi.y);
	
	RTOutput[DispatchRaysIndex().xy] = payload.color;
}

#endif //_RAYGEN_HLSL