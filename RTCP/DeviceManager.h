#pragma once

#include "pch.h"

class DeviceManager
{
public:
	HRESULT InitializeDeviceResponsibilities(HWND hwnd);
	HRESULT CreateWindowResources(HWND hwnd);

	static constexpr int GetFrameCount() { return m_frameCount; }
	std::pair<int, int> GetWindowSize() { return std::pair<int, int>{ m_windowWidth, m_windowHeight }; };

	ComPtr<ID3D12CommandAllocator> GetCurrentCommandAllocator() const { return m_commandAllocators[m_currentBackBufferIndex]; };
	ComPtr<ID3D12Resource> GetCurrentBackBufferResource() const { return m_backBuffers[m_currentBackBufferIndex]; };
	ComPtr<ID3D12GraphicsCommandList> GetCommandList() const { return m_commandList; };

	void ClearRenderTarget();
	void Present();
	void FlushBackBuffer();
	void CloseMainHandle();

	void Resize(int width, int height);
	bool ToggleVSync();

private:
	ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hwnd, ComPtr<ID3D12CommandQueue> commandQueue, int width, int height, int bufferCount) const;
	ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter) const;
	ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) const;
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, int numDescriptors) const;
	ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) const;
	ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type) const;
	ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device) const;
	HANDLE CreateEventHandle() const;

	bool CheckTearingSupport() const;
	ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp) const;

	//Synchronization functions
	void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, int& fenceValue, HANDLE fenceEvent);
	void WaitForFenceValue(ComPtr<ID3D12Fence> fence, int fenceValue, HANDLE fenceEvent, std::chrono::milliseconds duration = std::chrono::milliseconds::max());
	int Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, int& fenceValue);

	void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap);


private:
	static constexpr int m_frameCount = 2;
	static constexpr bool m_useWarp = false;

	int m_windowWidth = 1280;
	int m_windowHeight = 720;
	bool m_vSync = true;
	bool m_tearingSupported = true;

	ComPtr<IDXGISwapChain4> m_swapChain				= NULL;
	ComPtr<ID3D12Device2> m_device					= NULL;
	ComPtr<ID3D12CommandQueue> m_commandQueue		= NULL;
	ComPtr<ID3D12GraphicsCommandList> m_commandList	= NULL;
	ComPtr<ID3D12Resource> m_backBuffers[m_frameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[m_frameCount];

	ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
	UINT m_rtvDescriptorSize;
	UINT m_currentBackBufferIndex;

	//Synchronization
	ComPtr<ID3D12Fence> m_fence;
	int m_fenceValue = 0;
	int m_frameFenceValues[m_frameCount] = {};
	HANDLE m_fenceEvent;
};