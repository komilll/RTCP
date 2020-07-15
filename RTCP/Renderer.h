#pragma once

#include "pch.h"
#include "DeviceManager.h"
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>
#include "ModelClass.h"

using namespace DirectX;

class Renderer
{
public:
	Renderer() = default;
	Renderer(std::shared_ptr<DeviceManager> deviceManager, HWND hwnd);

	void Render();
	void PrepareDataStructures();

	void OnInit(HWND hwnd);
	void OnUpdate();
	void OnRender();
	void OnDestroy();

private:
	void UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	void ResizeDepthBuffer(int width, int height);

	void LoadPipeline(HWND hwnd);
	void LoadAssets();

	void PopulateCommandList();
	void WaitForPreviousFrame();

	void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter);

private:
	static constexpr bool m_useWarpDevice = false;
	ComPtr<ID3D12Device2> m_device = NULL;
	static constexpr int m_frameCount = 2;

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT4 color;
	};

	static constexpr int m_windowWidth = 1280;
	static constexpr int m_windowHeight = 720;
	static constexpr float m_aspectRatio = (float)m_windowWidth / (float)m_windowHeight;
	bool m_vSync = true;
	bool m_tearingSupported = true;
	int m_frameIndex = 0;

	std::shared_ptr<DeviceManager> m_deviceManager  = NULL;
	ComPtr<ID3D12GraphicsCommandList> m_commandList = NULL;
	ComPtr<ID3D12CommandQueue> m_commandQueue		= NULL;
	ComPtr<IDXGISwapChain3> m_swapChain				= NULL;
	ComPtr<ID3D12Resource> m_backBuffers[m_frameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator = NULL;
	ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
	UINT m_rtvDescriptorSize;

	ComPtr<ID3D12Resource> m_vertexBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_indexBuffer = NULL;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	ComPtr<ID3D12Resource> m_depthBuffer		= NULL;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap		= NULL;
	ComPtr<ID3D12RootSignature> m_rootSignature = NULL;
	ComPtr<ID3D12PipelineState> m_pipelineState = NULL;

	std::unique_ptr<ModelClass> m_model = NULL;

	bool m_contentLoaded = false;

	//Synchronization
	ComPtr<ID3D12Fence> m_fence;
	int m_fenceValue = 0;
	int m_frameFenceValues[m_frameCount] = {};
	HANDLE m_fenceEvent;
};