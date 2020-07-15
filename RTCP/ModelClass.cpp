#include "ModelClass.h"

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

	localMesh.vertices[0].color = XMFLOAT3(0.0f, 0.0f, 1.0f);
	localMesh.vertices[1].color = XMFLOAT3(0.0f, 0.0f, 1.0f);
	localMesh.vertices[2].color = XMFLOAT3(0.0f, 0.0f, 1.0f);
	localMesh.vertices[3].color = XMFLOAT3(0.0f, 0.0f, 1.0f);
	localMesh.vertices[4].color = XMFLOAT3(0.0f, 0.0f, 1.0f);
	localMesh.vertices[5].color = XMFLOAT3(0.0f, 0.0f, 1.0f);

	//localMesh.vertices[0].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	//localMesh.vertices[1].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	//localMesh.vertices[2].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	//localMesh.vertices[3].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	//localMesh.vertices[4].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	//localMesh.vertices[5].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

	////Set UV values
	//{
	//	//First triangle	
	//	localMesh.vertices[0].uv = XMFLOAT2(0.0, 0.0);  // Top left.	
	//	localMesh.vertices[1].uv = XMFLOAT2(1.0, 1.0);  // Bottom right.
	//	localMesh.vertices[2].uv = XMFLOAT2(0.0, 1.0);  // Bottom left.
	//	//Second triangle
	//	localMesh.vertices[3].uv = XMFLOAT2(0.0, 0.0);  // Top left.
	//	localMesh.vertices[4].uv = XMFLOAT2(1.0, 0.0);  // Top right.
	//	localMesh.vertices[5].uv = XMFLOAT2(1.0, 1.0);  // Bottom right.
	//}

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
