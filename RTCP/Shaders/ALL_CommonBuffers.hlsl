#ifndef _COMMON_BUFFERS_HLSL_
#define _COMMON_BUFFERS_HLSL_

// Standard Vertex structre
struct VertexInputType
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD0;
	uint textureID : TEXCOORD1;
};

struct Vertex
{
	float3 position;
	float3 normal;
	float3 tangent;
	float3 binormal;
	float2 uv;
	uint textureAlbedoID;
	uint textureSpecRoughnessID;
};

// Constant Buffers
struct SceneConstantBuffer
{
	matrix projectionToWorld;
	float4 cameraPosition;

	int frameCount;
	int specularIndex;
	float padding[42];
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