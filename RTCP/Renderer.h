#pragma once

#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>
#include <random>
#include <chrono>

#include "pch.h"
#include "DeviceManager.h"
#include "ModelClass.h"
#include "BufferStructures.h"
#include "CBuffer.h"
#include "RaytracingShadersHelper.h"
#include "RaytracingResources.h"
#include "Profiler.h"
#include "LightSettings.h"

using namespace DirectX;
typedef std::array<D3D12_INPUT_ELEMENT_DESC, 6> BasicInputLayout;
struct WindowSize {
	int x, y;
};

// DEFINES
#define ROOT_SIGNATURE_PIXEL D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;// | //D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

class Renderer
{
	friend class GuiManager;
	friend class Main;
public:
	// No default constructor
	Renderer() = default;

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

	// Get properties of renderer
	WindowSize GetWindowSize() const { return { m_windowWidth, m_windowHeight }; };

private:
	// Prepare pipeline and load data to start rendering
	void LoadPipeline(HWND hwnd);
	void LoadAssets();

	// Executing commands/synchronization functions
	void PopulateCommandList();
	void CloseCommandList();
	void WaitForPreviousFrame();
	void MoveToNextFrame();

	void CreateViewAndPerspective();

	// Helper functions, creating structures
	void CreateTexture2D(ComPtr<ID3D12Resource>& texture, UINT64 width, UINT height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATES InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	void CreateTextureFromFileRTCP(ComPtr<ID3D12Resource>& texture, ComPtr<ID3D12GraphicsCommandList4> commandList, const wchar_t* path, ComPtr<ID3D12Resource>& uploadHeap, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATES InitialResourceState = D3D12_RESOURCE_STATE_COPY_SOURCE);
	
	void CreateRootSignatureRTCP(UINT rootParamCount, UINT samplerCount, CD3DX12_ROOT_PARAMETER rootParameters[], CD3DX12_STATIC_SAMPLER_DESC samplers[], D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ComPtr<ID3D12RootSignature>& rootSignature);
	void CreateRootSignatureRTCP(UINT rootParamCount, UINT samplerCount, CD3DX12_ROOT_PARAMETER rootParameters[], CD3DX12_STATIC_SAMPLER_DESC samplers[], D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ID3D12RootSignature*& rootSignature);
	
	void CreateCBV(ComPtr<ID3D12Resource>& resource, UINT bufferSize, D3D12_CPU_DESCRIPTOR_HANDLE& handle) const;

	static void CreateSRV(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc);
	static void CreateSRV_Texture2D(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });
	static void CreateSRV_Texture2DArray(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2DARRAY, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });
	static void CreateSRV_Texture3D(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE3D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });
	static void CreateSRV_TextureCube(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURECUBE, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });

	D3D12_SHADER_RESOURCE_VIEW_DESC GetDefaultSkyboxDesc(DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM) const;
	D3D12_SHADER_RESOURCE_VIEW_DESC GetDefaultSRVTexture2DDesc(DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM) const;
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
	void Compile_Shader(_In_ LPCWSTR pFileName, _In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines, _In_opt_ ID3DInclude* pInclude, _In_ LPCSTR pEntrypoint, _In_ LPCSTR pTarget, _In_ UINT Flags1, _In_ UINT Flags2, _Out_ ID3DBlob** ppCode) const;

#pragma region Raytracing functions
	// Main function, invoked to prepare DXR pipeline
	void PrepareRaytracingResources(const std::shared_ptr<ModelClass> model);
	void PrepareRaytracingResourcesAO(const std::shared_ptr<ModelClass> model);
	void PrepareRaytracingResourcesLambert(const std::shared_ptr<ModelClass> model);
	void PrepareRaytracingResourcesGGX(const std::shared_ptr<ModelClass> model);

	// Functions called by PrepareRaytracingResources() - generating shaders
	void CreateRayGenShader(RtProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, int cbvDescriptors, std::vector<TextureWithDesc> textures, std::vector<CD3DX12_STATIC_SAMPLER_DESC> samplers, LPCWSTR name, LPCWSTR nameToExport = nullptr);
	void CreateRayGenShader(RtProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, int cbvDescriptors, int uavDescriptors, int srvDescriptors, std::vector<CD3DX12_STATIC_SAMPLER_DESC> samplers, LPCWSTR name, LPCWSTR nameToExport = nullptr);
	void CreateRayGenShader(RtProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, std::vector<D3D12_DESCRIPTOR_RANGE> ranges, std::vector<CD3DX12_STATIC_SAMPLER_DESC> samplers, LPCWSTR name, LPCWSTR nameToExport = nullptr);
	void CreateMissShader(RtProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, LPCWSTR name, LPCWSTR nameToExport = nullptr) const;
	void CreateClosestHitShader(HitProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, LPCWSTR name, LPCWSTR nameToExport = nullptr) const;
	void CreateAnyHitShader(HitProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, LPCWSTR name, LPCWSTR nameToExport = nullptr) const;

#pragma endregion

private:
	// CONST PROPERTIES
	static constexpr float Z_NEAR = 10.f;
	static constexpr float Z_FAR = 20000.0f;
	static constexpr int m_frameCount = 2;
	static constexpr int m_windowWidth = 1920;
	static constexpr int m_windowHeight = 1080;
	static constexpr float m_aspectRatio = static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight);

	// DEBUG PROPERTIES
	bool FREEZE_CAMERA = false;
	bool DO_RAYTRACING = true;

	bool USE_SKYBOX = true;

	bool USE_AO_FRAME_JITTER = false;
	bool USE_AO_THIN_LENS = false;
	bool USE_GI_INDIRECT = false;
	
	bool RENDER_ONLY_RTAO = false;
	bool RENDER_LAMBERT = true;
	bool RENDER_GGX = false;
	
	bool SAMPLE_MJ = true;
	bool SAMPLE_UNIFORM = false;
	bool SAMPLE_RANDOM = false;

	// Frame data
	UINT64 m_currentCPUFrame = 0;
	UINT64 m_currentGPUFrame = 0;
	UINT64 m_currentFrameIdx = 0;
	std::shared_ptr<Profiler> m_profiler = nullptr;

	// Camera settings
	float m_cameraSpeed = 0.25f;
	XMFLOAT3 m_cameraPosition{ 0,0,0 };
	XMFLOAT3 m_cameraRotation{ 0,0,0 };
	XMFLOAT3 m_cameraPositionStoredInFrame{ 0,0,0 };

	// Light settings
	std::unique_ptr<LightSettings> m_lightSettings;

	// Random number generator
	std::uniform_real_distribution<float> m_rngDist;
	std::mt19937 m_rng;

	// Helper variables
	bool m_resetFrameAO = false;
	bool m_resetFrameGI = false;
	bool m_resetFrameProfiler = false;
	bool m_updateLightCount = false;
	UINT m_rtvDescriptorSize = 0;

	// Pipeline variables - device, commandQueue, swap chain
	int m_frameIndex = 0;
	ComPtr<ID3D12Device5> m_device					= NULL;
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
	ComPtr<ID3D12DescriptorHeap> m_heapPostprocess;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeapPostprocess;

	// Root signatures/PSO
	ComPtr<ID3D12RootSignature> m_rootSignature = NULL;
	ComPtr<ID3D12PipelineState> m_pipelineState = NULL;
	ComPtr<ID3D12RootSignature> m_rootSignatureSkybox = NULL;
	ComPtr<ID3D12PipelineState> m_pipelineStateSkybox = NULL;
	ComPtr<ID3D12RootSignature> m_rootSignaturePostprocess = NULL;
	ComPtr<ID3D12PipelineState> m_psoPostprocess = NULL;
#pragma endregion

#pragma region Resource variables
	// Depth stencil view (DSV)
	ComPtr<ID3D12Resource> m_depthStencil;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12Resource> m_depthBuffer = NULL;

	// Constant buffers
	CBuffer<SceneConstantBuffer> m_sceneBuffer;
	CBuffer<CameraConstantBuffer> m_cameraBuffer;
	CBuffer<AoConstantBuffer> m_aoBuffer;
	CBuffer<GiConstantBuffer> m_giBuffer;
	CBuffer<ConstantBufferStruct> m_constantBuffer;
	CBuffer<ConstantBufferStruct> m_constantBufferSkybox;
	CBuffer<LightConstantBuffer> m_lightBuffer;
	CBuffer<PostprocessConstantBuffer> m_postprocessBuffer;

	// Textures
	ComPtr<ID3D12Resource> m_backBuffers[m_frameCount];
	ComPtr<ID3D12Resource> m_pebblesTexture;
	ComPtr<ID3D12Resource> m_skyboxTexture;

	// Models
	std::shared_ptr<ModelClass> m_modelCube = NULL;
	std::shared_ptr<ModelClass> m_modelSphere = NULL;
	std::shared_ptr<ModelClass> m_modelBuddha = NULL;
	std::shared_ptr<ModelClass> m_modelPinkRoom = NULL;
	std::shared_ptr<ModelClass> m_modelFullscreen = NULL;
#pragma endregion
	
#pragma region Raytracing variables
	// Raytracing - resources containing all data for single raytracing shaders group
	std::shared_ptr<RaytracingResources> m_raytracingGBuffer = NULL;
	std::shared_ptr<RaytracingResources> m_raytracingAO = NULL;
	std::shared_ptr<RaytracingResources> m_raytracingLambert = NULL;
	std::shared_ptr<RaytracingResources> m_raytracingGGX = NULL;

	// DXR - output texture and descriptor heap of resources
	ComPtr<ID3D12Resource> m_rtAlbedoTexture = NULL;
	ComPtr<ID3D12Resource> m_rtNormalTexture = NULL;
	ComPtr<ID3D12Resource> m_rtSpecularTexture = NULL;
	ComPtr<ID3D12Resource> m_rtAoTexture = NULL;
	ComPtr<ID3D12Resource> m_rtLambertTexture = NULL;
	ComPtr<ID3D12Resource> m_rtGGXTexture = NULL;

	// Shader compiler
	D3D12ShaderCompilerInfo m_shaderCompiler{};
#pragma endregion

	// Synchronization
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[m_frameCount];
	HANDLE m_fenceEvent;
};
