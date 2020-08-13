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