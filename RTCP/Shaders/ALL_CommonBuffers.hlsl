#ifndef _COMMON_BUFFERS_HLSL_
#define _COMMON_BUFFERS_HLSL_

// Constant Buffers
struct SceneConstantBuffer
{
	matrix projectionToWorld;
	float4 cameraPosition;
	float4 lightPosition;
	float4 lightAmbientColor;
	float4 lightDiffuseColor;
	
	int frameCount;
	float padding[31];
};
////

struct CameraConstantBuffer
{
	float2 jitterCamera;
	float fNumber;
	float focalLength;

	float4 cameraU;
	float4 cameraV;
	float4 cameraW;

	float lensRadius;

	float padding[47];
};
#endif //_COMMON_BUFFERS_HLSL_