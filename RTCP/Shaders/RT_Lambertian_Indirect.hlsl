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

[shader("miss")]
void MissIndirect(inout RayPayload payload : SV_RayPayload)
{
	payload.vis = 1.0f;
}

[shader("closesthit")]
void ClosestHitIndirect(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	
}


#endif //_RT_LAMBERTIAN_HLSL_