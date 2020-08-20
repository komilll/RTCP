#ifndef _MISS_HLSL
#define _MISS_HLSL

#include "Common.hlsl"

// ---[ Miss Shader ]---

[shader("miss")]
void Miss(inout RayPayload payload)
{
	payload.albedo = float4(0, 0, 0, 0);
	payload.normalWithDepth = float4(0, 0, 0, 0);
}

#endif // _MISS_HLSL
