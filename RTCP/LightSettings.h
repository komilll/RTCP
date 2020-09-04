#pragma once
#ifndef _LIGHT_SETTINGS_H_
#define _LIGHT_SETTINGS_H_

#include "pch.h"

using namespace DirectX;

enum LightType : int {
	Directional = 0,
	Point = 1
};

struct Light
{
public:
	LightType type;
	XMFLOAT3 position;
	XMFLOAT3 rotation;
	XMFLOAT4 color;
};

class LightSettings 
{
public:
	void AddLight(Light light);
	void RemoveLight(int lightIndex);
	void SaveLightToFile();
	void LoadLightFromFile();

	std::vector<Light>& GetLightsInfo() { return m_lights; }

private:
	std::vector<Light> m_lights;
	const std::string lightDataPath = "lights.txt";
};

#endif // !_LIGHT_SETTINGS_H_
