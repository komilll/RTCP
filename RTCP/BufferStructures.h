#pragma once

#ifndef _BUFFER_STRUCTURES_H_
#define _BUFFER_STRUCTURES_H_

#include "pch.h"
using namespace DirectX;

typedef struct _constantBufferStruct {
	XMMATRIX world;
	XMMATRIX view;
	XMMATRIX projection;

	XMMATRIX paddingMatrix;
} ConstantBufferStruct;
static_assert((sizeof(ConstantBufferStruct) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

typedef struct _sceneConstantBuffer
{
	XMMATRIX projectionToWorld;
	XMFLOAT4 cameraPosition;
	XMFLOAT4 lightPosition;
	XMFLOAT4 lightAmbientColor;
	XMFLOAT4 lightDiffuseColor;
	int frameCount;

	float padding[31] = {};
} SceneConstantBuffer;
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Scene Constant Buffer size must be 256-byte aligned");

typedef struct _aoConstantBuffer
{
	float aoRadius = 1.0f;
	float minT = 1e-4f;
	int accFrames;

	float padding[61] = {};
} AoConstantBuffer;
static_assert((sizeof(AoConstantBuffer) % 256) == 0, "Ao Constant Buffer size must be 256-byte aligned");

typedef struct _giConstantBuffer
{
	int useIndirect = 1;

	float padding[63] = {};
} GiConstantBuffer;
static_assert((sizeof(GiConstantBuffer) % 256) == 0, "GI Constant Buffer size must be 256-byte aligned");


typedef struct _cameraConstantBuffer
{
	XMFLOAT2 jitterCamera;
	float fNumber = 32.0f;
	float focalLength = 1.0f;

	XMFLOAT4 cameraU;
	XMFLOAT4 cameraV;
	XMFLOAT4 cameraW;

	float lensRadius;

	float padding[47] = {};
} CameraConstantBuffer;
static_assert((sizeof(CameraConstantBuffer) % 256) == 0, "Camera Constant Buffer size must be 256-byte aligned");

#endif // !_BUFFER_STRUCTURES_H_
