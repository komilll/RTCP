#include "Renderer.h"
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#include <ResourceUploadBatch.h>
#include <DirectXHelpers.h>
#include <DirectXTex.h>
#include <SimpleMath.h>

Renderer::Renderer(std::shared_ptr<DeviceManager> deviceManager, HWND hwnd)
{
    m_deviceManager = deviceManager;
    assert(m_deviceManager && "Device manager is null");

    m_viewport.TopLeftY = 0.0f;
    m_viewport.TopLeftX = 0.0f;
    m_viewport.Width = static_cast<float>(m_windowWidth);
    m_viewport.Height = static_cast<float>(m_windowHeight);

    m_viewportPreview.TopLeftY = 0;
    m_viewportPreview.TopLeftX = m_windowWidth * 0.7;
    m_viewportPreview.Width = static_cast<float>(m_windowWidth * 0.3);
    m_viewportPreview.Height = static_cast<float>(m_windowHeight * 0.3);

    m_scissorRect.left = 0;
    m_scissorRect.top = 0;
    m_scissorRect.right = static_cast<LONG>(m_windowWidth);
    m_scissorRect.bottom = static_cast<LONG>(m_windowHeight);

    m_scissorRectPreview.left = 0;
    m_scissorRectPreview.top = 0;
    m_scissorRectPreview.right = static_cast<LONG>(m_windowWidth);
    m_scissorRectPreview.bottom = static_cast<LONG>(m_windowHeight);

    m_rtvDescriptorSize = 0;

    m_cameraPosition = XMFLOAT3{ 0, 0, -100.0f };
}

void Renderer::Render()
{
    OnRender();
}

void Renderer::OnInit(HWND hwnd)
{
    // Preparing devices, resources, views to enable rendering
    LoadPipeline(hwnd);
    LoadAssets();
}

void Renderer::OnUpdate()
{
    {
        CreateViewAndPerspective();
        m_constantBufferData.world = XMMatrixIdentity();
        memcpy(m_pConstantBufferDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));

        // Skybox
        XMFLOAT3 pos = XMFLOAT3{ m_cameraPosition.x - 0.5f, m_cameraPosition.y - 0.5f, m_cameraPosition.z - 0.5f };
        m_constantBufferData.world = XMMatrixIdentity();
        m_constantBufferData.world = XMMatrixMultiply(m_constantBufferData.world, XMMatrixScaling(1.0f, 1.0f, 1.0f));
        m_constantBufferData.world = XMMatrixMultiply(m_constantBufferData.world, XMMatrixTranslation(pos.x, pos.y, pos.z));
        m_constantBufferData.world = XMMatrixTranspose(m_constantBufferData.world);

        // Uber buffer memcpy
        memcpy(m_pConstantBufferDataBeginSkybox, &m_constantBufferData, sizeof(m_constantBufferData));
        m_uberBufferData.viewPosition = m_cameraPosition;
        for (auto& p : m_uberBufferData.padding) {
            p = {};
        }
        memcpy(m_pUberBufferDataBegin, &m_uberBufferData, sizeof(m_uberBufferData));
    }

    {
        m_sceneBufferData.cameraPosition = XMFLOAT4(m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z, 1.0f);
        XMVECTOR eye = XMVECTOR{ m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z };
        XMMATRIX view = XMMatrixLookAtLH(eye, XMVECTOR{ 0,0,0 }, XMVECTOR{ 0, 1, 0 });
        //XMMATRIX view = XMMatrixTranspose(m_constantBufferData.view);
        //XMMATRIX proj = XMMatrixTranspose(m_constantBufferData.projection);

        constexpr float FOV = 45.0f;
        XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(XMConvertToRadians(FOV), m_aspectRatio, Z_NEAR, Z_FAR);

        XMMATRIX viewProj = XMMatrixMultiply(view, proj);
        m_sceneBufferData.projectionToWorld = XMMatrixInverse(nullptr, viewProj);

        XMFLOAT4 lightPosition = XMFLOAT4(0.0f, 1.8f, -3.0f, 0.0f);
        XMFLOAT4 lightAmbientColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        XMFLOAT4 lightDiffuseColor = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f);
        m_sceneBufferData.lightPosition = lightPosition;
        m_sceneBufferData.lightAmbientColor = lightAmbientColor;
        m_sceneBufferData.lightDiffuseColor = lightDiffuseColor;

        // Uber buffer memcpy
        memcpy(m_sceneBufferDataBegin, &m_sceneBufferData, sizeof(m_sceneBufferData));
    }
}

void Renderer::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get(), m_commandListSkybox.Get() };
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
    if (x != 0 || y != 0 || z != 0)
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
    if (x != 0 || y != 0 || z != 0)
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

        // Describe and create a shader resource view (SRV) heap for the texture.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = 2;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

        // Describe and create a depth stencil view (DSV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
    }

    // Create the depth stencil view.
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_windowWidth, m_windowHeight, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&m_depthStencil)
        ));

        m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
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
            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocatorsSkybox[n])));
        }
    }
}

void Renderer::LoadAssets()
{
    //D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    //featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    //if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
    //    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    //}

    // Create an empty root signature.
    {
        CD3DX12_ROOT_PARAMETER rootParameters[2] = {};
        CD3DX12_DESCRIPTOR_RANGE range{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 };
        rootParameters[0].InitAsDescriptorTable(1, &range);
        rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = ROOT_SIGNATURE_PIXEL;

        CD3DX12_STATIC_SAMPLER_DESC samplers[1] = {};
        samplers[0].Init(0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT);

        CreateRootSignatureRTCP(_countof(rootParameters), _countof(samplers), rootParameters, samplers, rootSignatureFlags, m_rootSignature);
    }

    // Create an empty root signature. - SKYBOX
    {
        CD3DX12_ROOT_PARAMETER rootParameters[3] = {};
        CD3DX12_DESCRIPTOR_RANGE range{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 };
        rootParameters[0].InitAsDescriptorTable(1, &range);
        rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = ROOT_SIGNATURE_PIXEL;

        CD3DX12_STATIC_SAMPLER_DESC samplers[1] = {};
        samplers[0].Init(0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT);

        CreateRootSignatureRTCP(_countof(rootParameters), _countof(samplers), rootParameters, samplers, rootSignatureFlags, m_rootSignatureSkybox);
    }

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        // Prepare shaders
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;
        ThrowIfFailed(D3DCompileFromFile(L"Shaders/vertexShader.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", 0, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(L"Shaders/pixelShader.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &pixelShader, nullptr));

        // Preprare layout, DSV and create PSO
        auto inputElementDescs = CreateBasicInputLayout();
        CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc = CreateDefaultDepthStencilDesc();
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = CreateDefaultPSO(inputElementDescs, vertexShader, pixelShader, depthStencilDesc, m_rootSignature);
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // Create the pipeline state, which includes compiling and loading shaders. - SKYBOX
    {
        // Prepare shaders
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;
        ThrowIfFailed(D3DCompileFromFile(L"Shaders/VS_Skybox.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", 0, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(L"Shaders/PS_Skybox.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &pixelShader, nullptr));

        // Preprare layout, DSV and create PSO
        auto inputElementDescs = CreateBasicInputLayout();
        CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc = CreateDefaultDepthStencilDesc();

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = CreateDefaultPSO(inputElementDescs, vertexShader, pixelShader, depthStencilDesc, m_rootSignatureSkybox);
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateSkybox)));
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocatorsSkybox[m_frameIndex].Get(), m_pipelineStateSkybox.Get(), IID_PPV_ARGS(&m_commandListSkybox)));

    // Create the vertex buffer.
    {
        m_modelCube = std::shared_ptr<ModelClass>(new ModelClass("cube.obj", m_device));
        m_modelSphere = std::shared_ptr<ModelClass>(new ModelClass("sphere.obj", m_device));
        m_previewModel = std::shared_ptr<ModelClass>(new ModelClass());
        m_previewModel->SetFullScreenRectangleModel(m_device, m_commandList, 0.75f, 1.0f, 1.0f, 0.75f * 1.666667f);
    }

    // Preparation of raytracing
    PrepareRaytracingResources();

    // Create the constant buffer
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBufferStruct)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBuffers[0])
        ));

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeof(UberBufferStruct)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBuffers[1])
        ));

        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_constantBuffers[0]->Map(0, &readRange, reinterpret_cast<void**>(&m_pConstantBufferDataBegin)));
        memcpy(m_pConstantBufferDataBegin, &m_constantBufferData, sizeof(ConstantBufferStruct));
        m_constantBuffers[0]->Unmap(0, &readRange);

        ThrowIfFailed(m_constantBuffers[1]->Map(0, &readRange, reinterpret_cast<void**>(&m_pUberBufferDataBegin)));
        memcpy(m_pUberBufferDataBegin, &m_uberBufferData, sizeof(UberBufferStruct));
        m_constantBuffers[1]->Unmap(0, &readRange);
    }

    // Create the constant buffer - SKYBOX
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBufferStruct)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBufferSkyboxMatrices)
        ));

        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_constantBufferSkyboxMatrices->Map(0, &readRange, reinterpret_cast<void**>(&m_pConstantBufferDataBeginSkybox)));
        memcpy(m_pConstantBufferDataBeginSkybox, &m_constantBufferData, sizeof(ConstantBufferStruct));
        m_constantBufferSkyboxMatrices->Unmap(0, &readRange);
    }


#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
    Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialize)) {
        throw std::exception();
    }
    // error
#else
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        throw std::exception();
    }
    // error
#endif

    ComPtr<ID3D12Resource> uploadHeap;

    // Create texture
    {
        CreateTextureRTCP(m_texture, m_commandList, L"Pebles.png", uploadHeap);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
        m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHandle);
    }

    ComPtr<ID3D12Resource> skyboxUploadHeap;

    // Create skybox texture
    {
        CreateTextureRTCP(m_skyboxTexture, m_commandListSkybox, L"Skyboxes/cubemap.dds", skyboxUploadHeap);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Texture2D.MipLevels = 1;

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
        m_device->CreateShaderResourceView(m_skyboxTexture.Get(), &srvDesc, srvHandle);
    }

    // Close the command list and execute it to begin the initial GPU setup.
    ThrowIfFailed(m_commandList->Close());
    ThrowIfFailed(m_commandListSkybox->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get(), m_commandListSkybox.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

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
        WaitForPreviousFrame();
    }
}

void Renderer::PopulateCommandList()
{
    if (DO_RAYTRACING)
    {
        ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

        ThrowIfFailed(m_commandAllocatorsSkybox[m_frameIndex]->Reset());
        ThrowIfFailed(m_commandListSkybox->Reset(m_commandAllocatorsSkybox[m_frameIndex].Get(), m_pipelineStateSkybox.Get()));

        D3D12_RESOURCE_BARRIER OutputBarriers[2] = {};

        // Transition the back buffer to a copy destination
        OutputBarriers[0].Transition.pResource = m_backBuffers[m_frameIndex].Get();
        OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        OutputBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        // Transition the DXR output buffer to a copy source
        OutputBarriers[1].Transition.pResource = m_dxrOutput.Get();
        OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        OutputBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        // Wait for the transitions to complete
        m_commandList->ResourceBarrier(2, OutputBarriers);

        // Set the UAV/SRV/CBV and sampler heaps
        ID3D12DescriptorHeap* ppHeaps[] = { m_dxrDescriptorHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        // Dispatch rays
        D3D12_DISPATCH_RAYS_DESC desc = {};
        desc.RayGenerationShaderRecord.StartAddress = m_shaderTable->GetGPUVirtualAddress();
        desc.RayGenerationShaderRecord.SizeInBytes = m_shaderTableRecordSize;

        desc.MissShaderTable.StartAddress = m_shaderTable->GetGPUVirtualAddress() + m_shaderTableRecordSize;
        desc.MissShaderTable.SizeInBytes = m_shaderTableRecordSize;		// Only a single Miss program entry
        desc.MissShaderTable.StrideInBytes = m_shaderTableRecordSize;

        desc.HitGroupTable.StartAddress = m_shaderTable->GetGPUVirtualAddress() + (m_shaderTableRecordSize * 2);
        desc.HitGroupTable.SizeInBytes = m_shaderTableRecordSize;			// Only a single Hit program entry
        desc.HitGroupTable.StrideInBytes = m_shaderTableRecordSize;

        desc.Width = m_windowWidth;
        desc.Height = m_windowHeight;
        desc.Depth = 1;

        m_commandList->SetPipelineState1(m_rtpso.Get());
        m_commandList->DispatchRays(&desc);

        // Transition DXR output to a copy source
        OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

        // Wait for the transitions to complete
        m_commandList->ResourceBarrier(1, &OutputBarriers[1]);

        // Copy the DXR output to the back buffer
        m_commandList->CopyResource(m_backBuffers[m_frameIndex].Get(), m_dxrOutput.Get());

        // Transition back buffer to present
        OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

        // Wait for the transitions to complete
        m_commandList->ResourceBarrier(1, &OutputBarriers[0]);

        ThrowIfFailed(m_commandList->Close());
        ThrowIfFailed(m_commandListSkybox->Close());
    }
    else
    {
        ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

        ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle0(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), 0, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle1(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), 1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
        m_commandList->SetGraphicsRootDescriptorTable(0, srvHandle0);

        m_commandList->SetGraphicsRootConstantBufferView(1, m_constantBuffers[0]->GetGPUVirtualAddress());

        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        // Indicate that the back buffer will be used as a render target.
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        // Record commands.
        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &m_modelSphere->GetVertexBufferView());
        m_commandList->DrawInstanced(m_modelSphere->GetIndicesCount(), 1, 0, 0);

        // Indicate that the back buffer will now be used to present.
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        //////////////////////
        // DRAW SKYBOX
        ThrowIfFailed(m_commandAllocatorsSkybox[m_frameIndex]->Reset());
        ThrowIfFailed(m_commandListSkybox->Reset(m_commandAllocatorsSkybox[m_frameIndex].Get(), m_pipelineStateSkybox.Get()));

        m_commandListSkybox->SetGraphicsRootSignature(m_rootSignatureSkybox.Get());
        m_commandListSkybox->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        m_commandListSkybox->SetGraphicsRootDescriptorTable(0, srvHandle1);

        m_commandListSkybox->SetGraphicsRootConstantBufferView(1, m_constantBufferSkyboxMatrices->GetGPUVirtualAddress());
        m_commandListSkybox->SetGraphicsRootConstantBufferView(2, m_constantBuffers[1]->GetGPUVirtualAddress());

        m_commandListSkybox->RSSetViewports(1, &m_viewport);
        m_commandListSkybox->RSSetScissorRects(1, &m_scissorRect);

        // Indicate that the back buffer will be used as a render target.
        m_commandListSkybox->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        m_commandListSkybox->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        // Record commands.
        m_commandListSkybox->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandListSkybox->IASetVertexBuffers(0, 1, &m_modelCube->GetVertexBufferView());
        m_commandListSkybox->DrawInstanced(m_modelCube->GetIndicesCount(), 1, 0, 0);

        // Indicate that the back buffer will now be used to present.
        m_commandListSkybox->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        //////////////////////

        ThrowIfFailed(m_commandList->Close());
        ThrowIfFailed(m_commandListSkybox->Close());
    }
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

void Renderer::CreateRootSignatureRTCP(UINT rootParamCount, UINT samplerCount, CD3DX12_ROOT_PARAMETER rootParameters[], CD3DX12_STATIC_SAMPLER_DESC samplers[], D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ComPtr<ID3D12RootSignature> &rootSignature)
{
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(rootParamCount, rootParameters, samplerCount, samplers, rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
}

void Renderer::CreateRootSignatureRTCP(UINT rootParamCount, UINT samplerCount, CD3DX12_ROOT_PARAMETER rootParameters[], CD3DX12_STATIC_SAMPLER_DESC samplers[], D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ID3D12RootSignature*& rootSignature)
{
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(rootParamCount, rootParameters, samplerCount, samplers, rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
}

CD3DX12_DEPTH_STENCIL_DESC1 Renderer::CreateDefaultDepthStencilDesc()
{
    CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc{};
    depthStencilDesc.DepthBoundsTestEnable = true;

    // Set up the description of the stencil state.
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    depthStencilDesc.StencilEnable = true;
    depthStencilDesc.StencilReadMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.StencilWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

    // Stencil operations if pixel is front-facing.
    depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_INCR;
    depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    // Stencil operations if pixel is back-facing.
    depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_DECR;
    depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    return depthStencilDesc;
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC Renderer::CreateDefaultPSO(BasicInputLayout inputElementDescs, ComPtr<ID3DBlob> vertexShader, ComPtr<ID3DBlob> pixelShader, D3D12_DEPTH_STENCIL_DESC depthStencilDesc, ComPtr<ID3D12RootSignature> rootSignature)
{
    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { &inputElementDescs[0], static_cast<UINT>(inputElementDescs.size()) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    return psoDesc;
}

BasicInputLayout Renderer::CreateBasicInputLayout()
{
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    BasicInputLayout arr;
    std::move(std::begin(inputElementDescs), std::end(inputElementDescs), arr.begin());

    return arr;
}

void Renderer::CreateTextureRTCP(ComPtr<ID3D12Resource>& texture, ComPtr<ID3D12GraphicsCommandList4> commandList, const wchar_t* path, ComPtr<ID3D12Resource>& uploadHeap)
{
    std::unique_ptr<uint8_t[]> decodedData;
    std::vector<D3D12_SUBRESOURCE_DATA> textureData;
    D3D12_SUBRESOURCE_DATA textureDataSingle;

    if (SUCCEEDED(LoadDDSTextureFromFileEx(m_device.Get(), path, 0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_DEFAULT, texture.ReleaseAndGetAddressOf(), decodedData, textureData))) 
    {
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, static_cast<UINT>(textureData.size()));

        // uploadHeap must outlive this function - until command list is closed
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadHeap)
        ));

        UpdateSubresources(commandList.Get(), texture.Get(), uploadHeap.Get(), 0, 0, static_cast<UINT>(textureData.size()), &textureData[0]);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    }
    else
    {
        ThrowIfFailed(LoadWICTextureFromFileEx(m_device.Get(), path, 0, D3D12_RESOURCE_FLAG_NONE, WIC_LOADER_FORCE_RGBA32, texture.ReleaseAndGetAddressOf(), decodedData, textureDataSingle));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);

        // uploadHeap must outlive this function - until command list is closed
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadHeap)
        ));

        UpdateSubresources(commandList.Get(), texture.Get(), uploadHeap.Get(), 0, 0, 1, &textureDataSingle);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    }
}

void Renderer::PrepareRaytracingResources()
{
    InitShaderCompiler(m_shaderCompiler);

    CreateBLAS(m_modelSphere);
    CreateTLAS();
    CreateDxrPipelineAssets(m_modelSphere.get());

    CreateRayGenShader(m_shaderCompiler);
    CreateMissShader(m_shaderCompiler);
    CreateClosestHitShader(m_shaderCompiler);

    CreateRTPSO();

    CreateShaderTable();
}

void Renderer::CreateBLAS(std::shared_ptr<ModelClass> model)
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
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Get the size requirements for the BLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.pGeometryDescs = &geometryDesc;
    ASInputs.NumDescs = 1;
    ASInputs.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);
    ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);

    // Create the BLAS scratch buffer
    auto bufferInfo = CD3DX12_RESOURCE_DESC::Buffer(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &bufferInfo,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_blasScratch)
    ));

    // Create the BLAS buffer
    bufferInfo = CD3DX12_RESOURCE_DESC::Buffer(ASPreBuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

    ThrowIfFailed(m_device->CreateCommittedResource(
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

    m_commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the BLAS build to complete
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_blasResult.Get()));
}

void Renderer::CreateTLAS()
{
    // Describe the TLAS geometry instance(s)
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    instanceDesc.InstanceID = 0;
    instanceDesc.InstanceContributionToHitGroupIndex = 0;
    instanceDesc.InstanceMask = 0xFF;
    instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
    instanceDesc.AccelerationStructure = m_blasResult->GetGPUVirtualAddress();
    instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

    // Create the TLAS instance buffer
    ThrowIfFailed(m_device->CreateCommittedResource(
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

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // Get the size requirements for the TLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
    ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ASInputs.InstanceDescs = m_tlasInstanceDesc->GetGPUVirtualAddress();
    ASInputs.NumDescs = 1;
    ASInputs.Flags = buildFlags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

    ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
    ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

    // Set TLAS size
    m_tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

    // Create TLAS scratch buffer
    auto bufferInfo = CD3DX12_RESOURCE_DESC::Buffer(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));
    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &bufferInfo,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_tlasScratch)
    ));

    bufferInfo = CD3DX12_RESOURCE_DESC::Buffer(ASPreBuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));
    ThrowIfFailed(m_device->CreateCommittedResource(
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

    m_commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the TLAS build to complete
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_tlasResult.Get()));
}

void Renderer::CreateDxrPipelineAssets(ModelClass* model)
{
    // Create DXR ouput resource - 2d texture
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.DepthOrArraySize = 1;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        desc.Width = m_windowWidth;
        desc.Height = m_windowHeight;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            nullptr,
            IID_PPV_ARGS(&m_dxrOutput)
        ));
    }

    // Create view buffer and material buffer
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneConstantBuffer) + 255) & ~255), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_sceneBuffer)
        ));

        {
            CD3DX12_RANGE readRange(0, 0);
            ThrowIfFailed(m_sceneBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_sceneBufferDataBegin)));
            memcpy(m_sceneBufferDataBegin, &m_sceneBufferData, sizeof(SceneConstantBuffer));
            m_sceneBuffer->Unmap(0, &readRange);
        }

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer((sizeof(CubeConstantBuffer) + 255) & ~255), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_cubeBuffer)
        ));

        {
            CD3DX12_RANGE readRange(0, 0);
            ThrowIfFailed(m_cubeBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cubeBufferDataBegin)));
            memcpy(m_cubeBufferDataBegin, &m_cubeBufferData, sizeof(SceneConstantBuffer));
            m_cubeBuffer->Unmap(0, &readRange);
        }
    }

    // Create descriptor heaps
    {
        // Describe the CBV/SRV/UAV heap
         // Need 6 entries:
         // 1 CBV for the SceneCB       : b0
         // 1 CBV for the CubeCB        : b1
         // 1 UAV for the RT output     : u0
         // 1 SRV for the Scene BVH     : t0
         // 1 SRV for the index buffer  : t1
         // 1 SRV for the vertex buffer : t2
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 6;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        // Create the descriptor heap
        ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dxrDescriptorHeap)));

        // Get the descriptor heap handle and increment size
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_dxrDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        UINT handleIncrement = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Create the SceneCB CBV
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(m_sceneBufferData));
        cbvDesc.BufferLocation = m_sceneBuffer->GetGPUVirtualAddress();

        m_device->CreateConstantBufferView(&cbvDesc, handle);

        // Create the CubeCB CBV
        cbvDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(m_cubeBufferData));
        cbvDesc.BufferLocation = m_cubeBuffer->GetGPUVirtualAddress();

        handle.ptr += handleIncrement;
        m_device->CreateConstantBufferView(&cbvDesc, handle); 

        // Create the DXR output buffer UAV
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        handle.ptr += handleIncrement;
        m_device->CreateUnorderedAccessView(m_dxrOutput.Get(), nullptr, &uavDesc, handle);

        // Create the DXR Top Level Acceleration Structure SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = m_tlasResult->GetGPUVirtualAddress();

        handle.ptr += handleIncrement;
        m_device->CreateShaderResourceView(nullptr, &srvDesc, handle);

        // Create the index buffer SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC indexSRVDesc;
        indexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        indexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        indexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        indexSRVDesc.Buffer.StructureByteStride = 0;
        indexSRVDesc.Buffer.FirstElement = 0;
        indexSRVDesc.Buffer.NumElements = (model->GetIndicesCount() * sizeof(UINT)) / sizeof(float);
        indexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        handle.ptr += handleIncrement;
        m_device->CreateShaderResourceView(model->GetIndexBuffer().Get(), &indexSRVDesc, handle);

        // Create the vertex buffer SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC vertexSRVDesc;
        vertexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        vertexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        vertexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        vertexSRVDesc.Buffer.StructureByteStride = 0;
        vertexSRVDesc.Buffer.FirstElement = 0;
        vertexSRVDesc.Buffer.NumElements = (static_cast<UINT>(model->GetMesh(0).vertices.size()) * sizeof(ModelClass::VertexBufferStruct)) / sizeof(float);
        vertexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        handle.ptr += handleIncrement;
        m_device->CreateShaderResourceView(model->GetVertexBuffer().Get(), &vertexSRVDesc, handle);
    }
}

void Renderer::CreateRayGenShader(D3D12ShaderCompilerInfo& shaderCompiler)
{
    // Load and compile the ray generation shader
    m_rayGenShader = RtProgram(D3D12ShaderInfo(L"Shaders/RayGen.hlsl", L"", L"lib_6_3"));
    Compile_Shader(shaderCompiler, m_rayGenShader);

    // Describe the ray generation root signature
    D3D12_DESCRIPTOR_RANGE ranges[3];

    ranges[0].BaseShaderRegister = 0;
    ranges[0].NumDescriptors = 2;
    ranges[0].RegisterSpace = 0;
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    ranges[1].BaseShaderRegister = 0;
    ranges[1].NumDescriptors = 1;
    ranges[1].RegisterSpace = 0;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].OffsetInDescriptorsFromTableStart = 2;

    ranges[2].BaseShaderRegister = 0;
    ranges[2].NumDescriptors = 3;
    ranges[2].RegisterSpace = 0;
    ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[2].OffsetInDescriptorsFromTableStart = 3;

    CD3DX12_ROOT_PARAMETER param0 = {};
    param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param0.DescriptorTable.NumDescriptorRanges = _countof(ranges);
    param0.DescriptorTable.pDescriptorRanges = ranges;

    CD3DX12_ROOT_PARAMETER rootParams[1]{ param0 };

    // Create the root signature
    CreateRootSignatureRTCP(_countof(rootParams), 0, rootParams, NULL, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE, m_rayGenShader.pRootSignature);
}

void Renderer::InitShaderCompiler(D3D12ShaderCompilerInfo& shaderCompiler)
{
    ThrowIfFailed(shaderCompiler.DxcDllHelper.Initialize());
    ThrowIfFailed(shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &shaderCompiler.compiler));
    ThrowIfFailed(shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &shaderCompiler.library));
}

void Renderer::Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, D3D12ShaderInfo& info, IDxcBlob** blob)
{
    HRESULT hr;
    UINT32 code(0);
    IDxcBlobEncoding* pShaderText(nullptr);

    // Load and encode the shader file
    ThrowIfFailed(compilerInfo.library->CreateBlobFromFile(info.filename, &code, &pShaderText));

    // Create the compiler include handler
    ComPtr<IDxcIncludeHandler> dxcIncludeHandler;
    ThrowIfFailed(compilerInfo.library->CreateIncludeHandler(&dxcIncludeHandler));

    // Compile the shader
    IDxcOperationResult* result;
    ThrowIfFailed(compilerInfo.compiler->Compile(
        pShaderText,
        info.filename,
        info.entryPoint,
        info.targetProfile,
        info.arguments,
        info.argCount,
        info.defines,
        info.defineCount,
        dxcIncludeHandler.Get(),
        &result
    ));

    // Verify the result
    result->GetStatus(&hr);
    if (FAILED(hr))
    {
        IDxcBlobEncoding* error;
        ThrowIfFailed(result->GetErrorBuffer(&error));

        // Convert error blob to a string
        std::vector<char> infoLog(error->GetBufferSize() + 1);
        memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
        infoLog[error->GetBufferSize()] = 0;

        std::string errorMsg = "Shader Compiler Error:\n";
        errorMsg.append(infoLog.data());

        MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
        return;
    }

    ThrowIfFailed(result->GetResult(blob));
}

void Renderer::Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, RtProgram& program)
{
    Compile_Shader(compilerInfo, program.info, &program.blob);
    program.SetBytecode();
}

void Renderer::CreateMissShader(D3D12ShaderCompilerInfo& shaderCompiler)
{
    // Load and compile the miss shader
    m_missShader = RtProgram(D3D12ShaderInfo(L"Shaders/Miss.hlsl", L"", L"lib_6_3"));
    Compile_Shader(shaderCompiler, m_missShader);
}

void Renderer::CreateClosestHitShader(D3D12ShaderCompilerInfo& shaderCompiler)
{
    // Load and compile the Closest Hit shader
    m_hitShader = HitProgram(L"Hit");
    m_hitShader.chs = RtProgram(D3D12ShaderInfo(L"Shaders/ClosestHit.hlsl", L"", L"lib_6_3"));
    Compile_Shader(shaderCompiler, m_hitShader.chs);
}

void Renderer::CreateShaderTable()
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
    ThrowIfFailed(m_device->CreateCommittedResource(
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
    memcpy(pData, m_rtpsoInfo->GetShaderIdentifier(L"RayGen_12"), shaderIdSize);

    // Set the root parameter data. Point to start of descriptor heap.
    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = m_dxrDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // Shader Record 1 - Miss program (no local root arguments to set)
    pData += m_shaderTableRecordSize;
    memcpy(pData, m_rtpsoInfo->GetShaderIdentifier(L"Miss_5"), shaderIdSize);

    // Shader Record 2 - Closest Hit program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
    pData += m_shaderTableRecordSize;
    memcpy(pData, m_rtpsoInfo->GetShaderIdentifier(L"HitGroup"), shaderIdSize);

    // Set the root parameter data. Point to start of descriptor heap.
    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = m_dxrDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // Unmap
    m_shaderTable->Unmap(0, nullptr);
}

void Renderer::CreateRTPSO()
{
    // Need 10 subobjects:
    // 1 for RGS program
    // 1 for Miss program
    // 1 for CHS program
    // 1 for Hit Group
    // 2 for RayGen Root Signature (root-signature and association)
    // 2 for Shader Config (config and association)
    // 1 for Global Root Signature
    // 1 for Pipeline Config	
    UINT index = 0;
    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.resize(10);

    // Add state subobject for the RGS
    D3D12_EXPORT_DESC rgsExportDesc = {};
    rgsExportDesc.Name = L"RayGen_12";
    rgsExportDesc.ExportToRename = L"RayGen";
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
    msExportDesc.Name = L"Miss_5";
    msExportDesc.ExportToRename = L"Miss";
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
    chsExportDesc.Name = L"ClosestHit_76";
    chsExportDesc.ExportToRename = L"ClosestHit";
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

    // Add a state subobject for the hit group
    D3D12_HIT_GROUP_DESC hitGroupDesc = {};
    hitGroupDesc.ClosestHitShaderImport = L"ClosestHit_76";
    hitGroupDesc.HitGroupExport = L"HitGroup";

    D3D12_STATE_SUBOBJECT hitGroup = {};
    hitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    hitGroup.pDesc = &hitGroupDesc;

    subobjects[index++] = hitGroup;

    // Add a state subobject for the shader payload configuration
    D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
    shaderDesc.MaxPayloadSizeInBytes = sizeof(XMFLOAT4);	// RGB and HitT
    shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;

    D3D12_STATE_SUBOBJECT shaderConfigObject = {};
    shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigObject.pDesc = &shaderDesc;

    subobjects[index++] = shaderConfigObject;

    // Create a list of the shader export names that use the payload
    const WCHAR* shaderExports[] = { L"RayGen_12", L"Miss_5", L"HitGroup" };

    // Add a state subobject for the association between shaders and the payload
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
    shaderPayloadAssociation.NumExports = _countof(shaderExports);
    shaderPayloadAssociation.pExports = shaderExports;
    shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

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
    const WCHAR* rootSigExports[] = { L"RayGen_12", L"HitGroup", L"Miss_5" };

    // Add a state subobject for the association between the RayGen shader and the local root signature
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenShaderRootSigAssociation = {};
    rayGenShaderRootSigAssociation.NumExports = _countof(rootSigExports);
    rayGenShaderRootSigAssociation.pExports = rootSigExports;
    rayGenShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

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
    ThrowIfFailed(m_device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&m_rtpso)));

    // Get the RTPSO properties
    ThrowIfFailed(m_rtpso->QueryInterface(IID_PPV_ARGS(&m_rtpsoInfo)));
}
