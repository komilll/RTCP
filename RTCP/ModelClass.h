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
	ModelClass(std::string path, ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList);
	ModelClass(std::string path, ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList, ComPtr<ID3D12Resource>& uploadHeap);

	struct Texture {
		std::string type;
		std::string path;
		ComPtr<ID3D12Resource> resource;
		D3D12_SUBRESOURCE_DATA subresourceData;
	};

	struct VertexBufferStruct {
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT3 tangent;
		XMFLOAT3 binormal;
		XMFLOAT2 uv;
		unsigned int textureID;
	};

	struct Bounds {
		float minX, minY, minZ;
		float maxX, maxY, maxZ;

		Bounds() = default;
		Bounds(XMFLOAT3 min_, XMFLOAT3 max_) {
			minX = min_.x; minY = min_.y; minZ = min_.z;
			maxX = max_.x; maxY = max_.y; maxZ = max_.z;
		}
		Bounds(float minX_, float minY_, float minZ_, float maxX_, float maxY_, float maxZ_)
		{
			minX = minX_; minY = minY_; minZ = minZ_;
			maxX = maxX_; maxY = maxY_; maxZ = maxZ_;
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

	// Creating or loading new model
	void SetFullScreenRectangleModel(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList, float left = -1.0f, float right = 1.0f, float top = 1.0f, float bottom = -1.0f, DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT);
	void LoadModel(std::string path, ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList, DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT);
	void LoadModel(std::string path, ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList, ComPtr<ID3D12Resource>& uploadHeap, DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT);
	void ProcessScene(std::string path, ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList);

	// Get meshes
	Mesh GetMesh(int index) const { return m_meshes.at(index); };
	std::vector<Mesh> GetMeshes() const { return m_meshes; };

	// Get textures
	ComPtr<ID3D12Resource>& GetTextureResources() { return m_resource; };
	ComPtr<ID3D12Resource>& GetTextureResource(int index) { return m_textures.at(index).resource; };
	Texture GetTexture(int index) const { return m_textures.at(index); };
	std::vector<Texture> GetTextures() const { return m_textures; };

	// Get vertex buffer data
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return m_vertexBufferView; }
	ComPtr<ID3D12Resource> GetVertexBuffer() const { return m_vertexBuffer; }

	// Get index buffer data
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return m_indexBufferView; }
	ComPtr<ID3D12Resource> GetIndexBuffer() const { return m_indexBuffer; }
	int GetIndicesCount() const { return m_indicesCount; }
	int GetVerticesCount() const { return m_verticesCount; }

private:
	// Processing data by assimp
	void ProcessNode(std::vector<Mesh>& meshes, aiNode* node, const aiScene* scene, ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList);
	Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, unsigned int textureID, ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList);
	std::string DetermineTextureType(const aiScene* scene, aiMaterial* mat);
	std::vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene, ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList, int index);
	int GetTextureIndex(aiString* str);
	std::pair<ComPtr<ID3D12Resource>, D3D12_SUBRESOURCE_DATA> GetTextureFromModel(const aiScene* scene, std::string filename, ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList, int index);

	// Internal functions - creating shapes
	bool CreateRectangle(ComPtr<ID3D12Device2> device, float left, float right, float top, float bottom);

	// Modifying vertex/index buffer, stored in class
	bool PrepareBuffers(ComPtr<ID3D12Device2> device, DXGI_FORMAT indexFormat);
	void UpdateBufferResource(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList4> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

//VARIABLES
private:
	int m_correctCount = 0;
	ComPtr<ID3D12Resource> m_resource;
	std::vector<ComPtr<ID3D12Resource>> m_uploadHeaps{};

	std::vector<Mesh> m_meshes;
	std::vector<Texture> m_textures;

	ComPtr<ID3D12Resource> m_textureBuffer = NULL;

	ComPtr<ID3D12Resource> m_vertexBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_indexBuffer = NULL;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	int m_indicesCount = 0;
	int m_verticesCount = 0;
};