#include "RaytracingResources.h"

RaytracingResources::RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, LPCWSTR rayGenName, LPCWSTR missName, LPCWSTR hitName, LPCWSTR hitGroupName, LPCWSTR rayGenNameExport, LPCWSTR missNameExport, LPCWSTR hitNameExport)
{
    HitGroup hitGroup;
    hitGroup.rayGenShader.name = rayGenName;
    hitGroup.rayGenShader.exportToRename = rayGenNameExport;
    hitGroup.missShader.name = missName;
    hitGroup.missShader.exportToRename = missNameExport;
    hitGroup.hitShader.name = hitName;
    hitGroup.hitShader.exportToRename = hitNameExport;
    hitGroup.hitGroupName = hitGroupName;

    m_hitGroups.push_back(hitGroup);

    CreateBLAS(model, device, commandList);
    CreateTLAS(device, commandList);
}

RaytracingResources::RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, RtProgram rayGenShader, RtProgram missShader, HitProgram hitShader, LPCWSTR hitGroupName)
{
    HitGroup hitGroup;
    hitGroup.rayGenShader = rayGenShader;
    hitGroup.missShader = missShader;
    hitGroup.hitShader = hitShader;
    hitGroup.hitGroupName = hitGroupName;

    m_hitGroups.push_back(hitGroup);

    CreateBLAS(model, device, commandList);
    CreateTLAS(device, commandList);
}

RaytracingResources::RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, HitGroup hitGroup)
{
    m_hitGroups.push_back(hitGroup);

    CreateBLAS(model, device, commandList);
    CreateTLAS(device, commandList);
}

RaytracingResources::RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, std::vector<HitGroup> hitGroups)
{
    m_hitGroups = hitGroups;

    CreateBLAS(model, device, commandList);
    CreateTLAS(device, commandList);
}

void RaytracingResources::CreateRaytracingPipelineContinue(ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, std::vector<ResourceWithSize> buffersWithSize, std::vector<bool> isUAV, size_t maxPayloadSize)
{
    CreateDxrPipelineAssets(device, model, texturesWithDesc, indexDesc, vertexDesc, buffersWithSize, isUAV);
    CreateRTPSO(device, maxPayloadSize);
    CreateShaderTable(device);
}

D3D12_DISPATCH_RAYS_DESC RaytracingResources::GetDispatchRaysDesc(UINT width, UINT height, UINT depth) const
{
    D3D12_DISPATCH_RAYS_DESC desc = {};
    // rgs
    desc.RayGenerationShaderRecord.StartAddress = m_shaderTable->GetGPUVirtualAddress();
    desc.RayGenerationShaderRecord.SizeInBytes = m_shaderTableStruct.raygenSize;

    // hit
    desc.HitGroupTable.StartAddress = m_shaderTable->GetGPUVirtualAddress() + m_shaderTableStruct.raygenSize;
    desc.HitGroupTable.SizeInBytes = m_shaderTableStruct.hitSize;
    desc.HitGroupTable.StrideInBytes = m_shaderTableStruct.hitStride;

    // miss
    desc.MissShaderTable.StartAddress = m_shaderTable->GetGPUVirtualAddress() + m_shaderTableStruct.raygenSize + m_shaderTableStruct.hitSize;
    desc.MissShaderTable.SizeInBytes = m_shaderTableStruct.missSize;
    desc.MissShaderTable.StrideInBytes = m_shaderTableStruct.missStride;

    desc.Width = width;
    desc.Height = height;
    desc.Depth = depth;

    return desc;
}

void RaytracingResources::CreateBLAS(std::shared_ptr<ModelClass> model, ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, D3D12_RAYTRACING_GEOMETRY_FLAGS rayTracingFlags, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
    // Describe the geometry that goes in the bottom acceleration structure(s)
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.VertexBuffer.StartAddress = model->GetVertexBuffer()->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = model->GetVertexBufferView().StrideInBytes;
    geometryDesc.Triangles.VertexCount = static_cast<UINT>(model->GetMesh(0).vertices.size());
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.IndexBuffer = model->GetIndexBuffer()->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexFormat = model->GetIndexBufferView().Format;
    geometryDesc.Triangles.IndexCount = static_cast<UINT>(model->GetIndicesCount());
    geometryDesc.Triangles.Transform3x4 = 0;
    geometryDesc.Flags = rayTracingFlags;

    // Get the size requirements for the BLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.pGeometryDescs = &geometryDesc;
    ASInputs.NumDescs = 1;
    ASInputs.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);
    ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);

    // Create the BLAS scratch buffer
    auto bufferInfo = CD3DX12_RESOURCE_DESC::Buffer(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &bufferInfo,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_blasScratch)
    ));

    // Create the BLAS buffer
    bufferInfo = CD3DX12_RESOURCE_DESC::Buffer(ASPreBuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &bufferInfo,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nullptr,
        IID_PPV_ARGS(&m_blasResult)
    ));

    // Describe and build the bottom level acceleration structure
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = ASInputs;
    buildDesc.ScratchAccelerationStructureData = m_blasScratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = m_blasResult->GetGPUVirtualAddress();

    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the BLAS build to complete
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_blasResult.Get()));
}

void RaytracingResources::CreateTLAS(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, UINT tlasFlags, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
    // Describe the TLAS geometry instance(s)
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    instanceDesc.InstanceID = 0;
    instanceDesc.InstanceContributionToHitGroupIndex = 0;
    instanceDesc.InstanceMask = 0xFF;
    instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
    instanceDesc.AccelerationStructure = m_blasResult->GetGPUVirtualAddress();
    instanceDesc.Flags = tlasFlags;

    // Create the TLAS instance buffer
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(instanceDesc)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_tlasInstanceDesc)
    ));

    // Copy the instance data to the buffer
    UINT8* pData;
    m_tlasInstanceDesc->Map(0, nullptr, (void**)&pData);
    memcpy(pData, &instanceDesc, sizeof(instanceDesc));
    m_tlasInstanceDesc->Unmap(0, nullptr);

    // Get the size requirements for the TLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.InstanceDescs = m_tlasInstanceDesc->GetGPUVirtualAddress();
    ASInputs.NumDescs = 1;
    ASInputs.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
    ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

    // Set TLAS size
    //m_tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

    // Create TLAS scratch buffer
    auto bufferInfo = CD3DX12_RESOURCE_DESC::Buffer(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &bufferInfo,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_tlasScratch)
    ));

    bufferInfo = CD3DX12_RESOURCE_DESC::Buffer(ASPreBuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &bufferInfo,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nullptr,
        IID_PPV_ARGS(&m_tlasResult)
    ));

    // Describe and build the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = ASInputs;
    buildDesc.ScratchAccelerationStructureData = m_tlasScratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = m_tlasResult->GetGPUVirtualAddress();

    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the TLAS build to complete
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_tlasResult.Get()));
}

void RaytracingResources::CreateDxrPipelineAssets(ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, std::vector<ResourceWithSize> buffersWithSize, std::vector<bool> isUAV)
{
    // Create descriptor heaps
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        // Vertex + index + TLAS = 3
        // Buffer count = y // buffersWithSize
        // Number of textures = x // texturesWithDesc
        desc.NumDescriptors = 3 + static_cast<UINT>(buffersWithSize.size()) + static_cast<UINT>(texturesWithDesc.size());
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        // Create the descriptor heap
        ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeap)));

        // Get the descriptor heap handle and increment size
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        UINT handleIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Create the CBV
        if (buffersWithSize.size() > 0)
        {
            bool firstPassed = false;

            // Create CBVs one by one
            for (int i = 0; i < buffersWithSize.size(); ++i)
            {
                // We follow CBV-UAV-SRV creation order
                if (!isUAV[i])
                {
                    // Increase handler pointer only after creation of any SRV
                    if (firstPassed) {
                        handle.ptr += handleIncrement;
                    }

                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { buffersWithSize[i].resource->GetGPUVirtualAddress(), ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, static_cast<UINT>(buffersWithSize[i].size)) };
                    device->CreateConstantBufferView(&cbvDesc, handle);
                    firstPassed = true;
                }
            }
        }

        // Create the DXR output buffer UAV
        for (auto& tex : texturesWithDesc)
        {
            if (!tex.isSRV)
            {
                handle.ptr += handleIncrement;
                device->CreateUnorderedAccessView(tex.resource.Get(), nullptr, &tex.uavDesc, handle);
            }
        }
        // Create buffer UAVs from CBuffers
        for (int i = 0; i < buffersWithSize.size(); ++i)
        {
            if (isUAV[i])
            {
                handle.ptr += handleIncrement;
                //D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                //uavDesc.Format = DXGI_FORMAT_UNKNOWN;
                //uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                //uavDesc.Buffer.FirstElement = 0;
                //uavDesc.Buffer.NumElements = 2;
                //uavDesc.Buffer.StructureByteStride = sizeof(buffersWithSize[i].first);
                //uavDesc.Buffer.CounterOffsetInBytes = 0;
                //uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
                
                UINT size = static_cast<UINT>(buffersWithSize[i].size);
                UINT64 bufferSize = size;
                auto uavDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
                auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

                ThrowIfFailed(device->CreateCommittedResource(
                    &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&buffersWithSize[i].resource)
                ));

                D3D12_UNORDERED_ACCESS_VIEW_DESC uavViewDesc = {};
                uavViewDesc.Buffer.NumElements = 1;
                uavViewDesc.Buffer.FirstElement = 0;
                uavViewDesc.Buffer.StructureByteStride = size;
                uavViewDesc.Buffer.CounterOffsetInBytes = 0;
                uavViewDesc.Format = DXGI_FORMAT_UNKNOWN;
                uavViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

                device->CreateUnorderedAccessView(buffersWithSize[i].resource.Get(), nullptr, &uavViewDesc, handle);
            }
        }

        // Create the DXR Top Level Acceleration Structure SRV
        handle.ptr += handleIncrement;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = m_tlasResult->GetGPUVirtualAddress();
        device->CreateShaderResourceView(nullptr, &srvDesc, handle);

        // Create the index buffer SRV
        handle.ptr += handleIncrement;
        device->CreateShaderResourceView(indexDesc.Buffer.NumElements == 0 ? nullptr : model->GetIndexBuffer().Get(), &indexDesc, handle);

        // Create the vertex buffer SRV
        handle.ptr += handleIncrement;
        device->CreateShaderResourceView(vertexDesc.Buffer.NumElements == 0 ? nullptr : model->GetVertexBuffer().Get(), &vertexDesc, handle);

        // Create texture buffer SRV
        for (auto& tex : texturesWithDesc)
        {
            if (tex.isSRV)
            {
                handle.ptr += handleIncrement;
                device->CreateShaderResourceView(tex.resource.Get(), &tex.srvDesc, handle);
            }
        }
    }
}

void RaytracingResources::CreateShaderTable(ID3D12Device5* device)
{
    /*
The Shader Table layout is as follows:
    Entry 0 - Ray Generation shader
    Entry 1 - Miss shader
    Entry 2 - Closest Hit shader
All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
The ray generation program requires the largest entry:
    32 bytes - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
  +  8 bytes - a CBV/SRV/UAV descriptor table pointer (64-bits)
  = 40 bytes ->> aligns to 64 bytes
The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
*/
// Bindings based on - http://www.realtimerendering.com/raytracinggems/unofficial_RayTracingGems_v1.7.pdf#page=82&zoom=160,-147,633

    // Set struct data
    m_shaderTableStruct.raygenSize = 64;
    m_shaderTableStruct.missStride = 32;
    m_shaderTableStruct.hitStride = 64;
    m_shaderTableStruct.missSize = m_shaderTableStruct.missStride * static_cast<uint32_t>(m_hitGroups.size());
    m_shaderTableStruct.hitSize = m_shaderTableStruct.hitStride * static_cast<uint32_t>(m_hitGroups.size());

    uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    uint32_t shaderTableSize = 0;

    m_shaderTableRecordSize = shaderIdSize;
    m_shaderTableRecordSize += 8;							// CBV/SRV/UAV descriptor table
    m_shaderTableRecordSize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, m_shaderTableRecordSize);

    shaderTableSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, m_shaderTableStruct.raygenSize + m_shaderTableStruct.missSize + m_shaderTableStruct.hitSize);

    // Create the shader table buffer
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(shaderTableSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_shaderTable)
    ));

    // Map the buffer
    uint8_t* pData;
    ThrowIfFailed(m_shaderTable->Map(0, nullptr, (void**)&pData));

    // Ray Generation program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
    memcpy(pData, m_rtpsoInfo->GetShaderIdentifier(m_hitGroups[0].rayGenShader.name), shaderIdSize);
    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // Closest Hit program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
    pData += m_shaderTableStruct.raygenSize;
    for (int i = 0; i < m_hitGroups.size(); ++i)
    {
        memcpy(pData, m_rtpsoInfo->GetShaderIdentifier(m_hitGroups[i].hitGroupName), shaderIdSize);
        pData += shaderIdSize;
        *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData) = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
        pData += shaderIdSize;
    }

    // Miss program (no local root arguments to set)
    for (int i = 0; i < m_hitGroups.size(); ++i) 
    {
        memcpy(pData, m_rtpsoInfo->GetShaderIdentifier(m_hitGroups[i].missShader.name), shaderIdSize);
        pData += shaderIdSize;
    }

    // Unmap
    m_shaderTable->Unmap(0, nullptr);
}

void RaytracingResources::CreateRTPSO(ID3D12Device5* device, size_t maxPayloadSize)
{
    // Need 11 subobjects:
    // 1 for RGS program
    // 1 for Miss program
    // 1 for CHS program
    // 1 for AHS program
    // 1 for Hit Group
    // 2 for RayGen Root Signature (root-signature and association)
    // 2 for Shader Config (config and association)
    // 1 for Global Root Signature
    // 1 for Pipeline Config	
    UINT index = 0;
    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    size_t size = 0;
    size += 1; // rgs
    for (auto& hitGroup : m_hitGroups)
    {
        size += 2; // chs, miss
        size += (hitGroup.hitShader.ahs.name != nullptr) ? 1 : 0; // ahs
        size += 1; // hit group
        size += 2; // payload + association
        size += 2; // local root + association
    }
    size += 3; // global root + local root + pipeline config
    subobjects.resize(size);

    ID3D12RootSignature* dummyLocalRootSignature;
    ID3D12RootSignature* dummyGlobalRootSignature;
    // Create dummy local root signature
    {
        HRESULT hr = 0;
        D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.NumParameters = 0;
        rootDesc.pParameters = nullptr;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        ID3DBlob* serializedRootSignature;
        ID3DBlob* error;

        // Create the global root signature
        ThrowIfFailed(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRootSignature, &error));

        ThrowIfFailed(device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(),
            serializedRootSignature->GetBufferSize(),
            IID_PPV_ARGS(&dummyGlobalRootSignature)));

        serializedRootSignature->Release();

        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        // Create the local root signature, reusing the same descriptor but altering the creation flag
        ThrowIfFailed(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRootSignature, &error));

        ThrowIfFailed(device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(),
            serializedRootSignature->GetBufferSize(),
            IID_PPV_ARGS(&dummyLocalRootSignature)));

        serializedRootSignature->Release();
    }

#pragma region HitGroup creation - 1
    {
        HitGroup hitGroup = m_hitGroups[0];
        // Add state subobject for the RGS
        {
            D3D12_EXPORT_DESC rgsExportDesc = { hitGroup.rayGenShader.name, nullptr, D3D12_EXPORT_FLAG_NONE };

            D3D12_DXIL_LIBRARY_DESC	rgsLibDesc = {};
            rgsLibDesc.DXILLibrary.BytecodeLength = hitGroup.rayGenShader.blob->GetBufferSize();
            rgsLibDesc.DXILLibrary.pShaderBytecode = hitGroup.rayGenShader.blob->GetBufferPointer();
            rgsLibDesc.NumExports = 1;
            rgsLibDesc.pExports = &rgsExportDesc;

            D3D12_STATE_SUBOBJECT rgs = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &rgsLibDesc };
            subobjects[index++] = rgs;
        }

        //Add state subobject for the Miss shader
        {
            D3D12_EXPORT_DESC msExportDesc = { hitGroup.missShader.name, hitGroup.missShader.exportToRename, D3D12_EXPORT_FLAG_NONE };

            D3D12_DXIL_LIBRARY_DESC	msLibDesc = {};
            msLibDesc.DXILLibrary.BytecodeLength = hitGroup.missShader.blob->GetBufferSize();
            msLibDesc.DXILLibrary.pShaderBytecode = hitGroup.missShader.blob->GetBufferPointer();
            msLibDesc.NumExports = 1;
            msLibDesc.pExports = &msExportDesc;

            D3D12_STATE_SUBOBJECT ms = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &msLibDesc };
            subobjects[index++] = ms;
        }

        // Add state subobject for the Closest Hit shader
        {
            D3D12_EXPORT_DESC chsExportDesc = { hitGroup.hitShader.chs.name, hitGroup.hitShader.chs.exportToRename, D3D12_EXPORT_FLAG_NONE };

            D3D12_DXIL_LIBRARY_DESC	chsLibDesc = {};
            chsLibDesc.DXILLibrary.BytecodeLength = hitGroup.hitShader.chs.blob->GetBufferSize();
            chsLibDesc.DXILLibrary.pShaderBytecode = hitGroup.hitShader.chs.blob->GetBufferPointer();
            chsLibDesc.NumExports = 1;
            chsLibDesc.pExports = &chsExportDesc;

            D3D12_STATE_SUBOBJECT chs = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &chsLibDesc };
            subobjects[index++] = chs;
        }

        // Add state subobject for the Any Hit shader
        if (hitGroup.hitShader.ahs.name != nullptr)
        {
            D3D12_EXPORT_DESC ahsExportDesc = { hitGroup.hitShader.ahs.name, hitGroup.hitShader.ahs.exportToRename, D3D12_EXPORT_FLAG_NONE };

            D3D12_DXIL_LIBRARY_DESC	ahsLibDesc = {};
            ahsLibDesc.DXILLibrary.BytecodeLength = hitGroup.hitShader.ahs.blob->GetBufferSize();
            ahsLibDesc.DXILLibrary.pShaderBytecode = hitGroup.hitShader.ahs.blob->GetBufferPointer();
            ahsLibDesc.NumExports = 1;
            ahsLibDesc.pExports = &ahsExportDesc;

            D3D12_STATE_SUBOBJECT ahs = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &ahsLibDesc };
            subobjects[index++] = ahs;
        }

        // Add a state subobject for the hit group
        {
            D3D12_HIT_GROUP_DESC hitGroupDesc = {};
            hitGroupDesc.ClosestHitShaderImport = hitGroup.hitShader.chs.name;
            if (hitGroup.hitShader.ahs.name != nullptr) hitGroupDesc.AnyHitShaderImport = hitGroup.hitShader.ahs.name;
            hitGroupDesc.HitGroupExport = hitGroup.hitGroupName;

            D3D12_STATE_SUBOBJECT hitGroup = { D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroupDesc };
            subobjects[index++] = hitGroup;
        }

        // Add a state subobject for the shader payload configuration
        {
            D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = { static_cast<UINT>(maxPayloadSize), D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES };
            D3D12_STATE_SUBOBJECT shaderConfigObject = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderDesc };
            subobjects[index++] = shaderConfigObject;
        }

        // Create a list of the shader export names that use the payload
        {
            D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
            if (hitGroup.hitShader.ahs.name != nullptr) {
                const WCHAR* shaderExports[] = { hitGroup.rayGenShader.name, hitGroup.missShader.name, hitGroup.hitShader.chs.name, hitGroup.hitShader.ahs.name };
                shaderPayloadAssociation = { &subobjects[(index - 1)], _countof(shaderExports), shaderExports };
            }
            else {
                const WCHAR* shaderExports[] = { hitGroup.rayGenShader.name, hitGroup.missShader.name, hitGroup.hitShader.chs.name };
                shaderPayloadAssociation = { &subobjects[(index - 1)], _countof(shaderExports), shaderExports };
            }

            D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = { D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &shaderPayloadAssociation };
            subobjects[index++] = shaderPayloadAssociationObject;
        }

        // Add a state subobject for the shared root signature 
        {
            D3D12_STATE_SUBOBJECT rayGenRootSigObject = { D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &hitGroup.rayGenShader.pRootSignature };
            subobjects[index++] = rayGenRootSigObject;
        }

        // Create a list of the shader export names that use the root signature
        {
            D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenShaderRootSigAssociation = {};
            if (hitGroup.hitShader.ahs.name != nullptr)
            {
                const WCHAR* rootSigExports[] = { m_hitGroups[0].rayGenShader.name, hitGroup.missShader.name, hitGroup.hitShader.chs.name, hitGroup.hitShader.ahs.name };
                rayGenShaderRootSigAssociation = { &subobjects[(index - 1)], _countof(rootSigExports), rootSigExports };
            }
            else
            {
                const WCHAR* rootSigExports[] = { hitGroup.rayGenShader.name, hitGroup.missShader.name, hitGroup.hitShader.chs.name };
                rayGenShaderRootSigAssociation = { &subobjects[(index - 1)], _countof(rootSigExports), rootSigExports };
            }

            D3D12_STATE_SUBOBJECT rayGenShaderRootSigAssociationObject = { D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &rayGenShaderRootSigAssociation };
            subobjects[index++] = rayGenShaderRootSigAssociationObject;
        }
    }
#pragma endregion

#pragma region HitGroup creation - 2
    if (m_hitGroups.size() > 1)
    {
        HitGroup hitGroup = m_hitGroups[1];
        //Add state subobject for the Miss shader
        {
            D3D12_EXPORT_DESC msExportDesc = { hitGroup.missShader.name, hitGroup.missShader.exportToRename, D3D12_EXPORT_FLAG_NONE };

            D3D12_DXIL_LIBRARY_DESC	msLibDesc = {};
            msLibDesc.DXILLibrary.BytecodeLength = hitGroup.missShader.blob->GetBufferSize();
            msLibDesc.DXILLibrary.pShaderBytecode = hitGroup.missShader.blob->GetBufferPointer();
            msLibDesc.NumExports = 1;
            msLibDesc.pExports = &msExportDesc;

            D3D12_STATE_SUBOBJECT ms = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &msLibDesc };
            subobjects[index++] = ms;
        }

        // Add state subobject for the Closest Hit shader
        {
            D3D12_EXPORT_DESC chsExportDesc = { hitGroup.hitShader.chs.name, hitGroup.hitShader.chs.exportToRename, D3D12_EXPORT_FLAG_NONE };

            D3D12_DXIL_LIBRARY_DESC	chsLibDesc = {};
            chsLibDesc.DXILLibrary.BytecodeLength = hitGroup.hitShader.chs.blob->GetBufferSize();
            chsLibDesc.DXILLibrary.pShaderBytecode = hitGroup.hitShader.chs.blob->GetBufferPointer();
            chsLibDesc.NumExports = 1;
            chsLibDesc.pExports = &chsExportDesc;

            D3D12_STATE_SUBOBJECT chs = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &chsLibDesc };
            subobjects[index++] = chs;
        }

        // Add state subobject for the Any Hit shader
        if (hitGroup.hitShader.ahs.name != nullptr)
        {
            D3D12_EXPORT_DESC ahsExportDesc = { hitGroup.hitShader.ahs.name, hitGroup.hitShader.ahs.exportToRename, D3D12_EXPORT_FLAG_NONE };

            D3D12_DXIL_LIBRARY_DESC	ahsLibDesc = {};
            ahsLibDesc.DXILLibrary.BytecodeLength = hitGroup.hitShader.ahs.blob->GetBufferSize();
            ahsLibDesc.DXILLibrary.pShaderBytecode = hitGroup.hitShader.ahs.blob->GetBufferPointer();
            ahsLibDesc.NumExports = 1;
            ahsLibDesc.pExports = &ahsExportDesc;

            D3D12_STATE_SUBOBJECT ahs = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &ahsLibDesc };
            subobjects[index++] = ahs;
        }

        // Add a state subobject for the hit group
        {
            D3D12_HIT_GROUP_DESC hitGroupDesc = {};
            hitGroupDesc.ClosestHitShaderImport = hitGroup.hitShader.chs.name;
            if (hitGroup.hitShader.ahs.name != nullptr) hitGroupDesc.AnyHitShaderImport = hitGroup.hitShader.ahs.name;
            hitGroupDesc.HitGroupExport = hitGroup.hitGroupName;

            D3D12_STATE_SUBOBJECT hitGroup = { D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroupDesc };
            subobjects[index++] = hitGroup;
        }

        // Add a state subobject for the shader payload configuration
        {
            D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = { static_cast<UINT>(maxPayloadSize), D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES };
            D3D12_STATE_SUBOBJECT shaderConfigObject = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderDesc };
            subobjects[index++] = shaderConfigObject;
        }

        // Create a list of the shader export names that use the payload
        {
            D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
            if (hitGroup.hitShader.ahs.name != nullptr) {
                const WCHAR* shaderExports[] = { hitGroup.missShader.name, hitGroup.hitShader.chs.name, hitGroup.hitShader.ahs.name };
                shaderPayloadAssociation = { &subobjects[(index - 1)], _countof(shaderExports), shaderExports };
            }
            else {
                const WCHAR* shaderExports[] = { hitGroup.missShader.name, hitGroup.hitShader.chs.name };
                shaderPayloadAssociation = { &subobjects[(index - 1)], _countof(shaderExports), shaderExports };
            }

            D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = { D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &shaderPayloadAssociation };
            subobjects[index++] = shaderPayloadAssociationObject;
        }

        // Add a state subobject for the shared root signature 
        {
            D3D12_STATE_SUBOBJECT rayGenRootSigObject = { D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &hitGroup.rayGenShader.pRootSignature };
            subobjects[index++] = rayGenRootSigObject;
        }

        // Create a list of the shader export names that use the root signature
        {
            D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenShaderRootSigAssociation = {};
            if (hitGroup.hitShader.ahs.name != nullptr)
            {
                const WCHAR* rootSigExports[] = { hitGroup.missShader.name, hitGroup.hitShader.chs.name, hitGroup.hitShader.ahs.name };
                rayGenShaderRootSigAssociation = { &subobjects[(index - 1)], _countof(rootSigExports), rootSigExports };
            }
            else
            {
                const WCHAR* rootSigExports[] = { hitGroup.missShader.name, hitGroup.hitShader.chs.name };
                rayGenShaderRootSigAssociation = { &subobjects[(index - 1)], _countof(rootSigExports), rootSigExports };
            }

            D3D12_STATE_SUBOBJECT rayGenShaderRootSigAssociationObject = { D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &rayGenShaderRootSigAssociation };
            subobjects[index++] = rayGenShaderRootSigAssociationObject;
        }
    }
#pragma endregion

    // Setup global root signature
    {
        D3D12_STATE_SUBOBJECT globalRootSig = { D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &dummyGlobalRootSignature };
        subobjects[index++] = globalRootSig;
    }

    // The pipeline construction always requires an empty local root signature
    {
        D3D12_STATE_SUBOBJECT localRootSig = { D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &dummyLocalRootSignature };
        subobjects[index++] = localRootSig;
    }

    // Add a state subobject for the ray tracing pipeline config
    {
        D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = { 2 }; // Max Trace Recursion Depth
        D3D12_STATE_SUBOBJECT pipelineConfigObject = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig };
        subobjects[index++] = pipelineConfigObject; 
    }

    // Finish rtpso creation and store in variable
    {
        // Describe RTPSO
        D3D12_STATE_OBJECT_DESC pipelineDesc = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE, static_cast<UINT>(subobjects.size()), subobjects.data() };

        // Create the RT Pipeline State Object (RTPSO) / Get the RTPSO properties
        ThrowIfFailed(device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&m_rtpso)));
        ThrowIfFailed(m_rtpso->QueryInterface(IID_PPV_ARGS(&m_rtpsoInfo)));
    }
}
