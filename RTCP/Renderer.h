#pragma once

#include "pch.h"
#include "DeviceManager.h"
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>
#include "ModelClass.h"
#include "BufferStructures.h"

#include "dxc/dxcapi.h"
#include "dxc/dxcapi.use.h"

using namespace DirectX;
typedef std::array<D3D12_INPUT_ELEMENT_DESC, 5> BasicInputLayout;

// DEFINES
#define ROOT_SIGNATURE_PIXEL D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;// | //D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

struct D3D12ShaderCompilerInfo
{
	dxc::DxcDllSupport		DxcDllHelper;
	IDxcCompiler* compiler = nullptr;
	IDxcLibrary* library = nullptr;
};

struct D3D12ShaderInfo
{
	LPCWSTR		filename = nullptr;
	LPCWSTR		entryPoint = nullptr;
	LPCWSTR		targetProfile = nullptr;
	LPCWSTR* arguments = nullptr;
	DxcDefine* defines = nullptr;
	UINT32		argCount = 0;
	UINT32		defineCount = 0;

	D3D12ShaderInfo() {}
	D3D12ShaderInfo(LPCWSTR inFilename, LPCWSTR inEntryPoint, LPCWSTR inProfile)
	{
		filename = inFilename;
		entryPoint = inEntryPoint;
		targetProfile = inProfile;
	}
};

struct RtProgram
{
	D3D12ShaderInfo			info = {};
	IDxcBlob* blob = nullptr;
	ID3D12RootSignature* pRootSignature = nullptr;

	D3D12_DXIL_LIBRARY_DESC	dxilLibDesc{};
	D3D12_EXPORT_DESC		exportDesc{};
	D3D12_STATE_SUBOBJECT	subobject{};
	std::wstring			exportName{};

	RtProgram()
	{
		exportDesc.ExportToRename = nullptr;
	}

	RtProgram(D3D12ShaderInfo shaderInfo)
	{
		info = shaderInfo;
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		exportName = shaderInfo.entryPoint;
		exportDesc.ExportToRename = nullptr;
		exportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
	}

	void SetBytecode()
	{
		exportDesc.Name = exportName.c_str();

		dxilLibDesc.NumExports = 1;
		dxilLibDesc.pExports = &exportDesc;
		dxilLibDesc.DXILLibrary.BytecodeLength = blob->GetBufferSize();
		dxilLibDesc.DXILLibrary.pShaderBytecode = blob->GetBufferPointer();

		subobject.pDesc = &dxilLibDesc;
	}

};

struct HitProgram
{
	RtProgram ahs;
	RtProgram chs;

	std::wstring exportName;
	D3D12_HIT_GROUP_DESC desc = {};
	D3D12_STATE_SUBOBJECT subobject = {};

	HitProgram() {}
	HitProgram(LPCWSTR name) : exportName(name)
	{
		desc = {};
		desc.HitGroupExport = exportName.c_str();
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		subobject.pDesc = &desc;
	}

	void SetExports(bool anyHit)
	{
		desc.HitGroupExport = exportName.c_str();
		if (anyHit) desc.AnyHitShaderImport = ahs.exportDesc.Name;
		desc.ClosestHitShaderImport = chs.exportDesc.Name;
	}

};

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
	void CreateTextureRTCP(ComPtr<ID3D12Resource>& texture, ComPtr<ID3D12GraphicsCommandList4> commandList, const wchar_t* path, ComPtr<ID3D12Resource>& uploadHeap);
	void CreateRootSignatureRTCP(UINT rootParamCount, UINT samplerCount, CD3DX12_ROOT_PARAMETER rootParameters[], CD3DX12_STATIC_SAMPLER_DESC samplers[], D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ComPtr<ID3D12RootSignature>& rootSignature);
	void CreateRootSignatureRTCP(UINT rootParamCount, UINT samplerCount, CD3DX12_ROOT_PARAMETER rootParameters[], CD3DX12_STATIC_SAMPLER_DESC samplers[], D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ID3D12RootSignature*& rootSignature);
	static void CreateSRV(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc);
	static void CreateSRV_Texture2D(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });
	static void CreateSRV_Texture3D(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE3D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });
	static void CreateSRV_TextureCube(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels = 1, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURECUBE, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING });

	// Helper functions, returning default descriptions/structres
	BasicInputLayout CreateBasicInputLayout();
	CD3DX12_DEPTH_STENCIL_DESC1 CreateDefaultDepthStencilDesc();
	D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateDefaultPSO(BasicInputLayout inputElementDescs, ComPtr<ID3DBlob> vertexShader, ComPtr<ID3DBlob> pixelShader, D3D12_DEPTH_STENCIL_DESC depthStencilDesc, ComPtr<ID3D12RootSignature> rootSignature);

	// Compiling shaders
	void InitShaderCompiler(D3D12ShaderCompilerInfo& shaderCompiler);
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, D3D12ShaderInfo& info, IDxcBlob** blob);
	void Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, RtProgram& program);

#pragma region Raytracing
	// Main function, invoked to prepare DXR pipeline
	void PrepareRaytracingResources();

	// Functions called by PrepareRaytracingResources()
	void CreateBLAS(std::shared_ptr<ModelClass> model);
	void CreateTLAS();
	void CreateDxrPipelineAssets(ModelClass* model);
	void CreateShaderTable();
	void CreateRTPSO();

	// Functions called by PrepareRaytracingResources() - prepare initial buffer values
	void InitializeRaytracingBufferValues();

	// Functions called by PrepareRaytracingResources() - generating shaders
	void CreateRayGenShader(D3D12ShaderCompilerInfo& shaderCompiler);
	void CreateMissShader(D3D12ShaderCompilerInfo& shaderCompiler);
	void CreateClosestHitShader(D3D12ShaderCompilerInfo& shaderCompiler);

#pragma endregion

	// Raytracing functions
	struct Viewport
	{
		float left;
		float top;
		float right;
		float bottom;
	};

	// Raytracing - Acceleration structures - BLAS
	ComPtr<ID3D12Resource> m_blasScratch = NULL;
	ComPtr<ID3D12Resource> m_blasResult  = NULL;

	// Raytracing - Acceleration structures - TLAS
	ComPtr<ID3D12Resource> m_tlasInstanceDesc = NULL;
	ComPtr<ID3D12Resource> m_tlasScratch	  = NULL;
	ComPtr<ID3D12Resource> m_tlasResult		  = NULL;
	UINT64 m_tlasSize;

	// DXR - output texture and descriptor heap of resources
	ComPtr<ID3D12Resource> m_dxrOutput = NULL;
	ComPtr<ID3D12DescriptorHeap> m_dxrDescriptorHeap = NULL;

	// Shader compiler
	D3D12ShaderCompilerInfo m_shaderCompiler{};

	// RT shaders, RT shader table
	RtProgram m_rayGenShader;
	RtProgram m_missShader;
	HitProgram m_hitShader;
	uint32_t m_shaderTableRecordSize;
	ComPtr<ID3D12Resource> m_shaderTable;

	// RTPSO
	ComPtr<ID3D12StateObject> m_rtpso				= NULL;
	ComPtr<ID3D12StateObjectProperties> m_rtpsoInfo = NULL;

public:
	bool DO_RAYTRACING = false;

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

	// Camera settings
	XMFLOAT3 m_cameraPosition{ 0,0,0 };
	XMFLOAT3 m_cameraRotation{ 0,0,0 };
	XMFLOAT3 m_cameraPositionStoredInFrame{ 0,0,0 };

	SceneConstantBuffer m_sceneBufferData;
	CubeConstantBuffer m_cubeBufferData;
	ComPtr<ID3D12Resource> m_sceneBuffer = NULL;
	UINT8* m_sceneBufferDataBegin = nullptr;

	ComPtr<ID3D12Resource> m_cubeBuffer = NULL;
	UINT8* m_cubeBufferDataBegin = nullptr;

	ComPtr<ID3D12Device5> m_device = NULL;

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
	UINT m_rtvDescriptorSize = 0;

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

	std::shared_ptr<ModelClass> m_previewModel = NULL;
	std::shared_ptr<ModelClass> m_modelCube = NULL;
	std::shared_ptr<ModelClass> m_modelSphere = NULL;

	bool m_contentLoaded = false;

	// Synchronization
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[m_frameCount];
	HANDLE m_fenceEvent;
};