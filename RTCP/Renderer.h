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

	void OnInit(HWND hwnd);
	void OnUpdate();
	void OnRender();
	void OnDestroy();

	void AddCameraPosition(float x, float y, float z);
	void AddCameraPosition(XMFLOAT3 addPos);

	void AddCameraRotation(float x, float y, float z);
	void AddCameraRotation(XMFLOAT3 addRot);

private:
	void UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, bool setName = false, LPCWSTR newName = L"");

	void ResizeDepthBuffer(int width, int height);

	void LoadPipeline(HWND hwnd);
	void LoadAssets();

	void PopulateCommandList();
	void WaitForPreviousFrame();

	void MoveToNextFrame();

	void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter);

	void CreateViewAndPerspective();

private:
	// CONST PROPERTIES
	static constexpr float Z_NEAR = 0.01f;
	static constexpr float Z_FAR = 200.0f;

	// DEBUG PROPERTIES
	bool FREEZE_CAMERA = false;

	// Camera settings
	XMFLOAT3 m_cameraPosition{ 0,0,0 };
	XMFLOAT3 m_cameraRotation{ 0,0,0 };
	XMFLOAT3 m_cameraPositionStoredInFrame{ 0,0,0 };

	typedef struct _constantBufferStruct {
		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX projection;

		XMMATRIX paddingMatrix;
	} ConstantBufferStruct;
	static_assert((sizeof(ConstantBufferStruct) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

	ComPtr<ID3D12Device2> m_device = NULL;
	static constexpr int m_frameCount = 2;

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
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[m_frameCount];
	ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
	UINT m_rtvDescriptorSize;

	ComPtr<ID3D12DescriptorHeap> m_textureHeap;

	ComPtr<ID3D12Resource> m_vertexBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_indexBuffer = NULL;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	UINT m_indicesCount = 0;

	UINT8* m_pCbvDataBegin = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
	ComPtr<ID3D12Resource> m_constantBuffer = NULL;
	ConstantBufferStruct m_constantBufferData;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	ComPtr<ID3D12Resource> m_depthBuffer		= NULL;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap		= NULL;
	ComPtr<ID3D12RootSignature> m_rootSignature = NULL;
	ComPtr<ID3D12PipelineState> m_pipelineState = NULL;

	//ComPtr<ID3D12Resource> albedoTexture;
	//ComPtr<ID3D12Resource> normalTexture;
	//ComPtr<ID3D12Resource> roughnessTexture;
	//ComPtr<ID3D12Resource> metalnessTexture;

	std::unique_ptr<ModelClass> m_model = NULL;

	bool m_contentLoaded = false;

	//Synchronization
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[m_frameCount];
	HANDLE m_fenceEvent;
};