#pragma once

#include "pch.h"
#include "DeviceManager.h"

class Renderer
{
public:
	Renderer() = default;
	Renderer(std::shared_ptr<DeviceManager> deviceManager);

	void Render();
	void PrepareDataStructures();

private:
	std::shared_ptr<DeviceManager> m_deviceManager;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

};