#pragma once

#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

#include "pch.h"
#include "DeviceManager.h"
#include "ModelClass.h"
#include "BufferStructures.h"
#include "CBuffer.h"
#include "RaytracingShadersHelper.h"
#include "RaytracingResources.h"

using namespace DirectX;
typedef std::array<D3D12_INPUT_ELEMENT_DESC, 5> BasicInputLayout;

// DEFINES
#define ROOT_SIGNATURE_PIXEL D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;// | //D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

class Renderer
{
public:
	// No default constructor
	Renderer(std::shared_ptr<DeviceManager> deviceManager, HWND hwnd);

	// Main events occuring in renderer - init/loop/destroy
	void OnInit(HWND hwnd);
	void OnUpdate();
	void OnRender();
	void OnDestroy();

	// Camera position/rotation manipulation - controlled by user's input
	void AddCameraPosition(float x, float y, float z);
	void AddCameraPosition(XMFLOAT3 addPos);
	void AddCameraRotation(float x, float y, float z);
	void AddCameraRotation(XMFLOAT3 addRot);

	// Modifying rendering state - DEBUG
	void ToggleRaytracing() { DO_RAYTRACING = !DO_RAYTRACING; };

private:
	// Prepare pipeline and load data to start rendering
	void LoadPipeline(HWND hwnd);
	void LoadAssets();

	// Executing commands/synchronization functions
	void PopulateCommandList();
	void WaitForPreviousFrame();
	void MoveToNextFrame();

	void CreateViewAndPerspective();

	// Helper functions, creating structures
	void CreateTexture2D(ComPtr<ID3D12Resource>& texture, UINT64 width, UINT height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATES InitialResourceState = D3D12_RESOURCE_STATE_COPY_SOURCE);
	void CreateTextureFromFileRTCP(ComPtr<ID3D12Resource>& texture, ComPtr<ID3D12GraphicsCommandList4> commandList, const wchar_t* path, ComPtr<ID3D12Resource>& uploadHeap);
	
	void CreateRootSignatureRTCP(UINT rootParamCount, UINT samplerCount, CD3DX12_ROOT_PARAMETER rootParameters[], CD3DX12_STATIC_SAMPLER_DESC samplers[], D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ComPtr<ID3D12RootSignature>& rootSignature);
	void CreateRootSignatureRTCP(UINT rootParamCount, UINT samplerCount, CD3DX12_ROOT_PARAMETER rootParameters[], CD3DX12_STATIC_SAMPLER_DESC samplers[], D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ID3D12RootSignature*& rootSignature);
	
	static void CreateSRV(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc);
	static void CreateSRV_Texture2D(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });
	static void CreateSRV_Texture3D(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE3D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });
	static void CreateSRV_TextureCube(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURECUBE, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });

	D3D12_SHADER_RESOURCE_VIEW_DESC GetVertexBufferSRVDesc(ModelClass* model, UINT vertexStructSize);
	D3D12_SHADER_RESOURCE_VIEW_DESC GetIndexBufferSRVDesc(ModelClass* model);
	template <class T>
	D3D12_CONSTANT_BUFFER_VIEW_DESC GetConstantBufferViewDesc(CBuffer<T> cbuffer);
	D3D12_SHADER_RESOURCE_VIEW_DESC GetAccelerationStructureDesc(ComPtr<ID3D12Resource>& tlasResult);
	D3D12_UNORDERED_ACCESS_VIEW_DESC GetAccessViewDesc(DXGI_FORMAT Format, D3D12_UAV_DIMENSION ViewDimension);

	// Helper functions, returning default descriptions/structres
	BasicInputLayout CreateBasicInputLayout();
	CD3DX12_DEPTH_STENCIL_DESC1 CreateDefaultDepthStencilDesc();
	D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateDefaultPSO(BasicInputLayout inputElementDescs, ComPtr<ID3DBlob> vertexShader, ComPtr<ID3DBlob> pixelShader, D3D12_DEPTH_STENCIL_DESC depthStencilDesc, ComPtr<ID3D12RootSignature> rootSignature);

	// Compiling shaders
	void InitShaderCompiler(D3D12ShaderCompilerInfo& shaderCompiler) const;
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, D3D12ShaderInfo& info, IDxcBlob** blob) const;
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, RtProgram& program) const;

#pragma region Raytracing functions
	// Main function, invoked to prepare DXR pipeline
	void PrepareRaytracingResources();
	void PrepareRaytracingResourcesAO();

	//// Functions called by PrepareRaytracingResources()
	//void CreateBLAS(std::shared_ptr<ModelClass> model, ComPtr<ID3D12GraphicsCommandList4> commandList, ComPtr<ID3D12Resource>& blasScratch, ComPtr<ID3D12Resource>& blasResult, D3D12_RAYTRACING_GEOMETRY_FLAGS rayTracingFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
	//void CreateTLAS(ComPtr<ID3D12Resource> blasResult, ComPtr<ID3D12GraphicsCommandList4> commandList, ComPtr<ID3D12Resource>& tlasInstanceDesc, ComPtr<ID3D12Resource>& tlasScratch, ComPtr<ID3D12Resource>& tlasResult, UINT tlasFlags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
	//void CreateDxrPipelineAssets(ModelClass* model);
	//void CreateShaderTable(ComPtr<ID3D12Resource>& shaderTable, ComPtr<ID3D12StateObject>& rtpso, ComPtr<ID3D12StateObjectProperties>& rtpsoInfo, RtProgram rayGenShader, RtProgram missShader, HitProgram hitShader, LPCWSTR hitGroupName, uint32_t& shaderTableRecordSize);
	//void CreateRTPSO(ComPtr<ID3D12StateObject>& rtpso, ComPtr<ID3D12StateObjectProperties>& rtpsoInfo, RtProgram rayGenShader, RtProgram missShader, HitProgram hitShader, LPCWSTR hitGroupName);

	// Functions called by PrepareRaytracingResources() - prepare initial buffer values
	void InitializeRaytracingBufferValues();

	// Functions called by PrepareRaytracingResources() - generating shaders
	void CreateRayGenShader(RtProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, int cbvDescriptors, int uavDescriptors, int srvDescriptors, std::vector<CD3DX12_STATIC_SAMPLER_DESC> samplers, LPCWSTR name, LPCWSTR nameToExport = L"RayGen");
	void CreateMissShader(RtProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, LPCWSTR name, LPCWSTR nameToExport = L"Miss") const;
	void CreateClosestHitShader(HitProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, LPCWSTR name, LPCWSTR nameToExport = L"ClosestHit") const;
	void CreateAnyHitShader(HitProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, LPCWSTR name, LPCWSTR nameToExport = L"AnyHit") const;

#pragma endregion

private:
	// CONST PROPERTIES
	static constexpr float Z_NEAR = 0.01f;
	static constexpr float Z_FAR = 200.0f;
	static constexpr int m_frameCount = 2;
	static constexpr int m_windowWidth = 1280;
	static constexpr int m_windowHeight = 720;
	static constexpr float m_aspectRatio = static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight);

	// DEBUG PROPERTIES
	bool FREEZE_CAMERA = false;
	bool DO_RAYTRACING = true;

	// Camera settings
	XMFLOAT3 m_cameraPosition{ 0,0,0 };
	XMFLOAT3 m_cameraRotation{ 0,0,0 };
	XMFLOAT3 m_cameraPositionStoredInFrame{ 0,0,0 };

	// Helper variables
	UINT m_rtvDescriptorSize = 0;

	// Pipeline variables - device, commandQueue, swap chain
	int m_frameIndex = 0;
	ComPtr<ID3D12Device5> m_device					= NULL;
	std::shared_ptr<DeviceManager> m_deviceManager	= NULL;
	ComPtr<ID3D12CommandQueue> m_commandQueue		= NULL;
	ComPtr<IDXGISwapChain3> m_swapChain				= NULL;

	// Viewport related variables
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

#pragma region Pushing data to GPU - variable references
	// Command lists
	ComPtr<ID3D12GraphicsCommandList4> m_commandList = NULL;
	ComPtr<ID3D12GraphicsCommandList4> m_commandListSkybox = NULL;

	// Command allocators
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[m_frameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocatorsSkybox[m_frameCount];

	// Descriptor heaps
	ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;

	// Root signatures/PSO
	ComPtr<ID3D12RootSignature> m_rootSignature = NULL;
	ComPtr<ID3D12PipelineState> m_pipelineState = NULL;
	ComPtr<ID3D12RootSignature> m_rootSignatureSkybox = NULL;
	ComPtr<ID3D12PipelineState> m_pipelineStateSkybox = NULL;
#pragma endregion

#pragma region Resource variables
	// Depth stencil view (DSV)
	ComPtr<ID3D12Resource> m_depthStencil;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12Resource> m_depthBuffer = NULL;

	// Constant buffers
	CBuffer<SceneConstantBuffer> m_sceneBuffer;
	CBuffer<CubeConstantBuffer> m_cubeBuffer;
	CBuffer<AoConstantBuffer> m_aoBuffer;
	CBuffer<UberBufferStruct> m_uberBuffer;
	CBuffer<ConstantBufferStruct> m_constantBuffer;
	CBuffer<ConstantBufferStruct> m_constantBufferSkybox;

	// Textures
	ComPtr<ID3D12Resource> m_backBuffers[m_frameCount];
	ComPtr<ID3D12Resource> m_texture;
	ComPtr<ID3D12Resource> m_skyboxTexture;

	// Models
	std::shared_ptr<ModelClass> m_modelCube = NULL;
	std::shared_ptr<ModelClass> m_modelSphere = NULL;
	std::shared_ptr<ModelClass> m_modelBuddha = NULL;
#pragma endregion
	
#pragma region Raytracing variables
	// Raytracing - resources containing all data for single raytracing shaders group
	std::shared_ptr<RaytracingResources> m_raytracingNormal = NULL;
	std::shared_ptr<RaytracingResources> m_raytracingAO = NULL;

	// DXR - output texture and descriptor heap of resources
	ComPtr<ID3D12Resource> m_rtPositionTexture = NULL;
	ComPtr<ID3D12Resource> m_rtNormalTexture = NULL;
	ComPtr<ID3D12Resource> m_rtAoTexture = NULL;

	// Shader compiler
	D3D12ShaderCompilerInfo m_shaderCompiler{};
#pragma endregion

	// Synchronization
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[m_frameCount];
	HANDLE m_fenceEvent;
};
