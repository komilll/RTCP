#include "Profiler.h"

void Profiler::Initialize(ID3D12Device* device, UINT64 backBufferCount)
{
	Shutdown();

	m_enableGPUProfiling = true;

	if (m_enableGPUProfiling)
	{
		D3D12_QUERY_HEAP_DESC heapDesc{};
		heapDesc.Count = MAX_PROFILES * 2;
		heapDesc.NodeMask = 0;
		heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_queryHeap));

        auto size = MAX_PROFILES * backBufferCount * 2 * sizeof(UINT64);

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = static_cast<UINT32>(size);
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Alignment = 0;

        D3D12_HEAP_PROPERTIES heapProps =
        {
            D3D12_HEAP_TYPE_READBACK,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0,
        };

        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_buffer));
	}
}

void Profiler::Shutdown()
{
    if (m_queryHeap != nullptr) m_queryHeap->Release();
    if (m_buffer != nullptr) m_buffer->Release();
}

UINT64 Profiler::StartProfile(ID3D12GraphicsCommandList* commandList, const char* name)
{
    assert(name != nullptr);
    if (m_enableGPUProfiling == false)
        return static_cast<UINT64>(-1);

    // Check if profile already exists
    UINT64 profileIdx = static_cast<UINT64>(-1);
    for (UINT64 i = 0; i < m_numProfiles; ++i)
    {
        if (m_profiles[i].name == name)
        {
            profileIdx = i;
            break;
        }
    }

    // If profile doesn't exist - add new profile
    if (profileIdx == static_cast<UINT64>(-1))
    {
        assert(m_numProfiles < MAX_PROFILES);
        profileIdx = m_numProfiles++;
        m_profiles.push_back({});
        m_profiles[profileIdx].name = name;
    }

    ProfilerStruct& profileData = m_profiles[profileIdx];
    assert(profileData.queryStarted == false);
    assert(profileData.queryFinished == false);
    profileData.active = true;

    // Insert the start timestamp
    const UINT32 startQueryIdx = static_cast<UINT32>(profileIdx * 2);
    commandList->EndQuery(m_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, startQueryIdx);

    profileData.queryStarted = true;

    return profileIdx;
}

void Profiler::EndProfile(ID3D12GraphicsCommandList* commandList, UINT64 idx, UINT64 currentFrameIdx)
{
    if (m_enableGPUProfiling == false)
        return;

    assert(idx < m_numProfiles);

    ProfilerStruct& profileData = m_profiles[idx];
    assert(profileData.queryStarted == true);
    assert(profileData.queryFinished == false);

    // Insert the end timestamp
    const UINT32 startQueryIdx = static_cast<UINT32>(idx * 2);
    const UINT32 endQueryIdx = startQueryIdx + 1;
    commandList->EndQuery(m_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, endQueryIdx);

    // Resolve the data
    const UINT64 dstOffset = ((currentFrameIdx * MAX_PROFILES * 2) + startQueryIdx) * sizeof(UINT64);
    commandList->ResolveQueryData(m_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, startQueryIdx, 2, m_buffer, dstOffset);

    profileData.queryStarted = false;
    profileData.queryFinished = true;
}

void Profiler::EndFrame(UINT64 currentFrameIdx, ID3D12CommandQueue* commandQueue, bool resetFrame)
{
    UINT64 gpuFrequency = 0;
    const UINT64* frameQueryData = nullptr;
    if (m_enableGPUProfiling)
    {
        commandQueue->GetTimestampFrequency(&gpuFrequency);

        const UINT64* queryData = Map<UINT64>();
        frameQueryData = queryData + (currentFrameIdx * MAX_PROFILES * 2);
    }

    bool drawText = false;
    if (m_showUI == false)
    {
        //ImGui::SetNextWindowSize(ImVec2(75.0f, 25.0f));
        //ImGui::SetNextWindowPos(ImVec2(25.0f, 50.0f));
        //ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        //    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        //if (ImGui::Begin("profiler_button", nullptr, ImVec2(75.0f, 25.0f), 0.0f, flags))
        //{
        //    if (ImGui::Button("Timing"))
        //        m_showUI = true;
        //}
    }
    else
    {
        //ImVec2 initialSize = ImVec2(displayWidth * 0.5f, float(displayHeight) * 0.25f);
        //ImGui::SetNextWindowSize(initialSize, ImGuiSetCond_FirstUseEver);
        //ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiSetCond_FirstUseEver);

        //drawText = ImGui::Begin("Timing", &showUI);

        //if (logToClipboard)
        //    ImGui::LogToClipboard();
    }

    if (drawText)
    {
        //ImGui::Text("GPU Timing");
        //ImGui::Separator();
    }

    m_output = "";

    // Iterate over all of the profiles
    for (UINT64 profileIdx = 0; profileIdx < m_numProfiles; ++profileIdx)
        UpdateProfile(m_profiles[profileIdx], profileIdx, drawText, gpuFrequency, frameQueryData, resetFrame);

    //ImGui::End();

    if (m_enableGPUProfiling)
        Unmap();

    m_enableGPUProfiling = true;
}

const char* Profiler::GetOutputString()
{
    return m_output.c_str();
}

void Profiler::UpdateProfile(ProfilerStruct& profile, UINT64 profileIdx, bool drawText, UINT64 gpuFrequency, const UINT64* frameQueryData, bool resetFrame)
{
    profile.queryFinished = false;

    double time = 0.0f;
    if (frameQueryData)
    {
        assert(frameQueryData != nullptr);

        // Get the query data
        UINT64 startTime = frameQueryData[profileIdx * 2 + 0];
        UINT64 endTime = frameQueryData[profileIdx * 2 + 1];

        if (endTime > startTime)
        {
            UINT64 delta = endTime - startTime;
            double frequency = double(gpuFrequency);
            time = (delta / frequency) * 1000.0;
        }
    }

    profile.timeSamples[profile.currentSample] = time;
    profile.currentSample = (profile.currentSample + 1) % ProfilerStruct::filterSize;

    double maxTime = 0.0;
    double avgTime = 0.0;
    UINT64 avgTimeSamples = 0;
    for (UINT i = 0; i < ProfilerStruct::filterSize; ++i)
    {
        if (profile.timeSamples[i] <= 0.0)
            continue;

        if (resetFrame)
        {
            avgTimeSamples = 1;
            avgTime = profile.timeSamples[i];
            maxTime = profile.timeSamples[i];
        }
        else
        {
            maxTime = std::max(profile.timeSamples[i], maxTime);
            avgTime += profile.timeSamples[i];
            ++avgTimeSamples;
        }
    }

    if (avgTimeSamples > 0)
        avgTime /= double(avgTimeSamples);

    //if (profile.active && drawText)
    //    ImGui::Text("%s: %.2fms (%.2fms max)", profile.Name, avgTime, maxTime);

    if (profile.active)
    {
        char arr[200];
        sprintf_s(arr, "%s - AVG: %.2f ; MAX: %.2f\n", profile.name, avgTime, maxTime);
        m_output = arr;
    }

    profile.active = false;
}

void* Profiler::Map()
{
    assert(m_buffer != nullptr);
    void* data = nullptr;
    m_buffer->Map(0, nullptr, &data);
    return data;
}

void Profiler::Unmap()
{
    assert(m_buffer != nullptr);
    m_buffer->Unmap(0, nullptr);
}
