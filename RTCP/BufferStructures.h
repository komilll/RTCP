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

typedef struct _uberBufferStruct {
	XMFLOAT3 viewPosition;

	float padding[61];
} UberBufferStruct;
static_assert((sizeof(UberBufferStruct) % 256) == 0, "Uber Buffer size must be 256-byte aligned");

typedef struct _sceneConstantBuffer
{
	XMMATRIX projectionToWorld;
	XMFLOAT4 cameraPosition;
	XMFLOAT4 lightPosition;
	XMFLOAT4 lightAmbientColor;
	XMFLOAT4 lightDiffuseColor;

	//float padding[32];
} SceneConstantBuffer;
//static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Scene Constant Buffer size must be 256-byte aligned");

typedef struct _cubeConstantBuffer
{
	XMFLOAT4 albedo = XMFLOAT4{ 1, 0, 0, 1 };
} CubeConstantBuffer;

#endif // !_BUFFER_STRUCTURES_H_
