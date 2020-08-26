#include "Renderer.h"
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
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

    m_scissorRect.left = 0;
    m_scissorRect.top = 0;
    m_scissorRect.right = static_cast<LONG>(m_windowWidth);
    m_scissorRect.bottom = static_cast<LONG>(m_windowHeight);

    m_cameraPosition = XMFLOAT3{ 0.1f, 1.4f, -3.7f };
    m_cameraRotation = XMFLOAT3{ 10.5f, -29.5f, 0.0f };
    CreateViewAndPerspective();
}

void Renderer::OnRender()
{
    // Prepare commands to execute
    //PopulateCommandList(); // Moved to main.cpp

    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get(), m_commandListSkybox.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    ThrowIfFailed(m_swapChain->Present(1, 0));
    MoveToNextFrame();
}

void Renderer::OnInit(HWND hwnd)
{
    // Preparing devices, resources, views to enable rendering
    LoadPipeline(hwnd);
    LoadAssets();

    auto now = std::chrono::high_resolution_clock::now();
    auto msTime = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    m_rng = std::mt19937(uint32_t(msTime.time_since_epoch().count()));
}

void Renderer::OnUpdate()
{
    XMMATRIX lastFrameView = m_constantBuffer.value.view;

    CreateViewAndPerspective();
    // Update vertexShader.hlsl
    {
        // Update data for single rasterized object
        m_constantBuffer.value.world = XMMatrixIdentity();
        m_constantBuffer.Update();

        // Update data for skybox rendering
        m_constantBufferSkybox.value = m_constantBuffer.value;
        XMFLOAT3 pos = XMFLOAT3{ m_cameraPosition.x - 0.5f, m_cameraPosition.y - 0.5f, m_cameraPosition.z - 0.5f };
        m_constantBufferSkybox.value.world = XMMatrixIdentity();
        m_constantBufferSkybox.value.world = XMMatrixMultiply(m_constantBufferSkybox.value.world, XMMatrixScaling(1.0f, 1.0f, 1.0f));
        m_constantBufferSkybox.value.world = XMMatrixMultiply(m_constantBufferSkybox.value.world, XMMatrixTranslation(pos.x, pos.y, pos.z));
        m_constantBufferSkybox.value.world = XMMatrixTranspose(m_constantBufferSkybox.value.world);
        m_constantBufferSkybox.Update();
    }
    // Update raygeneration cbuffer - scene CB
    {
        XMMATRIX viewProj = XMMatrixMultiply(XMMatrixTranspose(m_constantBuffer.value.view), XMMatrixTranspose(m_constantBuffer.value.projection));
        m_sceneBuffer.value.projectionToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));
        m_sceneBuffer.value.frameCount++;
        m_sceneBuffer.value.frameCount %= INT_MAX;
        m_sceneBuffer.Update();
    }
    // Update ao cbuffer and camera cbuffer - ao CB/camera CB
    {        
        for (int i = 0; i < 4; ++i) 
        {
            for (int j = 0; j < 4; ++j)
            {
                if (lastFrameView.r[i].m128_f32[j] != m_constantBuffer.value.view.r[i].m128_f32[j])
                {
                    m_resetFrameAO = true;
                    break;
                }
            }
            if (m_resetFrameAO)
            {
                break;
            }
        }

        if (!m_resetFrameAO)
        {
            m_aoBuffer.value.accFrames++;
        }
        else
        {
            m_resetFrameAO = false;
            m_aoBuffer.value.accFrames = 0;
        }

        if (USE_AO_FRAME_JITTER)
        {
            float x = (m_rngDist(m_rng) - 0.5f);// / static_cast<float>(m_windowWidth);
            float y = (m_rngDist(m_rng) - 0.5f);// / static_cast<float>(m_windowHeight);
            m_cameraBuffer.value.jitterCamera = { x, y };
        }
        else
        {
            m_cameraBuffer.value.jitterCamera = { 0.5f, 0.5f };
        }

        m_cameraBuffer.value.lensRadius = USE_AO_THIN_LENS ? m_cameraBuffer.value.focalLength / (2.0f * m_cameraBuffer.value.fNumber) : 0.0f;

        constexpr float conv{ 0.0174532925f };
        const XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(m_cameraRotation.x * conv, m_cameraRotation.y * conv, m_cameraRotation.z * conv);
        const XMMATRIX YrotationMatrix = XMMatrixRotationY(m_cameraRotation.y * conv);
        const XMVECTOR camRight = XMVector3TransformCoord(XMVECTOR{ 1,0,0,0 }, YrotationMatrix);
        const XMVECTOR camForward = XMVector3TransformCoord(XMVECTOR{ 0, 0, 1, 0 }, rotationMatrix);
        const XMVECTOR camUp = XMVector3Cross(camRight, camForward);

        m_cameraBuffer.value.cameraU = XMFLOAT4{ camRight.m128_f32[0], camRight.m128_f32[1], camRight.m128_f32[2], 1 };
        m_cameraBuffer.value.cameraV = XMFLOAT4{ camUp.m128_f32[0], camUp.m128_f32[1], camUp.m128_f32[2], 1 };
        m_cameraBuffer.value.cameraW = XMFLOAT4{ camForward.m128_f32[0], camForward.m128_f32[1], camForward.m128_f32[2], 1 };

        //m_aoBuffer.value.padding = {};
        m_aoBuffer.Update();
        m_cameraBuffer.Update();
    }
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
        m_resetFrameAO = true;
    }
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
        // Avoid gimbal lock problem, by avoiding reaching 90 deg
        m_cameraRotation.x = m_cameraRotation.x >= 90.0f ? 89.9f : (m_cameraRotation.x <= -90.0f ? -89.9f : m_cameraRotation.x);

        CreateViewAndPerspective();
        m_resetFrameAO = true;
    }
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
        Compile_Shader(L"Shaders/vertexShader.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_1", 0, 0, &vertexShader);
        Compile_Shader(L"Shaders/pixelShader.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_1", 0, 0, &pixelShader);

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
        Compile_Shader(L"Shaders/VS_Skybox.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_1", 0, 0, &vertexShader);
        Compile_Shader(L"Shaders/PS_Skybox.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_1", 0, 0, &pixelShader);

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

    ComPtr<ID3D12Resource> modelHeap;
    // Create the vertex buffer.
    {
        m_modelCube = std::shared_ptr<ModelClass>(new ModelClass("cube.obj", m_device, m_commandList));
        m_modelSphere = std::shared_ptr<ModelClass>(new ModelClass("sphere.obj", m_device, m_commandList));
        //m_modelBuddha = std::shared_ptr<ModelClass>(new ModelClass("happy-buddha.fbx", m_device, m_commandList));
        m_modelPinkRoom = std::shared_ptr<ModelClass>(new ModelClass("pink_room.fbx", m_device, m_commandList, modelHeap));
    }

    // Fill structure data
    InitializeRaytracingBufferValues();

    // Prepare shader compilator
    InitShaderCompiler(m_shaderCompiler);

    // Create the constant buffer
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBufferStruct)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBuffer.resource)
        ));

        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_constantBuffer.resource->Map(0, &readRange, reinterpret_cast<void**>(&m_constantBuffer.ptr)));
        memcpy(m_constantBuffer.ptr, &m_constantBuffer.value, sizeof(ConstantBufferStruct));
        m_constantBuffer.resource->Unmap(0, &readRange);
    }

    // Create the constant buffer - SKYBOX
    {
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBufferStruct)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBufferSkybox.resource)
        ));

        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_constantBufferSkybox.resource->Map(0, &readRange, reinterpret_cast<void**>(&m_constantBufferSkybox.ptr)));
        memcpy(m_constantBufferSkybox.ptr, &m_constantBufferSkybox.value, sizeof(ConstantBufferStruct));
        m_constantBufferSkybox.resource->Unmap(0, &readRange);
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
    // Create texture for rasterized object
    {
        CreateTextureFromFileRTCP(m_pebblesTexture, m_commandList, L"Pebles.png", uploadHeap, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        CreateSRV_Texture2DArray(m_modelPinkRoom->GetTextureResources(), m_srvHeap.Get(), 0, m_device.Get());
    }

    ComPtr<ID3D12Resource> skyboxUploadHeap;
    // Create skybox texture
    {
        CreateTextureFromFileRTCP(m_skyboxTexture, m_commandListSkybox, L"Skyboxes/cubemap.dds", skyboxUploadHeap);
        CreateSRV_TextureCube(m_skyboxTexture, m_srvHeap.Get(), 1, m_device.Get());
    }

    // Preparation of raytracing
    const std::shared_ptr<ModelClass> model = m_modelPinkRoom;
    PrepareRaytracingResources(model);
    PrepareRaytracingResourcesAO(model);
    PrepareRaytracingResourcesLambert(model);

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

        // Wait for the transitions to complete
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_rtAoTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_rtLambertTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

        // Set the UAV/SRV/CBV and sampler heaps
        ID3D12DescriptorHeap* ppHeaps[] = { m_raytracingNormal->GetDescriptorHeap().Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        ComPtr<ID3D12Resource> shaderTable = m_raytracingNormal->GetShaderTable();
        uint32_t shaderTableRecordSize = m_raytracingNormal->GetShaderTableRecordSize();
        ComPtr<ID3D12StateObject> rtpso = m_raytracingNormal->GetRTPSO();

        // Dispatch rays
        D3D12_DISPATCH_RAYS_DESC desc = {};
        desc.RayGenerationShaderRecord.StartAddress = shaderTable->GetGPUVirtualAddress();
        desc.RayGenerationShaderRecord.SizeInBytes = shaderTableRecordSize;

        desc.MissShaderTable.StartAddress = shaderTable->GetGPUVirtualAddress() + shaderTableRecordSize;
        desc.MissShaderTable.SizeInBytes = shaderTableRecordSize;		// Only a single Miss program entry
        desc.MissShaderTable.StrideInBytes = shaderTableRecordSize;

        desc.HitGroupTable.StartAddress = shaderTable->GetGPUVirtualAddress() + (shaderTableRecordSize * 2);
        desc.HitGroupTable.SizeInBytes = shaderTableRecordSize;			// Only a single Hit program entry
        desc.HitGroupTable.StrideInBytes = shaderTableRecordSize;

        desc.Width = m_windowWidth;
        desc.Height = m_windowHeight;
        desc.Depth = 1;

        m_commandList->SetPipelineState1(rtpso.Get());
        m_commandList->DispatchRays(&desc);

        /* RTAO NOISE ALGORITHM */
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_rtNormalTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

         //Set the UAV/SRV/CBV and sampler heaps
        {
            ID3D12DescriptorHeap* ppHeaps[] = { m_raytracingAO->GetDescriptorHeap().Get() };
            m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

            ComPtr<ID3D12Resource> shaderTable = m_raytracingAO->GetShaderTable();
            uint32_t shaderTableRecordSize = m_raytracingAO->GetShaderTableRecordSize();
            ComPtr<ID3D12StateObject> rtpso = m_raytracingAO->GetRTPSO();

            // Dispatch rays
            D3D12_DISPATCH_RAYS_DESC desc = {};
            desc.RayGenerationShaderRecord.StartAddress = shaderTable->GetGPUVirtualAddress();
            desc.RayGenerationShaderRecord.SizeInBytes = shaderTableRecordSize;

            desc.MissShaderTable.StartAddress = shaderTable->GetGPUVirtualAddress() + shaderTableRecordSize;
            desc.MissShaderTable.SizeInBytes = shaderTableRecordSize;		// Only a single Miss program entry
            desc.MissShaderTable.StrideInBytes = shaderTableRecordSize;

            desc.HitGroupTable.StartAddress = shaderTable->GetGPUVirtualAddress() + (shaderTableRecordSize * 2);
            desc.HitGroupTable.SizeInBytes = shaderTableRecordSize;			// Only a single Hit program entry
            desc.HitGroupTable.StrideInBytes = shaderTableRecordSize;

            desc.Width = m_windowWidth;
            desc.Height = m_windowHeight;
            desc.Depth = 1;

            m_commandList->SetPipelineState1(rtpso.Get());
            m_commandList->DispatchRays(&desc);
        }
        //// Transition DXR output to a copy source
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_rtAoTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));
        m_commandList->CopyResource(m_backBuffers[m_frameIndex].Get(), m_rtAoTexture.Get());
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

        /* RTAO LAMBERT ALGORITHM */
        //Set the UAV/SRV/CBV and sampler heaps
        {
            ID3D12DescriptorHeap* ppHeaps[] = { m_raytracingLambert->GetDescriptorHeap().Get() };
            m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

            ComPtr<ID3D12Resource> shaderTable = m_raytracingLambert->GetShaderTable();
            uint32_t shaderTableRecordSize = m_raytracingLambert->GetShaderTableRecordSize();
            ComPtr<ID3D12StateObject> rtpso = m_raytracingLambert->GetRTPSO();

            // Dispatch rays
            D3D12_DISPATCH_RAYS_DESC desc = {};
            desc.RayGenerationShaderRecord.StartAddress = shaderTable->GetGPUVirtualAddress();
            desc.RayGenerationShaderRecord.SizeInBytes = shaderTableRecordSize;

            desc.MissShaderTable.StartAddress = shaderTable->GetGPUVirtualAddress() + shaderTableRecordSize;
            desc.MissShaderTable.SizeInBytes = shaderTableRecordSize;		// Only a single Miss program entry
            desc.MissShaderTable.StrideInBytes = shaderTableRecordSize;

            desc.HitGroupTable.StartAddress = shaderTable->GetGPUVirtualAddress() + (shaderTableRecordSize * 2);
            desc.HitGroupTable.SizeInBytes = shaderTableRecordSize;			// Only a single Hit program entry
            desc.HitGroupTable.StrideInBytes = shaderTableRecordSize;

            desc.Width = m_windowWidth;
            desc.Height = m_windowHeight;
            desc.Depth = 1;

            m_commandList->SetPipelineState1(rtpso.Get());
            m_commandList->DispatchRays(&desc);
        }
        // Transition DXR output to a copy source
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_rtLambertTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));
        m_commandList->CopyResource(m_backBuffers[m_frameIndex].Get(), m_rtLambertTexture.Get());
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_rtNormalTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
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

        m_commandList->SetGraphicsRootConstantBufferView(1, m_constantBuffer.resource->GetGPUVirtualAddress());
        //m_commandList->SetGraphicsRootConstantBufferView(2, m_constantBuffers[1]->GetGPUVirtualAddress());

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
        m_commandList->IASetVertexBuffers(0, 1, &m_modelPinkRoom->GetVertexBufferView());
        m_commandList->DrawInstanced(m_modelPinkRoom->GetIndicesCount(), 1, 0, 0);

        // Indicate that the back buffer will now be used to present.
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        //////////////////////
        // DRAW SKYBOX
        ThrowIfFailed(m_commandAllocatorsSkybox[m_frameIndex]->Reset());
        ThrowIfFailed(m_commandListSkybox->Reset(m_commandAllocatorsSkybox[m_frameIndex].Get(), m_pipelineStateSkybox.Get()));

        m_commandListSkybox->SetGraphicsRootSignature(m_rootSignatureSkybox.Get());
        m_commandListSkybox->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        m_commandListSkybox->SetGraphicsRootDescriptorTable(0, srvHandle1);

        m_commandListSkybox->SetGraphicsRootConstantBufferView(1, m_constantBufferSkybox.resource->GetGPUVirtualAddress());
        m_commandListSkybox->SetGraphicsRootConstantBufferView(2, m_sceneBuffer.resource->GetGPUVirtualAddress());

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
    }
}

void Renderer::CloseCommandList()
{
    ThrowIfFailed(m_commandList->Close());
    ThrowIfFailed(m_commandListSkybox->Close());
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
        m_sceneBuffer.value.cameraPosition = XMFLOAT4{ m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z, 1.0f };
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
    m_constantBuffer.value.view = DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(eye, target, up));

    //Create perspective matrix
    constexpr float FOV = 3.14f / 4.0f;
    m_constantBuffer.value.projection = DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(FOV, m_aspectRatio, Z_NEAR, Z_FAR));
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

void Renderer::CreateSRV(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvHeap->GetCPUDescriptorHandleForHeapStart(), srvIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
    device->CreateShaderResourceView(resource.Get(), &srvDesc, srvHandle);
}

void Renderer::CreateSRV_Texture2D(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc)
{
    srvDesc.Texture2D.MipLevels = mipLevels;
    CreateSRV(resource, srvHeap, srvIndex, device, srvDesc);
}

void Renderer::CreateSRV_Texture2DArray(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc)
{
    srvDesc.Texture2DArray.MipLevels = mipLevels;
    srvDesc.Texture2DArray.ArraySize = 36;
    CreateSRV(resource, srvHeap, srvIndex, device, srvDesc);
}

void Renderer::CreateSRV_Texture3D(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc)
{
    srvDesc.Texture3D.MipLevels = mipLevels;
    CreateSRV(resource, srvHeap, srvIndex, device, srvDesc);
}

void Renderer::CreateSRV_TextureCube(ComPtr<ID3D12Resource>& resource, ID3D12DescriptorHeap* srvHeap, int srvIndex, ID3D12Device* device, int mipLevels, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc)
{
    srvDesc.TextureCube.MipLevels = mipLevels;
    CreateSRV(resource, srvHeap, srvIndex, device, srvDesc);
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

D3D12_SHADER_RESOURCE_VIEW_DESC Renderer::GetVertexBufferSRVDesc(ModelClass* model, UINT vertexStructSize)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC vertexSRVDesc;
    vertexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    vertexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    vertexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    vertexSRVDesc.Buffer.StructureByteStride = 0;
    vertexSRVDesc.Buffer.FirstElement = 0;
    vertexSRVDesc.Buffer.NumElements = model == nullptr ? 0 : (static_cast<UINT>(model->GetVerticesCount()) * vertexStructSize) / sizeof(float);
    vertexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    return vertexSRVDesc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC Renderer::GetIndexBufferSRVDesc(ModelClass* model)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC indexSRVDesc;
    indexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    // Application uses only R16 or R32
    if (model == nullptr) {
        indexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    } else {
        indexSRVDesc.Format = model->GetIndexBufferView().Format == DXGI_FORMAT_R32_UINT ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_R16_TYPELESS;
    }
    indexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    indexSRVDesc.Buffer.StructureByteStride = 0;
    indexSRVDesc.Buffer.FirstElement = 0;
    indexSRVDesc.Buffer.NumElements = model == nullptr ? 0 : (model->GetIndicesCount() * sizeof(UINT)) / sizeof(float);
    indexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    return indexSRVDesc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC Renderer::GetAccelerationStructureDesc(ComPtr<ID3D12Resource>& tlasResult)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = tlasResult->GetGPUVirtualAddress();

    return srvDesc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC Renderer::GetAccessViewDesc(DXGI_FORMAT Format, D3D12_UAV_DIMENSION ViewDimension)
{
    return D3D12_UNORDERED_ACCESS_VIEW_DESC{ Format, ViewDimension };
}

template<class T>
D3D12_CONSTANT_BUFFER_VIEW_DESC Renderer::GetConstantBufferViewDesc(CBuffer<T> cbuffer)
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { cbuffer.resource->GetGPUVirtualAddress(), ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(cbuffer.value)) };
    return cbvDesc;
}

BasicInputLayout Renderer::CreateBasicInputLayout()
{
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    BasicInputLayout arr;
    std::move(std::begin(inputElementDescs), std::end(inputElementDescs), arr.begin());

    return arr;
}

void Renderer::CreateTexture2D(ComPtr<ID3D12Resource>& texture, UINT64 width, UINT height, DXGI_FORMAT format, D3D12_TEXTURE_LAYOUT layout, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES InitialResourceState)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.DepthOrArraySize = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Format = format;
    desc.Flags = flags;
    desc.Width = width;
    desc.Height = height;
    desc.Layout = layout;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &desc,
        InitialResourceState,
        nullptr,
        IID_PPV_ARGS(&texture)
    ));
}

void Renderer::CreateTextureFromFileRTCP(ComPtr<ID3D12Resource>& texture, ComPtr<ID3D12GraphicsCommandList4> commandList, const wchar_t* path, ComPtr<ID3D12Resource>& uploadHeap, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES InitialResourceState)
{
    std::unique_ptr<uint8_t[]> decodedData;
    std::vector<D3D12_SUBRESOURCE_DATA> textureData;
    D3D12_SUBRESOURCE_DATA textureDataSingle;

    if (SUCCEEDED(LoadDDSTextureFromFileEx(m_device.Get(), path, 0, flags, DDS_LOADER_DEFAULT, texture.ReleaseAndGetAddressOf(), decodedData, textureData)))
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
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, InitialResourceState));
    }
    else
    {
        ThrowIfFailed(LoadWICTextureFromFileEx(m_device.Get(), path, 0, flags, WIC_LOADER_FORCE_RGBA32, texture.ReleaseAndGetAddressOf(), decodedData, textureDataSingle));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);
        auto desc = texture->GetDesc();

        // uploadHeap must outlive this function - until command list is closed
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadHeap)
        ));

        //CreateTexture2D(texture, desc.Width, desc.Height, desc.Format, desc.Layout, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
        UpdateSubresources(commandList.Get(), texture.Get(), uploadHeap.Get(), 0, 0, 1, &textureDataSingle);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, InitialResourceState));
    }
}

void Renderer::PrepareRaytracingResources(const std::shared_ptr<ModelClass> model)
{
    RtProgram rayGenShader, missShader;
    HitProgram hitShader;

    std::vector<CD3DX12_STATIC_SAMPLER_DESC> samplers(1);
    samplers[0].Init(0, D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR);

    CreateRayGenShader(rayGenShader, m_shaderCompiler, L"Shaders/RT_gBuffer.hlsl", 2, 2, 5, samplers, L"RayGen");
    CreateMissShader(missShader, m_shaderCompiler, L"Shaders/RT_gBuffer.hlsl", L"Miss");
    CreateClosestHitShader(hitShader, m_shaderCompiler, L"Shaders/RT_gBuffer.hlsl", L"ClosestHit");
    m_raytracingNormal = std::shared_ptr<RaytracingResources>(new RaytracingResources(m_device.Get(), m_commandList, model, rayGenShader, missShader, hitShader, L"Group_gBuffer"));

    std::vector<TextureWithDesc> textures{};
    CreateTexture2D(m_rtNormalTexture, m_windowWidth, m_windowHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    CreateTexture2D(m_rtAlbedoTexture, m_windowWidth, m_windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    textures.push_back({ TextureWithDesc{m_rtNormalTexture, GetAccessViewDesc(DXGI_FORMAT_UNKNOWN, D3D12_UAV_DIMENSION_TEXTURE2D)} });
    textures.push_back({ TextureWithDesc{m_rtAlbedoTexture, GetAccessViewDesc(DXGI_FORMAT_UNKNOWN, D3D12_UAV_DIMENSION_TEXTURE2D)} });
    // SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2DARRAY, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING };
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.ArraySize = 36;
    textures.push_back({ TextureWithDesc{m_modelPinkRoom->GetTextureResources(), srvDesc } });

    D3D12_SHADER_RESOURCE_VIEW_DESC skyboxDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURECUBE, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING };
    skyboxDesc.TextureCube.MipLevels = 1;
    skyboxDesc.TextureCube.MostDetailedMip = 0;
    textures.push_back({ TextureWithDesc{m_skyboxTexture, skyboxDesc } });

    CreateRaytracingPipeline(m_raytracingNormal.get(), m_device.Get(), model.get(), textures, GetIndexBufferSRVDesc(model.get()), GetVertexBufferSRVDesc(model.get(), sizeof(ModelClass::VertexBufferStruct)), m_sceneBuffer, m_cameraBuffer, { }, sizeof(XMFLOAT4) * 2);
}

void Renderer::PrepareRaytracingResourcesAO(const std::shared_ptr<ModelClass> model)
{
    RtProgram rayGenShader, missShader;
    HitProgram hitShader;

    CreateRayGenShader(rayGenShader, m_shaderCompiler, L"Shaders/RT_AO.hlsl", 3, 1, 5, {}, L"RayGen");
    CreateMissShader(missShader, m_shaderCompiler, L"Shaders/RT_AO.hlsl", L"Miss");
    CreateClosestHitShader(hitShader, m_shaderCompiler, L"Shaders/RT_AO.hlsl", L"ClosestHit");
    //CreateAnyHitShader(hitShader, m_shaderCompiler, L"Shaders/RT_AO.hlsl", L"AnyHit_AO");
    m_raytracingAO = std::shared_ptr<RaytracingResources>(new RaytracingResources(m_device.Get(), m_commandList, model, rayGenShader, missShader, hitShader, L"GroupAO"));

    std::vector<TextureWithDesc> textures{};
    CreateTexture2D(m_rtAoTexture, m_windowWidth, m_windowHeight);
    textures.push_back({ TextureWithDesc{m_rtAoTexture, GetAccessViewDesc(DXGI_FORMAT_UNKNOWN, D3D12_UAV_DIMENSION_TEXTURE2D)} });

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING };
    D3D12_SHADER_RESOURCE_VIEW_DESC normalDesc = { DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING };
    srvDesc.Texture2D.MipLevels = 1;
    normalDesc.Texture2D.MipLevels = 1;
    textures.push_back({ TextureWithDesc{m_rtNormalTexture, normalDesc } });
    textures.push_back({ TextureWithDesc{m_rtAlbedoTexture, srvDesc } });

    CreateRaytracingPipeline(m_raytracingAO.get(), m_device.Get(), model.get(), textures, GetIndexBufferSRVDesc(model.get()), GetVertexBufferSRVDesc(model.get(), sizeof(ModelClass::VertexBufferStruct)), m_sceneBuffer, m_cameraBuffer, m_aoBuffer, {}, sizeof(XMFLOAT4) * 2);
}

void Renderer::PrepareRaytracingResourcesLambert(const std::shared_ptr<ModelClass> model)
{
    RtProgram rayGenShader{};
    RtProgram missShader;
    HitProgram hitShader;

    RtProgram missShader_new{};
    HitProgram hitShader_new{};
    //RtProgram rayGenShader_new;

    CreateRayGenShader(rayGenShader, m_shaderCompiler, L"Shaders/RT_Lambertian.hlsl", 2, 1, 5, {}, L"RayGen");
    CreateMissShader(missShader, m_shaderCompiler, L"Shaders/RT_Lambertian.hlsl", L"Miss");
    CreateClosestHitShader(hitShader, m_shaderCompiler, L"Shaders/RT_Lambertian.hlsl", L"ClosestHit");

    //CreateRayGenShader(rayGenShader_new, m_shaderCompiler, L"Shaders/RT_Lambertian.hlsl", 2, 1, 5, {}, L"RayGen_Lambert_new");
    CreateMissShader(missShader_new, m_shaderCompiler, L"Shaders/RT_Lambertian_Indirect.hlsl", L"MissIndirect");
    CreateClosestHitShader(hitShader_new, m_shaderCompiler, L"Shaders/RT_Lambertian_Indirect.hlsl", L"ClosestHitIndirect");

    HitGroup group = { rayGenShader, missShader, hitShader, L"GroupLambert" };
    HitGroup group_new = { rayGenShader, missShader_new, hitShader_new, L"GroupLambert_new" };

    m_raytracingLambert = std::shared_ptr<RaytracingResources>(new RaytracingResources(m_device.Get(), m_commandList, model, { group, group_new }));

    std::vector<TextureWithDesc> textures{};
    CreateTexture2D(m_rtLambertTexture, m_windowWidth, m_windowHeight);
    textures.push_back({ TextureWithDesc{m_rtLambertTexture, GetAccessViewDesc(DXGI_FORMAT_UNKNOWN, D3D12_UAV_DIMENSION_TEXTURE2D)} });

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING };
    D3D12_SHADER_RESOURCE_VIEW_DESC normalDesc = { DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING };
    srvDesc.Texture2D.MipLevels = 1;
    normalDesc.Texture2D.MipLevels = 1;
    textures.push_back({ TextureWithDesc{m_rtNormalTexture, normalDesc } });
    textures.push_back({ TextureWithDesc{m_rtAlbedoTexture, srvDesc } });

    CreateRaytracingPipeline(m_raytracingLambert.get(), m_device.Get(), model.get(), textures, GetIndexBufferSRVDesc(nullptr), GetVertexBufferSRVDesc(nullptr, sizeof(ModelClass::VertexBufferStruct)), m_sceneBuffer, m_cameraBuffer);
}

void Renderer::CreateRayGenShader(RtProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, int cbvDescriptors, int uavDescriptors, int srvDescriptors, std::vector<CD3DX12_STATIC_SAMPLER_DESC> samplers, LPCWSTR name, LPCWSTR nameToExport)
{
    // Load and compile the ray generation shader
    shader = RtProgram(D3D12ShaderInfo(path, L"", L"lib_6_3"));
    shader.name = name;
    shader.exportToRename = nameToExport;
    Compile_Shader(shaderCompiler, shader);

    // Describe the ray generation root signature
    D3D12_DESCRIPTOR_RANGE ranges[3];

    ranges[0].BaseShaderRegister = 0;
    ranges[0].NumDescriptors = cbvDescriptors;
    ranges[0].RegisterSpace = 0;
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    ranges[1].BaseShaderRegister = 0;
    ranges[1].NumDescriptors = uavDescriptors;
    ranges[1].RegisterSpace = 0;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].OffsetInDescriptorsFromTableStart = cbvDescriptors;

    ranges[2].BaseShaderRegister = 0;
    ranges[2].NumDescriptors = srvDescriptors;
    ranges[2].RegisterSpace = 0;
    ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[2].OffsetInDescriptorsFromTableStart = cbvDescriptors + uavDescriptors;

    CD3DX12_ROOT_PARAMETER param0 = {};
    param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param0.DescriptorTable.NumDescriptorRanges = _countof(ranges);
    param0.DescriptorTable.pDescriptorRanges = ranges;

    CD3DX12_ROOT_PARAMETER rootParams[1]{ param0 };

    // Create the root signature    
    CreateRootSignatureRTCP(_countof(rootParams), static_cast<UINT>(samplers.size()), rootParams, samplers.size() > 0 ? &samplers[0] : NULL, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE, shader.pRootSignature);
}

void Renderer::CreateMissShader(RtProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, LPCWSTR name, LPCWSTR nameToExport) const
{
    // Load and compile the miss shader
    shader = RtProgram(D3D12ShaderInfo(path, L"", L"lib_6_3"));
    shader.name = name;
    shader.exportToRename = nameToExport;
    Compile_Shader(shaderCompiler, shader);
}

void Renderer::CreateClosestHitShader(HitProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, LPCWSTR name, LPCWSTR nameToExport) const
{
    // Load and compile the Closest Hit shader
    shader.chs = RtProgram(D3D12ShaderInfo(path, L"", L"lib_6_3"));
    shader.chs.name = name;
    shader.chs.exportToRename = nameToExport;
    Compile_Shader(shaderCompiler, shader.chs);
}

void Renderer::CreateAnyHitShader(HitProgram& shader, D3D12ShaderCompilerInfo& shaderCompiler, const wchar_t* path, LPCWSTR name, LPCWSTR nameToExport) const
{    
    // Load and compile the Any Hit shader
    shader.ahs = RtProgram(D3D12ShaderInfo(path, L"", L"lib_6_3"));
    shader.ahs.name = name;
    shader.ahs.exportToRename = nameToExport;
    Compile_Shader(shaderCompiler, shader.ahs);
}

void Renderer::InitShaderCompiler(D3D12ShaderCompilerInfo& shaderCompiler) const 
{
    ThrowIfFailed(shaderCompiler.DxcDllHelper.Initialize());
    ThrowIfFailed(shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &shaderCompiler.compiler));
    ThrowIfFailed(shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &shaderCompiler.library));
}

void Renderer::Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, D3D12ShaderInfo& info, IDxcBlob** blob) const
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

void Renderer::Compile_Shader(D3D12ShaderCompilerInfo& compilerInfo, RtProgram& program) const
{
    Compile_Shader(compilerInfo, program.info, &program.blob);
    program.SetBytecode();
}

void Renderer::Compile_Shader(LPCWSTR pFileName, const D3D_SHADER_MACRO* pDefines, ID3DInclude* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2, ID3DBlob** ppCode) const
{
    ComPtr<ID3DBlob> errorBlob;
    HRESULT result = D3DCompileFromFile(pFileName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, &errorBlob);
    if (FAILED(result))
    {
        if (errorBlob) {
            MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "HLSL error", MB_OK);
            errorBlob->Release();
        }
        ThrowIfFailed(result);
    }
}

void Renderer::InitializeRaytracingBufferValues()
{
    m_sceneBuffer.value.lightPosition = XMFLOAT4(-3.5f, 2.5f, -2.6f, 0.0f);
    m_sceneBuffer.value.lightAmbientColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
    m_sceneBuffer.value.lightDiffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
}
