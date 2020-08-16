#include "RaytracingResources.h"

RaytracingResources::RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, LPCWSTR rayGenName, LPCWSTR missName, LPCWSTR hitName, LPCWSTR hitGroupName, LPCWSTR rayGenNameExport, LPCWSTR missNameExport, LPCWSTR hitNameExport)
{
    m_rayGenShader.name = rayGenName;
    m_rayGenShader.exportToRename = rayGenNameExport;
    m_missShader.name = missName;
    m_missShader.exportToRename = missNameExport;
    m_hitShader.name = hitName;
    m_hitShader.exportToRename = hitNameExport;
	m_hitGroupName = hitGroupName;

    CreateBLAS(model, device, commandList);
    CreateTLAS(device, commandList);
}

RaytracingResources::RaytracingResources(ID3D12Device5* device, ComPtr<ID3D12GraphicsCommandList4> commandList, std::shared_ptr<ModelClass> model, RtProgram rayGenShader, RtProgram missShader, HitProgram hitShader, LPCWSTR hitGroupName)
{
    m_rayGenShader = rayGenShader;
    m_missShader = missShader;
    m_hitShader = hitShader;
    m_hitGroupName = hitGroupName;

    CreateBLAS(model, device, commandList);
    CreateTLAS(device, commandList);
}

void RaytracingResources::CreateRaytracingPipelineContinue(ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, std::vector<ResourceWithSize> buffersWithSize, size_t maxPayloadSize)
{
    CreateDxrPipelineAssets(device, model, texturesWithDesc, indexDesc, vertexDesc, buffersWithSize);
    CreateRTPSO(device, maxPayloadSize);
    CreateShaderTable(device);
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

void RaytracingResources::CreateDxrPipelineAssets(ID3D12Device5* device, ModelClass* model, std::vector<TextureWithDesc> texturesWithDesc, D3D12_SHADER_RESOURCE_VIEW_DESC indexDesc, D3D12_SHADER_RESOURCE_VIEW_DESC vertexDesc, std::vector<ResourceWithSize> buffersWithSize)
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
            // First element
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { buffersWithSize[0].first->GetGPUVirtualAddress(), ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(buffersWithSize[0].second)) };
            device->CreateConstantBufferView(&cbvDesc, handle);

            // Next elements
            for (int i = 1; i < buffersWithSize.size(); ++i)
            {
                handle.ptr += handleIncrement;
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { buffersWithSize[i].first->GetGPUVirtualAddress(), ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(buffersWithSize[i].second)) };
                device->CreateConstantBufferView(&cbvDesc, handle);
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
        device->CreateShaderResourceView(model->GetIndexBuffer().Get(), &indexDesc, handle);

        // Create the vertex buffer SRV
        handle.ptr += handleIncrement;
        device->CreateShaderResourceView(model->GetVertexBuffer().Get(), &vertexDesc, handle);

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

    uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    uint32_t shaderTableSize = 0;

    m_shaderTableRecordSize = shaderIdSize;
    m_shaderTableRecordSize += 8;							// CBV/SRV/UAV descriptor table
    m_shaderTableRecordSize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, m_shaderTableRecordSize);

    shaderTableSize = (m_shaderTableRecordSize * 3);		// 3 shader records in the table
    shaderTableSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, shaderTableSize);

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

    // Shader Record 0 - Ray Generation program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
    memcpy(pData, m_rtpsoInfo->GetShaderIdentifier(m_rayGenShader.name), shaderIdSize);

    // Set the root parameter data. Point to start of descriptor heap.
    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // Shader Record 1 - Miss program (no local root arguments to set)
    pData += m_shaderTableRecordSize;
    memcpy(pData, m_rtpsoInfo->GetShaderIdentifier(m_missShader.name), shaderIdSize);

    // Shader Record 2 - Closest Hit program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
    pData += m_shaderTableRecordSize;
    memcpy(pData, m_rtpsoInfo->GetShaderIdentifier(m_hitGroupName), shaderIdSize);

    // Set the root parameter data. Point to start of descriptor heap.
    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

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
    subobjects.resize(m_hitShader.ahs.name != nullptr ? 11 : 10);

    // Add state subobject for the RGS
    D3D12_EXPORT_DESC rgsExportDesc = {};
    rgsExportDesc.Name = m_rayGenShader.name; // L"RayGen_12";
    rgsExportDesc.ExportToRename = m_rayGenShader.exportToRename; //L"RayGen";
    rgsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC	rgsLibDesc = {};
    rgsLibDesc.DXILLibrary.BytecodeLength = m_rayGenShader.blob->GetBufferSize();
    rgsLibDesc.DXILLibrary.pShaderBytecode = m_rayGenShader.blob->GetBufferPointer();
    rgsLibDesc.NumExports = 1;
    rgsLibDesc.pExports = &rgsExportDesc;

    D3D12_STATE_SUBOBJECT rgs = {};
    rgs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    rgs.pDesc = &rgsLibDesc;

    subobjects[index++] = rgs;

    // Add state subobject for the Miss shader
    D3D12_EXPORT_DESC msExportDesc = {};
    msExportDesc.Name = m_missShader.name; //L"Miss_5";
    msExportDesc.ExportToRename = m_missShader.exportToRename; //L"Miss";
    msExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC	msLibDesc = {};
    msLibDesc.DXILLibrary.BytecodeLength = m_missShader.blob->GetBufferSize();
    msLibDesc.DXILLibrary.pShaderBytecode = m_missShader.blob->GetBufferPointer();
    msLibDesc.NumExports = 1;
    msLibDesc.pExports = &msExportDesc;

    D3D12_STATE_SUBOBJECT ms = {};
    ms.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    ms.pDesc = &msLibDesc;

    subobjects[index++] = ms;

    // Add state subobject for the Closest Hit shader
    D3D12_EXPORT_DESC chsExportDesc = {};
    chsExportDesc.Name = m_hitShader.chs.name; //L"ClosestHit_76";
    chsExportDesc.ExportToRename = m_hitShader.chs.exportToRename; //L"ClosestHit";
    chsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

    D3D12_DXIL_LIBRARY_DESC	chsLibDesc = {};
    chsLibDesc.DXILLibrary.BytecodeLength = m_hitShader.chs.blob->GetBufferSize();
    chsLibDesc.DXILLibrary.pShaderBytecode = m_hitShader.chs.blob->GetBufferPointer();
    chsLibDesc.NumExports = 1;
    chsLibDesc.pExports = &chsExportDesc;

    D3D12_STATE_SUBOBJECT chs = {};
    chs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    chs.pDesc = &chsLibDesc;

    subobjects[index++] = chs;

    if (m_hitShader.ahs.name != nullptr)
    {
        // Add state subobject for the Any Hit shader
        D3D12_EXPORT_DESC ahsExportDesc = {};
        ahsExportDesc.Name = m_hitShader.ahs.name; //L"AnytHit_34";
        ahsExportDesc.ExportToRename = m_hitShader.ahs.exportToRename; //L"AnyHit";
        ahsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

        D3D12_DXIL_LIBRARY_DESC	ahsLibDesc = {};
        ahsLibDesc.DXILLibrary.BytecodeLength = m_hitShader.ahs.blob->GetBufferSize();
        ahsLibDesc.DXILLibrary.pShaderBytecode = m_hitShader.ahs.blob->GetBufferPointer();
        ahsLibDesc.NumExports = 1;
        ahsLibDesc.pExports = &ahsExportDesc;

        D3D12_STATE_SUBOBJECT ahs = {};
        ahs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        ahs.pDesc = &ahsLibDesc;

        subobjects[index++] = ahs;
    }

    // Add a state subobject for the hit group
    D3D12_HIT_GROUP_DESC hitGroupDesc = {};
    hitGroupDesc.ClosestHitShaderImport = m_hitShader.chs.name; // L"ClosestHit_76";
    hitGroupDesc.HitGroupExport = m_hitGroupName; // L"HitGroup";

    D3D12_STATE_SUBOBJECT hitGroup = {};
    hitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    hitGroup.pDesc = &hitGroupDesc;

    subobjects[index++] = hitGroup;

    // Add a state subobject for the shader payload configuration
    D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
    shaderDesc.MaxPayloadSizeInBytes = static_cast<UINT>(maxPayloadSize);
    shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;

    D3D12_STATE_SUBOBJECT shaderConfigObject = {};
    shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigObject.pDesc = &shaderDesc;

    subobjects[index++] = shaderConfigObject;

    // Create a list of the shader export names that use the payload
    //const WCHAR* shaderExports[] = { L"RayGen_12", L"Miss_5", L"HitGroup" };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
    if (m_hitShader.ahs.name != nullptr)
    {
        const WCHAR* shaderExports[] = { m_rayGenShader.name, m_missShader.name, m_hitShader.chs.name, m_hitShader.ahs.name };
        // Add a state subobject for the association between shaders and the payload
        shaderPayloadAssociation.NumExports = _countof(shaderExports);
        shaderPayloadAssociation.pExports = shaderExports;
        shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];
    }
    else
    {
        const WCHAR* shaderExports[] = { m_rayGenShader.name, m_missShader.name, m_hitShader.chs.name };
        // Add a state subobject for the association between shaders and the payload
        shaderPayloadAssociation.NumExports = _countof(shaderExports);
        shaderPayloadAssociation.pExports = shaderExports;
        shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];
    }

    D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
    shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;

    subobjects[index++] = shaderPayloadAssociationObject;

    // Add a state subobject for the shared root signature 
    D3D12_STATE_SUBOBJECT rayGenRootSigObject = {};
    rayGenRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    rayGenRootSigObject.pDesc = &m_rayGenShader.pRootSignature;

    subobjects[index++] = rayGenRootSigObject;

    // Create a list of the shader export names that use the root signature
    //const WCHAR* rootSigExports[] = { L"RayGen_12", L"HitGroup", L"Miss_5" };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenShaderRootSigAssociation = {};
    if (m_hitShader.ahs.name != nullptr)
    {
        const WCHAR* rootSigExports[] = { m_rayGenShader.name, m_missShader.name, m_hitShader.chs.name, m_hitShader.ahs.name };
        // Add a state subobject for the association between the RayGen shader and the local root signature
        rayGenShaderRootSigAssociation.NumExports = _countof(rootSigExports);
        rayGenShaderRootSigAssociation.pExports = rootSigExports;
        rayGenShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];
    }
    else
    {
        const WCHAR* rootSigExports[] = { m_rayGenShader.name, m_missShader.name, m_hitShader.chs.name };
        // Add a state subobject for the association between the RayGen shader and the local root signature
        rayGenShaderRootSigAssociation.NumExports = _countof(rootSigExports);
        rayGenShaderRootSigAssociation.pExports = rootSigExports;
        rayGenShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];
    }

    D3D12_STATE_SUBOBJECT rayGenShaderRootSigAssociationObject = {};
    rayGenShaderRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    rayGenShaderRootSigAssociationObject.pDesc = &rayGenShaderRootSigAssociation;

    subobjects[index++] = rayGenShaderRootSigAssociationObject;

    D3D12_STATE_SUBOBJECT globalRootSig;
    globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    globalRootSig.pDesc = &m_missShader.pRootSignature;

    subobjects[index++] = globalRootSig;

    // Add a state subobject for the ray tracing pipeline config
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = 1;

    D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
    pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    pipelineConfigObject.pDesc = &pipelineConfig;

    subobjects[index++] = pipelineConfigObject;

    // Describe the Ray Tracing Pipeline State Object
    D3D12_STATE_OBJECT_DESC pipelineDesc = {};
    pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    pipelineDesc.NumSubobjects = static_cast<UINT>(subobjects.size());
    pipelineDesc.pSubobjects = subobjects.data();

    // Create the RT Pipeline State Object (RTPSO)
    ThrowIfFailed(device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&m_rtpso)));

    // Get the RTPSO properties
    ThrowIfFailed(m_rtpso->QueryInterface(IID_PPV_ARGS(&m_rtpsoInfo)));
}