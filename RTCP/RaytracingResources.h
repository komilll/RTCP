#pragma once
#ifndef _RAYTRACING_RESOURCES_H_
#define _RAYTRACING_RESOURCES_H_

#include "pch.h"
#include "ModelClass.h"
#include "RaytracingShadersHelper.h"
#include "CBuffer.h"
#include "BufferStructures.h"

typedef std::pair<ComPtr<ID3D12Resource>&, D3D12_UNORDERED_ACCESS_VIEW_DESC> TextureWithDesc;

class RaytracingResources
{
public:
	RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, LPCWSTR rayGenName, LPCWSTR missName, LPCWSTR hitName, LPCWSTR hitGroupName, LPCWSTR rayGenNameExport = L"RayGen", LPCWSTR missNameExport = L"Miss", LPCWSTR hitNameExport = L"Hit");
	RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, RtProgram rayGenShader, RtProgram missShader, HitProgram hitShader, LPCWSTR hitGroupName);

	//template <typename T1, typename T2>
	//void CreateRaytracingPipeline(ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, CBuffer<T1> cb1, CBuffer<T2> cb2);

	template <typename T>
	void CreateRaytracingPipeline(CBuffer<T> cb);

private:
	// Called in constructor
	void CreateBLAS(std::shared_ptr<ModelClass> model, ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, D3D12_RAYTRACING_GEOMETRY_FLAGS rayTracingFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
	void CreateTLAS(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, UINT tlasFlags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);

	// Called through CreateRaytracingPipeline
	void CreateShaderTable(ID3D12Device5* device);
	void CreateRTPSO(ID3D12Device5* device);
	template <class T1, class T2>
	void CreateDxrPipelineAssets(ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, T1 cb1, T2 cb2);

public:
	RtProgram m_rayGenShader;
	RtProgram m_missShader;
	HitProgram m_hitShader;
	LPCWSTR m_hitGroupName;

private:
	// Acceleration structure data
	ComPtr<ID3D12Resource> m_blasScratch = NULL;
	ComPtr<ID3D12Resource> m_blasResult = NULL;
	ComPtr<ID3D12Resource> m_tlasInstanceDesc = NULL;
	ComPtr<ID3D12Resource> m_tlasScratch = NULL;
	ComPtr<ID3D12Resource> m_tlasResult = NULL;

	ComPtr<ID3D12StateObject> m_rtpso = NULL;
	ComPtr<ID3D12StateObjectProperties> m_rtpsoInfo = NULL;
	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap = NULL;

	uint32_t m_shaderTableRecordSize;
	ComPtr<ID3D12Resource> m_shaderTable = NULL;
};
#endif // !_RAYTRACING_RESOURCES_H_

template<typename T>
inline void CreateRaytracingPipeline(CBuffer<T> cb)
{

}