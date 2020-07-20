#include "ModelClass.h"

void ModelClass::LoadModel(std::string path, ComPtr<ID3D12Device2> device)
{
	Assimp::Importer importer;
	char result[MAX_PATH];
	const std::string filename = path;
	path = std::string(result, GetModuleFileNameA(NULL, result, MAX_PATH));
	const auto index = path.find_last_of('\\');
	path = path.substr(0, index + 1) + filename;

	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_CalcTangentSpace | aiProcess_FlipUVs);

	assert(scene);
	ProcessNode(scene->mRootNode, scene);
	//assert(PrepareBuffers(device));
}

void ModelClass::ProcessNode(aiNode* node, const aiScene* scene)
{
	for (unsigned int i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		m_meshes.push_back(ProcessMesh(mesh, scene));
	}

	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		ProcessNode(node->mChildren[i], scene);
	}
}

ModelClass::Mesh ModelClass::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
	Mesh localMesh;
	localMesh.vertices.resize(mesh->mNumVertices);

	for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
	{
		//POSITION
		VertexBufferStruct vertex;
		vertex.position = XMFLOAT3{ mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

		//NORMAL
		if (mesh->mNormals) {
			vertex.normal = XMFLOAT3{ mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
		}
		else {
			assert(false);
		}

		//UV
		if (mesh->HasTextureCoords(0)) {
			vertex.uv.x = mesh->mTextureCoords[0][i].x;
			vertex.uv.y = mesh->mTextureCoords[0][i].y;
		}
		else {
			vertex.uv = XMFLOAT2{ 0,0 };
		}

		//TANGENT and BINORMAL
		if (!mesh->mBitangents || !mesh->mTangents)
		{
			if (i >= 2 && (i - 2) % 3 == 0)
			{
				//https://learnopengl.com/Advanced-Lighting/Normal-Mapping
				const XMFLOAT3 pos1 = localMesh.vertices[i - 2].position;
				const XMFLOAT3 pos2 = localMesh.vertices[i - 1].position;
				const XMFLOAT3 pos3 = localMesh.vertices[i].position;
				const XMFLOAT3 edge1 = XMFLOAT3{ pos2.x - pos1.x, pos2.y - pos1.y, pos2.z - pos1.z };
				const XMFLOAT3 edge2 = XMFLOAT3{ pos3.x - pos1.x, pos3.y - pos1.y, pos3.z - pos1.z };

				const XMFLOAT2 uv1 = localMesh.vertices[i - 2].uv;
				const XMFLOAT2 uv2 = localMesh.vertices[i - 1].uv;
				const XMFLOAT2 uv3 = localMesh.vertices[i].uv;
				const XMFLOAT2 deltaUV1 = XMFLOAT2{ uv2.x - uv1.x, uv2.y - uv1.y };
				const XMFLOAT2 deltaUV2 = XMFLOAT2{ uv3.x - uv1.x, uv3.y - uv1.y };

				const float denominator = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

				XMVECTOR tangent = XMVectorSet(denominator * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
					denominator * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
					denominator * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z),
					0.0f);
				tangent = XMVector3Normalize(tangent);

				XMVECTOR binormal = XMVectorSet(denominator * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),
					denominator * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),
					denominator * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z),
					0.0f);
				binormal = XMVector3Normalize(binormal);

				localMesh.vertices[i].tangent = XMFLOAT3{ tangent.m128_f32[0], tangent.m128_f32[1], tangent.m128_f32[2] };
				localMesh.vertices[i].binormal = XMFLOAT3{ binormal.m128_f32[0], binormal.m128_f32[1], binormal.m128_f32[2] };
			}
		}
		else
		{
			if (mesh->mBitangents) {
				vertex.binormal = XMFLOAT3{ mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
			}

			if (mesh->mTangents) {
				vertex.tangent = XMFLOAT3{ mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
			}
		}

		//Store in array
		localMesh.vertices[i].position = vertex.position;
		localMesh.vertices[i].uv = vertex.uv;
		localMesh.vertices[i].normal = vertex.normal;
		localMesh.vertices[i].binormal = vertex.binormal;
		localMesh.vertices[i].tangent = vertex.tangent;
	}

	unsigned int indicesCount = 0;
	for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
	{
		aiFace face = mesh->mFaces[i];
		indicesCount += face.mNumIndices;
	}
	localMesh.indices.resize(indicesCount);
	localMesh.indexCount = indicesCount;
	localMesh.vertexCount = mesh->mNumVertices;
	size_t currentIndexCounter = 0;

	for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; ++j)
		{
			localMesh.indices[currentIndexCounter++] = static_cast<unsigned long>(face.mIndices[j]);
		}
	}

	return localMesh;
}

bool ModelClass::CreateRectangle(ComPtr<ID3D12Device2> device, float left, float right, float top, float bottom)
{
	Mesh localMesh;
	constexpr size_t verticesCount = 6;

	localMesh.vertices.resize(verticesCount);
	localMesh.indices.resize(verticesCount);
	localMesh.vertexCount = verticesCount;
	localMesh.indexCount = verticesCount;
	for (size_t i = 0; i < verticesCount; ++i)
	{
		localMesh.indices[i] = static_cast<unsigned long>(i);
	}

	//First triangle
	localMesh.vertices[0].position = XMFLOAT3(left, top, 0.0f);  // Top left.	
	localMesh.vertices[1].position = XMFLOAT3(right, bottom, 0.0f);  // Bottom right.
	localMesh.vertices[2].position = XMFLOAT3(left, bottom, 0.0f);  // Bottom left.
	//Second triangle
	localMesh.vertices[3].position = XMFLOAT3(left, top, 0.0f);  // Top left.
	localMesh.vertices[4].position = XMFLOAT3(right, top, 0.0f);  // Top right.
	localMesh.vertices[5].position = XMFLOAT3(right, bottom, 0.0f);  // Bottom right.

	localMesh.vertices[0].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	localMesh.vertices[1].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	localMesh.vertices[2].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	localMesh.vertices[3].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	localMesh.vertices[4].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	localMesh.vertices[5].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

	//Set UV values
	{
		//First triangle	
		localMesh.vertices[0].uv = XMFLOAT2(0.0, 0.0);  // Top left.	
		localMesh.vertices[1].uv = XMFLOAT2(1.0, 1.0);  // Bottom right.
		localMesh.vertices[2].uv = XMFLOAT2(0.0, 1.0);  // Bottom left.
		//Second triangle
		localMesh.vertices[3].uv = XMFLOAT2(0.0, 0.0);  // Top left.
		localMesh.vertices[4].uv = XMFLOAT2(1.0, 0.0);  // Top right.
		localMesh.vertices[5].uv = XMFLOAT2(1.0, 1.0);  // Bottom right.
	}

	m_meshes.clear();
	m_meshes.push_back(localMesh);

	return true;
}

bool ModelClass::PrepareBuffers(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList> commandList)
{
	Mesh mesh = GetMesh(0);

	// Buffer which is used to CPU-GPU communication for vertex data
	ComPtr<ID3D12Resource> intermediateVertexBuffer = NULL;
	UpdateBufferResource(device, commandList, &m_vertexBuffer, &intermediateVertexBuffer, mesh.vertices.size(), sizeof(VertexBufferStruct), &mesh.vertices);

	// Create vertex buffer view
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = sizeof(&mesh.vertices);
	m_vertexBufferView.StrideInBytes = sizeof(VertexBufferStruct);

	// Index buffer
	ComPtr<ID3D12Resource> intermediateIndexBuffer = NULL;
	UpdateBufferResource(device, commandList, &m_indexBuffer, &intermediateIndexBuffer, mesh.indices.size(), sizeof(unsigned int), &mesh.indices);

	// Create index buffer view
	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_indexBufferView.SizeInBytes = sizeof(mesh.indices);

	return true;
}

void ModelClass::UpdateBufferResource(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
{
	size_t bufferSize = numElements * elementSize;
	{
		ThrowIfFailed(
			device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags), D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(pDestinationResource))
		);
	}

	// Make sure that passed data is not empty
	if (bufferData)
	{
		ThrowIfFailed(
			device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(bufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(pIntermediateResource))
		);

		D3D12_SUBRESOURCE_DATA subresourceData{};
		subresourceData.pData = bufferData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(commandList.Get(), *pDestinationResource, *pIntermediateResource, 0, 0, 1, &subresourceData);
	}
}

void ModelClass::SetFullScreenRectangleModel(ComPtr<ID3D12Device2> device, ComPtr<ID3D12GraphicsCommandList> commandList, float left, float right, float top, float bottom)
{
	assert(CreateRectangle(device, left, right, top, bottom) && "Failed to create full screen rectangle");
	assert(PrepareBuffers(device, commandList) && "Failed to prepare buffers");
}
