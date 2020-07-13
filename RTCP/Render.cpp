#include "Render.h"

Renderer::Renderer(std::shared_ptr<DeviceManager> deviceManager)
{
	m_deviceManager = deviceManager;
    assert(m_deviceManager && "Device manager is null");

    m_commandList = m_deviceManager->GetCommandList();
}

void Renderer::Render()
{
    auto commandAllocator = m_deviceManager->GetCurrentCommandAllocator();
    auto backBuffer = m_deviceManager->GetCurrentBackBufferResource();

    commandAllocator->Reset();
    m_commandList->Reset(commandAllocator.Get(), nullptr);

    m_deviceManager->ClearRenderTarget();
    m_deviceManager->Present();
}

void Renderer::PrepareDataStructures()
{

}
