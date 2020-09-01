#pragma once
#ifndef _RAYTRACING_RESOURCES_H_
#define _RAYTRACING_RESOURCES_H_

#include "pch.h"
#include "ModelClass.h"
#include "RaytracingShadersHelper.h"
#include "CBuffer.h"
#include "BufferStructures.h"

struct TextureWithDesc
{
public:
	TextureWithDesc(ComPtr<ID3D12Resource>& resource_, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_)
	{
		resource = resource_;
		isSRV = true;
		srvDesc = srvDesc_;
	};

	TextureWithDesc(ComPtr<ID3D12Resource>& resource_, D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc_)
	{
		resource = resource_;
		isSRV = false;
		uavDesc = uavDesc_;
	};

	ComPtr<ID3D12Resource> resource;
	bool isSRV;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
};

typedef std::pair<ComPtr<ID3D12Resource>, std::size_t> ResourceWithSize;

class RaytracingResources
{
public:
	RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, LPCWSTR rayGenName, LPCWSTR missName, LPCWSTR hitName, LPCWSTR hitGroupName, LPCWSTR rayGenNameExport = L"RayGen", LPCWSTR missNameExport = L"Miss", LPCWSTR hitNameExport = L"Hit");
	RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, RtProgram rayGenShader, RtProgram missShader, HitProgram hitShader, LPCWSTR hitGroupName);
	RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, HitGroup hitGroup);
	RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, std::vector<HitGroup> hitGroups);

    void CreateRaytracingPipelineContinue(ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, std::vector< ResourceWithSize> buffersWithSize, std::vector<bool> isUAV, size_t maxPayloadSize);
	ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() const { return m_descriptorHeap; };
	ComPtr<ID3D12Resource> GetShaderTable() const { return m_shaderTable; };
	uint32_t GetShaderTableRecordSize() const { return m_shaderTableRecordSize; };
	ComPtr<ID3D12StateObject> GetRTPSO() const { return m_rtpso; };
	D3D12_DISPATCH_RAYS_DESC GetDispatchRaysDesc(UINT width, UINT height, UINT depth) const;

private:
	// Called in constructor
	void CreateBLAS(std::shared_ptr<ModelClass> model, ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, D3D12_RAYTRACING_GEOMETRY_FLAGS rayTracingFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
	void CreateTLAS(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, UINT tlasFlags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);

	// Called through CreateRaytracingPipeline
	void CreateDxrPipelineAssets(ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, std::vector<ResourceWithSize> buffersWithSize, std::vector<bool> isUAV);
	void CreateShaderTable(ID3D12Device5* device);
	void CreateRTPSO(ID3D12Device5* device, size_t maxPayloadSize);

public:
	//RtProgram m_rayGenShader;
	//RtProgram m_missShader;
	//HitProgram m_hitShader;
	//LPCWSTR m_hitGroupName;

	std::vector<HitGroup> m_hitGroups;

private:
	// Acceleration structure data
	ComPtr<ID3D12Resource> m_blasScratch = NULL;
	ComPtr<ID3D12Resource> m_blasResult = NULL;
	ComPtr<ID3D12Resource> m_tlasInstanceDesc = NULL;
	ComPtr<ID3D12Resource> m_tlasScratch = NULL;
	ComPtr<ID3D12Resource> m_tlasResult = NULL;

	// RTPSO data
	ComPtr<ID3D12StateObject> m_rtpso = NULL;
	ComPtr<ID3D12StateObjectProperties> m_rtpsoInfo = NULL;
	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap = NULL;

	// Shader table data
	uint32_t m_shaderTableRecordSize;
	ComPtr<ID3D12Resource> m_shaderTable = NULL;
	struct ShaderTableStruct {
		// Structure is based on assumption of order: rgs - miss - hit
		uint32_t raygenSize;
		uint32_t missSize;
		uint32_t missStride;
		uint32_t hitSize;
		uint32_t hitStride;
	} m_shaderTableStruct;
};
#endif // !_RAYTRACING_RESOURCES_H_

template<typename T1, typename T2, typename T3>
inline void CreateRaytracingPipeline(RaytracingResources* raytracingResources, ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, CBuffer<T1>& cb1, CBuffer<T2>& cb2, CBuffer<T3>& cb3, std::array<bool, 3> isUAV = {}, size_t maxPayloadSize = sizeof(float) * 4)
{
	CreateUploadBuffers(raytracingResources, device, model, texturesWithDesc, indexDesc, vertexDesc, cb1, cb2, cb3, isUAV, maxPayloadSize);
};

template<typename T1, typename T2>
inline void CreateRaytracingPipeline(RaytracingResources* raytracingResources, ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, CBuffer<T1>& cb1, CBuffer<T2>& cb2, std::array<bool, 2> isUAV = {}, size_t maxPayloadSize = sizeof(float) * 4)
{
	CreateUploadBuffers(raytracingResources, device, model, texturesWithDesc, indexDesc, vertexDesc, cb1, cb2, isUAV, maxPayloadSize);
}

template <typename T1, typename T2>
inline void CreateUploadBuffers(RaytracingResources* raytracingResources, ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, CBuffer<T1>& cb1, CBuffer<T2>& cb2, std::array<bool, 2> isUAV, size_t maxPayloadSize)
{
    // Create view buffer and material buffer
    {
        CreateUploadHeapRTCP(device, cb1);
        CreateUploadHeapRTCP(device, cb2);
    }

	std::vector<ResourceWithSize> v{};
	v.push_back({ cb1.resource, sizeof(cb1.value) });
	v.push_back({ cb2.resource, sizeof(cb2.value) });

	raytracingResources->CreateRaytracingPipelineContinue(device, model, texturesWithDesc, indexDesc, vertexDesc, v, std::vector<bool>{isUAV.begin(), isUAV.end()}, maxPayloadSize);
}

template <typename T1, typename T2, typename T3>
inline void CreateUploadBuffers(RaytracingResources* raytracingResources, ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, CBuffer<T1>& cb1, CBuffer<T2>& cb2, CBuffer<T3>& cb3, std::array<bool, 3> isUAV, size_t maxPayloadSize)
{
	// Create view buffer and material buffer
	{
		CreateUploadHeapRTCP(device, cb1);
		CreateUploadHeapRTCP(device, cb2);
		CreateUploadHeapRTCP(device, cb3);
	}

	std::vector<ResourceWithSize> v{};
	v.push_back({ cb1.resource, sizeof(cb1.value) });
	v.push_back({ cb2.resource, sizeof(cb2.value) });
	v.push_back({ cb3.resource, sizeof(cb3.value) });

	raytracingResources->CreateRaytracingPipelineContinue(device, model, texturesWithDesc, indexDesc, vertexDesc, v, std::vector<bool>{isUAV.begin(), isUAV.end()}, maxPayloadSize);
}