#pragma once

#include "pch.h"
#include "DeviceManager.h"
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>
#include "ModelClass.h"

using namespace DirectX;
typedef std::array<D3D12_INPUT_ELEMENT_DESC, 5> BasicInputLayout;

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
	void LoadPipeline(HWND hwnd);
	void LoadAssets();

	void PopulateCommandList();
	void WaitForPreviousFrame();

	void MoveToNextFrame();

	void CreateViewAndPerspective();

	void CreateRootSignatureRTCP(UINT rootParamCount, UINT samplerCount, CD3DX12_ROOT_PARAMETER rootParameters[], CD3DX12_STATIC_SAMPLER_DESC samplers[], D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ComPtr<ID3D12RootSignature>& rootSignature);

	CD3DX12_DEPTH_STENCIL_DESC1 CreateDefaultDepthStencilDesc();
	D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateDefaultPSO(BasicInputLayout inputElementDescs, ComPtr<ID3DBlob> vertexShader, ComPtr<ID3DBlob> pixelShader, D3D12_DEPTH_STENCIL_DESC depthStencilDesc, ComPtr<ID3D12RootSignature> rootSignature);

	BasicInputLayout CreateBasicInputLayout();
	
	void CreateTextureRTCP(ComPtr<ID3D12Resource>& texture, ComPtr<ID3D12GraphicsCommandList4> commandList, const wchar_t* path, ComPtr<ID3D12Resource>& uploadHeap);

	// Raytracing functions
	struct Viewport
	{
		float left;
		float top;
		float right;
		float bottom;
	};

	struct RayGenConstantBuffer
	{
		Viewport viewport;
		Viewport stencil;
	};

	ComPtr<ID3D12Resource> m_blasScratch = NULL;
	ComPtr<ID3D12Resource> m_blasResult  = NULL;

	RayGenConstantBuffer m_rayGenCB;
	ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
	ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignature;

	void PrepareRaytracingResources();

	void CreateBLAS(std::shared_ptr<ModelClass> model);
	void CreateTLAS();

	void CreateRootSignatureForRaytracing();

	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);

private:
	// CONST PROPERTIES
	static constexpr float Z_NEAR = 0.01f;
	static constexpr float Z_FAR = 200.0f;

	// DEBUG PROPERTIES
	bool FREEZE_CAMERA = false;

	// DEFINES
	#define ROOT_SIGNATURE_PIXEL D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;// | //D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

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

	typedef struct _uberBufferStruct {
		XMFLOAT3 viewPosition;

		float padding[61];
	} UberBufferStruct;
	static_assert((sizeof(UberBufferStruct) % 256) == 0, "Uber Buffer size must be 256-byte aligned");

	ComPtr<ID3D12Device5> m_device = NULL;
	static constexpr int m_frameCount = 2;

	static constexpr int m_windowWidth = 1280;
	static constexpr int m_windowHeight = 720;
	static constexpr float m_aspectRatio = (float)m_windowWidth / (float)m_windowHeight;
	bool m_vSync = true;
	bool m_tearingSupported = true;
	int m_frameIndex = 0;

	std::shared_ptr<DeviceManager> m_deviceManager  = NULL;
	ComPtr<ID3D12GraphicsCommandList4> m_commandList = NULL;
	ComPtr<ID3D12GraphicsCommandList4> m_commandListSkybox = NULL;
	ComPtr<ID3D12CommandQueue> m_commandQueue		= NULL;
	ComPtr<IDXGISwapChain3> m_swapChain				= NULL;
	ComPtr<ID3D12Resource> m_backBuffers[m_frameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[m_frameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocatorsSkybox[m_frameCount];
	ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
	UINT m_rtvDescriptorSize;

	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	ComPtr<ID3D12Resource> m_texture;
	ComPtr<ID3D12Resource> m_skyboxTexture;

	UINT8* m_pConstantBufferDataBegin = nullptr;
	UINT8* m_pUberBufferDataBegin = nullptr;

	UINT8* m_pConstantBufferDataBeginSkybox = nullptr;
	ConstantBufferStruct m_constantBufferData;
	UberBufferStruct m_uberBufferData;

	ComPtr<ID3D12Resource> m_constantBuffers[2];
	ComPtr<ID3D12Resource> m_constantBufferSkyboxMatrices;

	// Depth stencil view (DSV)
	ComPtr<ID3D12Resource> m_depthStencil;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	ComPtr<ID3D12Resource> m_depthBuffer		= NULL;
	ComPtr<ID3D12RootSignature> m_rootSignature = NULL;
	ComPtr<ID3D12RootSignature> m_rootSignatureSkybox = NULL;
	ComPtr<ID3D12PipelineState> m_pipelineState = NULL;
	ComPtr<ID3D12PipelineState> m_pipelineStateSkybox = NULL;

	std::shared_ptr<ModelClass> m_modelCube = NULL;
	std::shared_ptr<ModelClass> m_modelSphere = NULL;

	bool m_contentLoaded = false;

	//Synchronization
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[m_frameCount];
	HANDLE m_fenceEvent;
};