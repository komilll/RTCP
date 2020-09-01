#pragma once
#ifndef _PROFILER_H_
#define _PROFILER_H_

#include "pch.h"

// Code based on MJP's DX12 Sample Framework under MIT license
//=================================================================================================
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//=================================================================================================

static std::string m_output;

struct ProfilerStruct
{
	const char* name = nullptr;

	bool queryStarted = false;
	bool queryFinished = false;
	bool active = false;

	bool cpuProfile = false;
	INT64 startTime = 0;
	INT64 endTime = 0;

	static const UINT64 filterSize = 64;
	double timeSamples[filterSize] = {};
	UINT64 currentSample = 0;
};

class Profiler
{

public:
	void Initialize(ID3D12Device* device, UINT64 backBufferCount);

	UINT64 StartProfile(ID3D12GraphicsCommandList* commandList, const char* name);
	void EndProfile(ID3D12GraphicsCommandList* commandList, UINT64 idx, UINT64 currentFrameIdx);

	void EndFrame(UINT64 currentFrameIdx, ID3D12CommandQueue* commandQueue, bool resetFrame);
	const char* GetOutputString();

private:
	void Shutdown();
	static void UpdateProfile(ProfilerStruct& profile, UINT64 profileIdx, bool drawText, UINT64 gpuFrequency, const UINT64* frameQueryData, bool resetFrame);

private:
	std::vector<ProfilerStruct> m_profiles;
	UINT64 m_numProfiles = 0;
	ID3D12QueryHeap* m_queryHeap = nullptr;
	ID3D12Resource* m_buffer;
	bool m_enableGPUProfiling = false;
	bool m_showUI = false;
	bool m_logToClipboard = false;

	static const UINT64 MAX_PROFILES = 64;

	void* Map();
	template<typename T> T* Map() { return reinterpret_cast<T*>(Map()); };
	void Unmap();
};

#endif // !_PROFILER_H_
