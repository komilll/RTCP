#ifndef _RT_HELPER_FUNCTIONS_HLSL_
#define _RT_HELPER_FUNCTIONS_HLSL_

#include "ALL_CommonBuffers.hlsl"
#include "SamplingHelper.hlsl"

inline void GenerateCameraRay(uint2 index, uint2 dimensions, float4x4 projectionToWorld, inout float3 origin, out float3 direction)
{
	float2 xy = index + float2(0.5f, 0.5f); // center in the middle of the pixel.
	float2 screenPos = (xy / (float2) dimensions) * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), projectionToWorld);

	world.xyz /= world.w;
	direction = normalize(world.xyz - origin);
}

inline void GenerateCameraRay(uint2 index, uint2 dimensions, float4x4 projectionToWorld, inout float3 origin, out float3 direction, in SceneConstantBuffer scene, in
CameraConstantBuffer camera)
{
	float2 xy = index + camera.jitterCamera; // center in the middle of the pixel.
	float2 screenPos = (xy / DispatchRaysDimensions().xy) * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
	screenPos *= float2(0.5 * 16.0f / 9.0f, 0.5);

    // Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), projectionToWorld);

	world.xyz /= world.w;
	direction = screenPos.x * camera.cameraU.xyz + screenPos.y * camera.cameraV.xyz + camera.cameraW.xyz;
	direction /= length(camera.cameraW.xyz);
	
	// Calculate random ray origin
	float3 focalPoint = scene.cameraPosition.xyz + camera.focalLength * direction;
	
	uint randSeed = initRand(index.x + index.y * DispatchRaysDimensions().x, scene.frameCount);
	float2 rnd = float2(nextRand(randSeed) * PI * 2.0, nextRand(randSeed) * camera.lensRadius);
	float2 uv = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);
	
	origin = scene.cameraPosition.xyz + uv.x * normalize(camera.cameraU.xyz) + uv.y * normalize(camera.cameraV.xyz);
}

#endif // _RT_HELPER_FUNCTIONS_HLSL_
