#include "DeviceManager.h"

HRESULT DeviceManager::InitializeDeviceResponsibilities(HWND hwnd)
{
	HRESULT result = S_OK;
    return result;

    ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(m_useWarp);
    m_device = CreateDevice(dxgiAdapter4);
    m_commandQueue = CreateCommandQueue(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_swapChain = CreateSwapChain(hwnd, m_commandQueue, m_windowWidth, m_windowHeight, m_frameCount);

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_rtvDescriptorHeap = CreateDescriptorHeap(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_frameCount);
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    UpdateRenderTargetViews(m_device, m_swapChain, m_rtvDescriptorHeap);

    for (int i = 0; i < m_frameCount; ++i)
    {
        m_commandAllocators[i] = CreateCommandAllocator(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }
    m_commandList = CreateCommandList(m_device, m_commandAllocators[m_currentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

    m_fence = CreateFence(m_device);
    m_fenceEvent = CreateEventHandle();
    
    m_tearingSupported = CheckTearingSupport();

	return result;
}

HRESULT DeviceManager::CreateWindowResources(HWND hwnd)
{
	return E_NOTIMPL;
}

ComPtr<IDXGISwapChain4> DeviceManager::CreateSwapChain(HWND hwnd, ComPtr<ID3D12CommandQueue> commandQueue, int width, int height, int bufferCount) const
{
    ComPtr<IDXGISwapChain4> swapChain4;
    IDXGIFactory4* dxgiFactory;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = bufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1));
    ThrowIfFailed(swapChain1.As(&swapChain4));

    return swapChain4.Get();
}

ComPtr<ID3D12Device2> DeviceManager::CreateDevice(ComPtr<IDXGIAdapter4> adapter) const
{
    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        D3D12_MESSAGE_SEVERITY severities[]{ D3D12_MESSAGE_SEVERITY_INFO };

        D3D12_MESSAGE_ID denyIDs[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
        };

        D3D12_INFO_QUEUE_FILTER newFilter{};
        newFilter.DenyList.NumSeverities = _countof(severities);
        newFilter.DenyList.pSeverityList = severities;
        newFilter.DenyList.NumIDs = _countof(denyIDs);
        newFilter.DenyList.pIDList = denyIDs;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&newFilter));
    }
#endif

    return d3d12Device2.Get();
}

ComPtr<ID3D12CommandQueue> DeviceManager::CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) const
{
    ComPtr<ID3D12CommandQueue> commandQueue;

    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));

    return commandQueue;
}

ComPtr<ID3D12DescriptorHeap> DeviceManager::CreateDescriptorHeap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, int numDescriptors) const
{
    ComPtr<ID3D12DescriptorHeap> heap;

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));

    return heap;
}

ComPtr<ID3D12CommandAllocator> DeviceManager::CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) const
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> DeviceManager::CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type) const
{
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
    ThrowIfFailed(commandList->Close());

    return commandList;
}

ComPtr<ID3D12Fence> DeviceManager::CreateFence(ComPtr<ID3D12Device2> device) const
{
    ComPtr<ID3D12Fence> fence = {};
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    return fence;
}

HANDLE DeviceManager::CreateEventHandle() const
{
    HANDLE fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent && "Failed to create fence event");

    return fenceEvent;
}

bool DeviceManager::CheckTearingSupport() const
{
    BOOL allowTearing = FALSE;

    ComPtr<IDXGIFactory4> factory4;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
    {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory4.As(&factory5)))
        {
            if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
            {
                allowTearing = FALSE;
            }
        }
    }
    return allowTearing == TRUE;
}

ComPtr<IDXGIAdapter4> DeviceManager::GetAdapter(bool useWarp) const
{
    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if (useWarp)
    {
        ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
    }
    else
    {
        SIZE_T maxDedicatedVideoMemory = 0;
        for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
            dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

            if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
            {
                if (SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
                {
                    //Find GPU with most video ram
                    if (dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
                    {
                        maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
                    }
                }
            }
        }
    }

    return dxgiAdapter4;
}

void DeviceManager::Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, int& fenceValue, HANDLE fenceEvent)
{
    int fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
    WaitForFenceValue(fence, fenceValue, fenceEvent);
}

void DeviceManager::WaitForFenceValue(ComPtr<ID3D12Fence> fence, int fenceValue, HANDLE fenceEvent, std::chrono::milliseconds duration)
{
    if (fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
        ::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
    }
}

int DeviceManager::Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, int& fenceValue)
{
    const int fenceValueForSignal = ++fenceValue;
    ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));

    return fenceValueForSignal;
}

void DeviceManager::Resize(int width, int height)
{
    if (m_windowWidth != width || m_windowHeight != height)
    {
        m_windowWidth = std::max(1, width);
        m_windowHeight = std::max(1, height);

        Flush(m_commandQueue, m_fence, m_fenceValue, m_fenceEvent);

        for (int i = 0; i < m_frameCount; ++i)
        {
            m_backBuffers[i]->Release();
            m_frameFenceValues[i] = m_frameFenceValues[m_currentBackBufferIndex];
        }

        DXGI_SWAP_CHAIN_DESC swapChainDesc{};
        ThrowIfFailed(m_swapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(m_swapChain->ResizeBuffers(m_frameCount, m_windowWidth, m_windowHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

        m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews(m_device, m_swapChain, m_rtvDescriptorHeap);
    }
}

void DeviceManager::UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
    auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{ descriptorHeap->GetCPUDescriptorHandleForHeapStart() };

    for (int i = 0; i < DeviceManager::GetFrameCount(); ++i)
    {
        ID3D12Resource* backBuffer;
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        device->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);
        m_backBuffers[i] = backBuffer;
        rtvHandle.Offset(rtvDescriptorSize);
    }
}

int DeviceManager::ExecuteCommandList()
{
    assert(m_commandList && "Command list is null");
    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* const commandLists[]{ m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    return (m_frameFenceValues[m_currentBackBufferIndex] = Signal(m_commandQueue, m_fence, m_fenceValue));
}

void DeviceManager::WaitForFenceValue(int fenceValue)
{
    WaitForFenceValue(m_fence, fenceValue, m_fenceEvent);
}

void DeviceManager::ClearRenderTarget()
{
    auto backBuffer = GetCurrentBackBufferResource();

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv{ m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(m_currentBackBufferIndex), m_rtvDescriptorSize };

    m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void DeviceManager::Present()
{
    auto backBuffer = GetCurrentBackBufferResource();

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* const commandLists[]{ m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    m_frameFenceValues[m_currentBackBufferIndex] = Signal(m_commandQueue, m_fence, m_fenceValue);

    UINT syncInterval = m_vSync ? 1 : 0;
    UINT presentFlags = m_tearingSupported && !m_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(m_swapChain->Present(syncInterval, presentFlags));

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    WaitForFenceValue(m_fence, m_frameFenceValues[m_currentBackBufferIndex], m_fenceEvent);
}

void DeviceManager::FlushBackBuffer()
{
    Flush(m_commandQueue, m_fence, m_fenceValue, m_fenceEvent);
}

void DeviceManager::CloseMainHandle()
{
    ::CloseHandle(m_fenceEvent);
}

bool DeviceManager::ToggleVSync()
{
    m_vSync = !m_vSync;
    return m_vSync;
}
