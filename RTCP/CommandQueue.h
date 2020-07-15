//#pragma once
//
//#include "pch.h"
//#include <queue>
//
//class CommandQueue
//{
//public:
//	CommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
//	//virtual ~CommandQueue();
//
//	ComPtr<ID3D12GraphicsCommandList2> GetCommandList();
//	int ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList);
//
//	int Signal();
//	bool IsFenceComplete(int fenceValue);
//	void WaitForFenceValue(int fenceValue, std::chrono::milliseconds duration = std::chrono::milliseconds::max());
//	void Flush();
//
//	void ResetCommandAllocator();
//	void ResetCommandList();
//	ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const { return m_commandQueue; };
//
//protected:
//	ComPtr<ID3D12CommandAllocator> CreateCommandAllocator() const;
//	ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator) const;
//
//private:
//	struct CommandAllocatorEntry
//	{
//		int fenceValue;
//		ComPtr<ID3D12CommandAllocator> commandAllocator;
//	};
//
//	using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
//	using CommandListQueue = std::queue<ComPtr<ID3D12GraphicsCommandList2>>;
//
//	D3D12_COMMAND_LIST_TYPE m_commandListType;
//	ComPtr<ID3D12Device2> m_device				= NULL;
//	ComPtr<ID3D12CommandQueue> m_commandQueue   = NULL;
//	ComPtr<ID3D12Fence> m_fence					= NULL;
//	HANDLE m_fenceEvent;
//	int m_fenceValue;
//
//	CommandAllocatorQueue m_commandAllocatorQueue;
//	CommandListQueue m_commandListQueue;
//};