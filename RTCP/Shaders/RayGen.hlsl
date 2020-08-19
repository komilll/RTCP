#ifndef _RAYGEN_HLSL
#define _RAYGEN_HLSL

#include "Common.hlsl"
#include "UniformDistribution.hlsl"
#include "RT_HelperFunctions.hlsl"

[shader("raygeneration")]
void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;
	float3 rayDir;
	float3 origin = g_sceneCB.cameraPosition.xyz;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	GenerateCameraRay(LaunchIndex, LaunchDimensions, g_sceneCB.projectionToWorld, origin, rayDir, g_sceneCB, g_cameraCB);

	RayDesc ray = { origin, 1e-4f, rayDir, 1e+38f};
	RayPayload payload;
	payload.worldPos = float4(0, 0, 0, 0);
	payload.normalWithDepth = float4(0, 0, 0, 0);
	
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
	//RAY_FLAG_CULL_BACK_FACING_TRIANGLES
	
	RTOutputNormal[DispatchRaysIndex().xy] = payload.normalWithDepth;
	RTOutputPosition[DispatchRaysIndex().xy] = payload.worldPos;
}

#endif //_RAYGEN_HLSL