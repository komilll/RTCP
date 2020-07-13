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
	typedef struct _instanceType {
		XMFLOAT3 color;
		XMFLOAT3 position;
	} InstanceType;

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

		VertexBufferStruct* vertices;
		unsigned long* indices;

		Mesh() = default;
		Mesh(VertexBufferStruct _vertices[], unsigned long _indices[]) : vertices{ _vertices }, indices{ _indices }
		{

		}
	};

	void CreateLine(ComPtr<ID3D12Device2> device, XMFLOAT3 start, XMFLOAT3 end);
	void CreatePlane(ComPtr<ID3D12Device2> device, XMFLOAT2 size);
	void CreateCube(ComPtr<ID3D12Device2> device, XMFLOAT3 min, XMFLOAT3 max);
	void SetFullScreenRectangleModel(ComPtr<ID3D12Device2> device, float left = -1.0f, float right = 1.0f, float top = 1.0f, float bottom = -1.0f);
	void LoadModel(std::string path, ComPtr<ID3D12Device2> device);
	Mesh GetMesh(int index) const { return m_meshes.at(index); };
	std::vector<Mesh> GetMeshes() const { return m_meshes; };
	//Return indices to render count
	//unsigned int Render(ID3D11DeviceContext* context);

	Bounds GetBounds(int meshIndex = 0);

private:
	void ProcessNode(aiNode* node, const aiScene* scene);
	Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);

	bool CreateRectangle(ComPtr<ID3D12Device2> device, float left, float right, float top, float bottom);

	bool PrepareBuffers(ComPtr<ID3D12Device2> device);

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
public:
	float m_scale = 1.0f;
	XMFLOAT3 m_position{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 m_rotation{ 0.0f, 0.0f, 0.0f };

private:
	std::vector<Mesh> m_meshes;
	//ID3D11Buffer* m_vertexBuffer = NULL;
	//ID3D11Buffer* m_indexBuffer = NULL;
	//ID3D11Buffer* m_instanceBuffer = NULL;
	unsigned int m_indexCount = 0;
	unsigned int m_instanceCount = 0;
	D3D_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};