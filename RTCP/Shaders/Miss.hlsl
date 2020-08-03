#ifndef _MISS_HLSL
#define _MISS_HLSL

#include "Common.hlsl"

// ---[ Miss Shader ]---

[shader("miss")]
void Miss(inout RayPayload payload)
{
    payload.color = float4(0.0f, 0.2f, 0.4f, 1.0f);
}

#endif // _MISS_HLSL
