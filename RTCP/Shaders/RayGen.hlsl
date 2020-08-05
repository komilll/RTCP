#ifndef _RAYGEN_HLSL
#define _RAYGEN_HLSL

#include "Common.hlsl"

[shader("raygeneration")]
void RayGen()
{
	float3 rayDir, origin;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

	RayDesc ray = { g_sceneCB.cameraPosition.xyz, 0.0f, rayDir, 1e+38f };
	RayPayload payload = { float4(0, 0, 0, 0) };
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
	//RAY_FLAG_CULL_BACK_FACING_TRIANGLES
    // Write the raytraced color to the output texture.
	RTOutput[DispatchRaysIndex().xy] = payload.color;
}

#endif //_RAYGEN_HLSL