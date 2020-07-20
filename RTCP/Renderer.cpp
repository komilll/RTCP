#include "Renderer.h"
#include "WICTextureLoader.h"
#include "ResourceUploadBatch.h"
#include "DirectXHelpers.h"

Renderer::Renderer(std::shared_ptr<DeviceManager> deviceManager, HWND hwnd)
{
    m_deviceManager = deviceManager;
    assert(m_deviceManager && "Device manager is null");

    m_viewport.TopLeftY = 0.0f;
    m_viewport.TopLeftX = 0.0f;
    m_viewport.Width = static_cast<float>(m_windowWidth);
    m_viewport.Height = static_cast<float>(m_windowHeight);

    m_scissorRect.left = 0;
    m_scissorRect.top = 0;
    m_scissorRect.right = static_cast<LONG>(m_windowWidth);
    m_scissorRect.bottom = static_cast<LONG>(m_windowHeight);

    m_rtvDescriptorSize = 0;

    m_cameraPosition = XMFLOAT3{ 0, 0, -100.0f };

#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
    Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialize)) {}
    // error
#else
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
        // error
#endif
}

void Renderer::Render()
{
    OnRender();
}

void Renderer::OnInit(HWND hwnd)
{
    LoadPipeline(hwnd);
    LoadAssets();
}

void Renderer::OnUpdate()
{
    {
        // Object not rotated, scale or translated
        m_constantBufferData.world = XMMatrixIdentity();
        //m_constantBufferData.world = XMMatrixMultiply(m_constantBufferData.world, XMMatrixScaling(10, 10, 10));
        //m_constantBufferData.world = XMMatrixMultiply(m_constantBufferData.world, XMMatrixTranslation(0.0f, 0.0f, 0.0f));
        //m_constantBufferData.world = XMMatrixTranspose(m_constantBufferData.world);

        CreateViewAndPerspective();

        memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
    }
}

void Renderer::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    MoveToNextFrame();
}

void Renderer::OnDestroy()
{
    // Wait for the GPU to be done with all resources.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void Renderer::AddCameraPosition(float x, float y, float z)
{
    //if (x != 0 || y != 0 || z != 0)
    {
        m_cameraPositionStoredInFrame.x = x;
        m_cameraPositionStoredInFrame.y = y;
        m_cameraPositionStoredInFrame.z = z;
        CreateViewAndPerspective();
    }
    //if (x != 0 || y != 0 || z != 0)
    //{
    //    m_profiler->ResetProfiling();
    //}
}

void Renderer::AddCameraPosition(XMFLOAT3 addPos)
{
    AddCameraPosition(addPos.x, addPos.y, addPos.z);
}

void Renderer::AddCameraRotation(float x, float y, float z)
{
    //if (x != 0 || y != 0 || z != 0)
    {
        m_cameraRotation.x += x;
        m_cameraRotation.y += y;

        if (m_cameraRotation.y > 360.0f)
            m_cameraRotation.y -= 360.0f;
        else if (m_cameraRotation.y < -360.0f)
            m_cameraRotation.y += 360.0f;
        m_cameraRotation.x = m_cameraRotation.x >= 90.0f ? 89.9f : (m_cameraRotation.x <= -90.0f ? -89.9f : m_cameraRotation.x);

        CreateViewAndPerspective();
    }
    //if (x != 0 || y != 0 || z != 0)
    //{
    //    m_profiler->ResetProfiling();
    //}
}

void Renderer::AddCameraRotation(XMFLOAT3 addRot)
{
    AddCameraPosition(addRot.x, addRot.y, addRot.z);
}

void Renderer::UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList> commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags /* = D3D12_RESOURCE_FLAG_NONE */, bool setName /* = false */, LPCWSTR newName /* = L"" */)
{
    size_t bufferSize = numElements * elementSize;
    {
        ThrowIfFailed(
            m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags), D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(pDestinationResource))
        );
    }

    // Make sure that passed data is not empty
    if (bufferData)
    {
        ThrowIfFailed(
            m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(bufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(pIntermediateResource))
        );

        D3D12_SUBRESOURCE_DATA subresourceData{};
        subresourceData.pData = bufferData;
        subresourceData.RowPitch = bufferSize;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        if (setName) {
            (*pDestinationResource)->SetName(newName);
        }

        UpdateSubresources(commandList.Get(), *pDestinationResource, *pIntermediateResource, 0, 0, 1, &subresourceData);
    }
}

void Renderer::ResizeDepthBuffer(int width, int height)
{
}

void Renderer::LoadPipeline(HWND hwnd)
{
    UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
    // Enable the D3D12 debug layer.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    // Create factory which will be used throughout function
    ComPtr<IDXGIFactory4> dxgiFactory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    // Create device, command queue and swap chain
    m_device = m_deviceManager->CreateDevice(dxgiFactory);
    m_commandQueue = m_deviceManager->CreateCommandQueue(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_swapChain = m_deviceManager->CreateSwapChain(hwnd, m_commandQueue, dxgiFactory, m_windowWidth, m_windowHeight, m_frameCount);

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    
    // Store current index of back buffer
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = m_frameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Create constant buffer view (CBV) heap
        D3D12_DESCRIPTOR_HEAP_DESC cbvDescHeap{};
        cbvDescHeap.NumDescriptors = 1;
        cbvDescHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvDescHeap, IID_PPV_ARGS(&m_cbvHeap)));
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < m_frameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_backBuffers[n])));
            m_device->CreateRenderTargetView(m_backBuffers[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
        }
    }
}

void Renderer::LoadAssets()
{
    // Create an empty root signature.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[2] = {};
        CD3DX12_ROOT_PARAMETER1 rootParameters[2] = {};

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(L"Shaders/vertexShader.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", 0, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(L"Shaders/pixelShader.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());

    std::unique_ptr<ModelClass> model = std::unique_ptr<ModelClass>(new ModelClass());
    model->LoadModel("sphere.obj", m_device);

    // Create the vertex buffer.
    {
        //ComPtr<ID3D12Resource> vertexUploadHeap;
        //UpdateBufferResource(m_commandList, &m_vertexBuffer, &vertexUploadHeap, model->GetMesh(0).vertices.size(), sizeof(ModelClass::VertexBufferStruct), &model->GetMesh(0).vertices[0], D3D12_RESOURCE_FLAG_NONE, true, L"VertexBuffer");

        //m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

        //// Initialize the vertex buffer view.
        //m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        //m_vertexBufferView.StrideInBytes = sizeof(ModelClass::VertexBufferStruct);
        //m_vertexBufferView.SizeInBytes = sizeof(model->GetMesh(0).vertices);

        ModelClass::Mesh mesh = model->GetMesh(0);
        const UINT vertexBufferSize = static_cast<UINT>(sizeof(ModelClass::VertexBufferStruct)) * static_cast<UINT>(mesh.vertices.size());

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, &mesh.vertices[0], vertexBufferSize);
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(ModelClass::VertexBufferStruct);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Create the index buffer.
    {
        //ComPtr<ID3D12Resource> indexUploadHeap;
        //UpdateBufferResource(m_commandList, &m_indexBuffer, &indexUploadHeap, model->GetMesh(0).indices.size(), sizeof(ModelClass::VertexBufferStruct), &model->GetMesh(0).indices[0], D3D12_RESOURCE_FLAG_NONE, true, L"IndexBuffer");

        //m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

        //// Initialize the index buffer view.
        //m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        //m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        //m_indexBufferView.SizeInBytes = sizeof(model->GetMesh(0).indices);

        ModelClass::Mesh mesh = model->GetMesh(0);
        const UINT indexBufferSize = sizeof(mesh.indices);
        m_indicesCount = mesh.indices.size();

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_indexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pIndexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
        memcpy(pIndexDataBegin, &mesh.indices[0], sizeof(mesh.indices));
        m_indexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_indexBufferView.SizeInBytes = indexBufferSize;
    }

    // Create the constant buffer
    {
        const UINT constantBufferSize = sizeof(ConstantBufferStruct);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBuffer)
        ));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantBufferSize;
        m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
        memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
    }

    // Close the command list and execute it to begin the initial GPU setup.
    //ThrowIfFailed(m_commandList->Close());
    //ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    //m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    ComPtr<ID3D12Resource> tex;

    // Create texture
    {
        std::unique_ptr<uint8_t[]> decodedData;
        D3D12_SUBRESOURCE_DATA subresource;
        ThrowIfFailed(LoadWICTextureFromFile(m_device.Get(), L"texture.bmp", tex.ReleaseAndGetAddressOf(), decodedData, subresource));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex.Get(), 0, 1);

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        ComPtr<ID3D12Resource> uploadRes;
        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(uploadRes.GetAddressOf())
        ));

        UpdateSubresources(m_commandList.Get(), tex.Get(), uploadRes.Get(), 0, 0, 1, &subresource);

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &barrier);
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValues[m_frameIndex]++;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

void Renderer::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    //m_commandList->IASetIndexBuffer(&m_indexBufferView);
    //m_commandList->DrawInstanced(3, 1, 0, 0);
    m_commandList->DrawInstanced(m_indicesCount, 1, 0, 0);

    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}

void Renderer::WaitForPreviousFrame()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

    // Increment the fence value for the current frame.
    m_fenceValues[m_frameIndex]++;
}

void Renderer::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    // Update the frame index.
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

void Renderer::GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter)
{
    *ppAdapter = nullptr;
    for (UINT adapterIndex = 0; ; ++adapterIndex)
    {
        IDXGIAdapter1* pAdapter = nullptr;
        if (DXGI_ERROR_NOT_FOUND == pFactory->EnumAdapters1(adapterIndex, &pAdapter))
        {
            // No more adapters to enumerate.
            break;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the
        // actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            *ppAdapter = pAdapter;
            return;
        }
        pAdapter->Release();
    }
}

void Renderer::CreateViewAndPerspective()
{
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.f);
    constexpr float conv{ 0.0174532925f };

    //Update camera position for shader buffer
    if (!FREEZE_CAMERA)
    {
        //m_uberBufferData.viewerPosition = m_cameraPosition;
    }

    // Create the rotation matrix from the yaw, pitch, and roll values.
    const XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(m_cameraRotation.x * conv, m_cameraRotation.y * conv, m_cameraRotation.z * conv);

    //Move camera along direction we look at
    if (m_cameraPositionStoredInFrame.x != 0 || m_cameraPositionStoredInFrame.y != 0 || m_cameraPositionStoredInFrame.z != 0)
    {
        const XMMATRIX YrotationMatrix = XMMatrixRotationY(m_cameraRotation.y * conv);
        const XMVECTOR camRight = XMVector3TransformCoord(XMVECTOR{ 1,0,0,0 }, YrotationMatrix);
        const XMVECTOR camForward = XMVector3TransformCoord(XMVECTOR{ 0, 0, 1, 0 }, rotationMatrix);

        const XMVECTOR addPos = camRight * m_cameraPositionStoredInFrame.x + camForward * m_cameraPositionStoredInFrame.z;
        m_cameraPosition.x += addPos.m128_f32[0];
        m_cameraPosition.y += (addPos.m128_f32[1] + m_cameraPositionStoredInFrame.y);
        m_cameraPosition.z += addPos.m128_f32[2];

        m_cameraPositionStoredInFrame = XMFLOAT3{ 0,0,0 };
        if (m_cameraPosition.x == 0.0f && m_cameraPosition.y == 0.0f && m_cameraPosition.z == 0.0f)
        {
            m_cameraPosition.x = FLT_MIN;
        }
    }
    const DirectX::XMVECTOR eye = DirectX::XMVectorSet(m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z, 0.0f);

    //Setup target (look at object position)
    XMVECTOR target = XMVector3TransformCoord(DirectX::XMVECTOR{ 0, 0, 1, 0 }, rotationMatrix);
    target = XMVector3Normalize(target);
    target = { m_cameraPosition.x + target.m128_f32[0], m_cameraPosition.y + target.m128_f32[1], m_cameraPosition.z + target.m128_f32[2], 0.0f };

    //Create view matrix
    m_constantBufferData.view = DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(eye, target, up));

    //Create perspective matrix
    constexpr float FOV = 3.14f / 4.0f;
    //const float aspectRatio = m_windowHeight / m_windowWidth;
    const float aspectRatio = static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight);
    m_constantBufferData.projection = DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(FOV, aspectRatio, Z_NEAR, Z_FAR));
    //Store projection matrix in SSAO as soon as it changes
    //m_specialBufferSSAOData.projectionMatrix = m_constantBufferData.projection;
}
