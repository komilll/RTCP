#ifndef _RAYGEN_HLSL
#define _RAYGEN_HLSL

#include "Common.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
	float3 rayDir;
	float3 origin;
	
	GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);
	
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = rayDir;
	ray.TMin = 0.001;
	ray.TMax = 10000.0;
	RayPayload payload = { float4(0, 0, 0, 0) };
	// https://docs.microsoft.com/en-us/windows/win32/direct3d12/traceray-function
	TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

	RTOutput[DispatchRaysIndex().xy] = payload.color;
}

#endif //_RAYGEN_HLSL