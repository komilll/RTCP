#pragma once

#include "pch.h"
#include <vector>
#include <array>

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

using namespace DirectX;

class ModelClass
{
public:
	ModelClass() = default;
	ModelClass(std::string path, ComPtr<ID3D12Device2> device);

	struct VertexBufferStruct {
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT3 tangent;
		XMFLOAT3 binormal;
		XMFLOAT2 uv;
	};

	struct Bounds {
		float minX;
		float minY;
		float minZ;
		float maxX;
		float maxY;
		float maxZ;

		Bounds() = default;

		Bounds(XMFLOAT3 min_, XMFLOAT3 max_)
		{
			minX = min_.x;
			minY = min_.y;
			minZ = min_.z;
			maxX = max_.x;
			maxY = max_.y;
			maxZ = max_.z;
		}

		Bounds(float minX_, float minY_, float minZ_, float maxX_, float maxY_, float maxZ_)
		{
			minX = minX_;
			minY = minY_;
			minZ = minZ_;
			maxX = maxX_;
			maxY = maxY_;
			maxZ = maxZ_;
		}

		XMFLOAT3 GetCenter() const {
			return XMFLOAT3{ minX + (maxX - minX) * 0.5f, minY + (maxY - minY) * 0.5f, minZ + (maxZ - minZ) * 0.5f };
		}

		XMFLOAT3 GetSize() const {
			return XMFLOAT3{ maxX - minX, maxY - minY, maxZ - minZ };
		}

		XMFLOAT3 GetHalfSize() const {
			return XMFLOAT3{ (maxX - minX) * 0.5f, (maxY - minY) * 0.5f, (maxZ - minZ) * 0.5f };
		}

		float GetRadius() const {
			XMFLOAT3 size = GetHalfSize();
			return std::max(std::max(size.x, size.y), size.z);
		}
	};

	struct Mesh {
		unsigned int vertexCount;
		unsigned int indexCount;

		std::vector<VertexBufferStruct> vertices;
		std::vector<unsigned long> indices;
	};

	void SetFullScreenRectangleModel(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList, float left = -1.0f, float right = 1.0f, float top = 1.0f, float bottom = -1.0f, DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT);
	void LoadModel(std::string path, ComPtr<ID3D12Device2> device, DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT);
	Mesh GetMesh(int index) const { return m_meshes.at(index); };
	std::vector<Mesh> GetMeshes() const { return m_meshes; };

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return m_vertexBufferView; }
	ComPtr<ID3D12Resource> GetVertexBuffer() const { return m_vertexBuffer; }

	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return m_indexBufferView; }
	ComPtr<ID3D12Resource> GetIndexBuffer() const { return m_indexBuffer; }
	int GetIndicesCount() const { return m_indicesCount; }

private:
	void ProcessNode(aiNode* node, const aiScene* scene);
	Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);

	bool CreateRectangle(ComPtr<ID3D12Device2> device, float left, float right, float top, float bottom);

	bool PrepareBuffers(ComPtr<ID3D12Device2> device, DXGI_FORMAT indexFormat);

	void UpdateBufferResource(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	template <std::size_t N, typename T>
	std::vector<T> GetFirstIndices()
	{
		std::vector<T> vec(N);
		for (size_t i = 0; i < N; ++i)
			vec[i] = static_cast<T>(i);
		return vec;
	}
	template <std::size_t N, typename T>
	std::array<T, N> GetFirstIndicesArray()
	{
		std::array<T> arr(N);
		for (size_t i = 0; i < N; ++i)
			arr[i] = static_cast<T>(i);
		return arr;
	}

//VARIABLES
private:
	std::vector<Mesh> m_meshes;

	ComPtr<ID3D12Resource> m_vertexBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_indexBuffer = NULL;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	int m_indicesCount = 0;
};