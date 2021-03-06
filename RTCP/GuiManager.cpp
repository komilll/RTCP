#include "GuiManager.h"

void GuiManager::Render(ID3D12GraphicsCommandList* commandList)
{
    // Do not render UI
    if (!m_isActive)
    {
        return;
    }
    ID3D12DescriptorHeap* ppHeaps[] = { m_imGuiHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Prepare ImGui settings
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Raytracing settings");

        // Progress section
        ImGui::Text(std::to_string(m_renderer->RENDER_ONLY_RTAO ? m_renderer->m_aoBuffer.value.accFrames : m_renderer->m_giBuffer.value.accFrames).c_str());
        ImGui::Checkbox("Pause RT", &m_renderer->DO_PAUSE);
        ImGui::Separator();

        // Ao settings section
        AoSettings();
        ImGui::Separator();

        // GI settings section
        if (ImGui::Checkbox("GI Use skybox", &m_renderer->USE_SKYBOX)) {
            m_renderer->m_resetFrameGI = true;
        }
        if (ImGui::Checkbox("GI indirect diffuse", &m_renderer->USE_GI_INDIRECT)) {
            m_renderer->m_resetFrameGI = true;
        }
        if (ImGui::SliderInt("GI Max frame count sqrt", &m_renderer->m_giBuffer.value.sqrtMaxFrames, 1, 100)) {
            m_renderer->m_resetFrameGI = true;
            m_renderer->m_giBuffer.value.maxFrames = m_renderer->m_giBuffer.value.sqrtMaxFrames * m_renderer->m_giBuffer.value.sqrtMaxFrames;
        }
        ImGui::Separator();

        // Sampling type section
        SamplingTypeSettings();
        ImGui::Separator();

        // Shading model secrtion
        ShadingModelSettings();
        ImGui::Separator();

        // Indirect lighting section
        if (ImGui::SliderInt("Indirect bounce count", &m_renderer->m_giBuffer.value.bounceCount, 1, 8)) {
            m_renderer->m_resetFrameGI = true;
        }
        ImGui::Separator();

        // Postprocess section
        ImGui::Text("Postprocess settings:");
        ImGui::SliderFloat("Exposure", &m_renderer->m_postprocessBuffer.value.exposure, -50.0f, 20.0f, "%.1f");

        // Profilling section
        ImGui::Text(m_renderer->m_profiler->GetOutputString());
        ImGui::Separator();

        // Lights section
        RenderLightSettings();
        ImGui::Separator();

        // Camera position/rotation section
        char camPosText[100];
        char camRotText[100];
        sprintf_s(camPosText, "Current cam pos: (%.1f, %.1f, %.1f)", m_renderer->m_cameraPosition.x, m_renderer->m_cameraPosition.y, m_renderer->m_cameraPosition.z);
        sprintf_s(camRotText, "Current cam rot: (%.1f, %.1f, %.1f)", m_renderer->m_cameraRotation.x, m_renderer->m_cameraRotation.y, m_renderer->m_cameraRotation.z);
        ImGui::Text(camPosText);
        ImGui::Text(camRotText);
        ImGui::SliderFloat("Camera speed", &m_renderer->m_cameraSpeed, 0.01f, 25.0f, "%.2f");

        ImGui::End();
    }

    // Finish imgui - render UI
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void GuiManager::ToogleRendering()
{
    m_isActive = !m_isActive;
}

void GuiManager::RenderLightSettings()
{
    ImGui::Text("Light settings:");
    if (ImGui::SliderFloat3("Sun direction", &m_renderer->m_giBuffer.value.sunDirection.x, 0.0f, 1.0f)) {
        m_renderer->m_resetFrameAO = true;
        m_renderer->m_resetFrameGI = true;
        m_renderer->m_resetFrameProfiler = true;
    }

    if (ImGui::Button("Create light in camera pos"))
    {
        Light light{};
        light.type = LightType::Point;
        light.position = XMFLOAT3{ m_renderer->m_cameraPosition.x, m_renderer->m_cameraPosition.y, m_renderer->m_cameraPosition.z };
        light.rotation = XMFLOAT3{ m_renderer->m_cameraRotation.x, m_renderer->m_cameraRotation.y, m_renderer->m_cameraRotation.z };
        light.color = XMFLOAT4{ 1,1,1,1 };
        m_renderer->m_lightSettings->AddLight(light);
        m_renderer->m_updateLightCount = true;
    }

    auto& lightsInfo = m_renderer->m_lightSettings->GetLightsInfo();
    for (int i = 0; i < lightsInfo.size(); ++i)
    {
        std::string name = "Light #" + std::to_string(i);
        if (ImGui::CollapsingHeader(name.c_str()))
        {
            char pos[100];
            char rot[100];
            char color[100];

            sprintf_s(pos, "Pos: (%.2f, %.2f, %.2f)", lightsInfo[i].position.x, lightsInfo[i].position.y, lightsInfo[i].position.z);
            sprintf_s(rot, "Rot: (%.2f, %.2f, %.2f)", lightsInfo[i].rotation.x, lightsInfo[i].rotation.y, lightsInfo[i].rotation.z);
            sprintf_s(color, "Color: (%.2f, %.2f, %.2f, %.2f)", lightsInfo[i].color.x, lightsInfo[i].color.y, lightsInfo[i].color.z, lightsInfo[i].color.w);

            ImGui::Text(pos);
            ImGui::Text(rot);
            ImGui::Text(color);

            std::string deleteName = "Delete " + name;
            if (ImGui::Button(deleteName.c_str()))
            {
                m_renderer->m_lightSettings->RemoveLight(i);
                m_renderer->m_updateLightCount = true;
                break;
            }

            std::string colorPickerHeaderName = "Color picker " + name;
            if (ImGui::CollapsingHeader(colorPickerHeaderName.c_str()))
            {
                std::string colorPickerName = "Color picker - picker " + name;
                float colors[4] = { lightsInfo[i].color.x, lightsInfo[i].color.y, lightsInfo[i].color.z, lightsInfo[i].color.w };
                if (ImGui::ColorPicker4(colorPickerName.c_str(), colors))
                {
                    lightsInfo[i].color = XMFLOAT4{ colors[0], colors[1], colors[2], colors[3] };
                    m_renderer->m_updateLightCount = true;
                }
            }
        }
    }
}

void GuiManager::SamplingTypeSettings()
{
    if (ImGui::CollapsingHeader("Sampling type", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Selectable("Uniform sampling", &m_renderer->SAMPLE_UNIFORM))
        {
            m_renderer->m_resetFrameAO = true;
            m_renderer->m_resetFrameGI = true;
            m_renderer->m_resetFrameProfiler = true;
            m_renderer->SAMPLE_RANDOM = false;
            m_renderer->SAMPLE_MJ = false;
            m_renderer->SAMPLE_UNIFORM = true;
        }
        if (ImGui::Selectable("Multi-jittered sampling", &m_renderer->SAMPLE_MJ))
        {
            m_renderer->m_resetFrameAO = true;
            m_renderer->m_resetFrameGI = true;
            m_renderer->m_resetFrameProfiler = true;
            m_renderer->SAMPLE_RANDOM = false;
            m_renderer->SAMPLE_MJ = true;
            m_renderer->SAMPLE_UNIFORM = false;
        }
        if (ImGui::Selectable("Random cosine sampling", &m_renderer->SAMPLE_RANDOM))
        {
            m_renderer->m_resetFrameAO = true;
            m_renderer->m_resetFrameGI = true;
            m_renderer->m_resetFrameProfiler = true;
            m_renderer->SAMPLE_RANDOM = true;
            m_renderer->SAMPLE_MJ = false;
            m_renderer->SAMPLE_UNIFORM = false;
        }
    }
}

void GuiManager::ShadingModelSettings()
{
    if (ImGui::CollapsingHeader("Raytracing shading model", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Selectable("Lambertian", &m_renderer->RENDER_LAMBERT))
        {
            m_renderer->m_resetFrameAO = true;
            m_renderer->m_resetFrameGI = true;
            m_renderer->m_resetFrameProfiler = true;
            m_renderer->RENDER_GGX = false;
            m_renderer->RENDER_LAMBERT = true;
        }
        if (ImGui::Selectable("GGX", &m_renderer->RENDER_GGX))
        {
            m_renderer->m_resetFrameAO = true;
            m_renderer->m_resetFrameGI = true;
            m_renderer->m_resetFrameProfiler = true;
            m_renderer->RENDER_GGX = true;
            m_renderer->RENDER_LAMBERT = false;
        }
    }
}

void GuiManager::AoSettings()
{
    if (ImGui::Checkbox("Render only AO", &m_renderer->RENDER_ONLY_RTAO))
    {
        m_renderer->m_resetFrameAO = true;
        m_renderer->m_resetFrameGI = true;
        m_renderer->m_resetFrameProfiler = true;
    }
    if (m_renderer->RENDER_ONLY_RTAO && ImGui::CollapsingHeader("AO options:"))
    {
        if (ImGui::SliderInt("AO Sample count", &m_renderer->m_aoBuffer.value.sampleCount, 1, 100)) {
            m_renderer->m_resetFrameAO = true;
        }
        if (ImGui::SliderInt("AO Max frame count", &m_renderer->m_aoBuffer.value.maxFrames, 0, 1000)) {
            m_renderer->m_resetFrameAO = true;
        }
        if (ImGui::SliderFloat("AO Radius", &m_renderer->m_aoBuffer.value.aoRadius, 0.0f, 50.0f, "%.2f")) {
            m_renderer->m_resetFrameAO = true;
        }
        if (ImGui::SliderFloat("AO Min T", &m_renderer->m_aoBuffer.value.minT, 1e-10f, 1e-1f, "%.4f")) {
            m_renderer->m_resetFrameAO = true;
        }
        if (ImGui::Checkbox("AO Frame jitter", &m_renderer->USE_AO_FRAME_JITTER)) {
            m_renderer->m_resetFrameAO = true;
        }

        if (ImGui::Checkbox("AO Thin lens", &m_renderer->USE_AO_THIN_LENS)) {
            m_renderer->m_resetFrameAO = true;
        }
        if (ImGui::SliderFloat("AO fNumber", &m_renderer->m_cameraBuffer.value.fNumber, 1e-4f, 64.0f, "%.2f")) {
            m_renderer->m_resetFrameAO = true;
        }
        if (ImGui::SliderFloat("AO Focal length", &m_renderer->m_cameraBuffer.value.focalLength, 1e-4f, 10.0f, "%.2f")) {
            m_renderer->m_resetFrameAO = true;
        }
    }
}

GuiManager::GuiManager(ID3D12Device* device, Renderer* renderer)
{
    m_renderer = renderer;
    assert(renderer != NULL && "Renderer is NULL");
    assert(device != NULL && "Device is NULL");

    // Initialize ImGui SRV heap
    D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV , 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imGuiHeap)));
    ImGui_ImplDX12_Init(device, 2, DXGI_FORMAT_R16G16B16A16_FLOAT, m_imGuiHeap.Get(), m_imGuiHeap->GetCPUDescriptorHandleForHeapStart(), m_imGuiHeap->GetGPUDescriptorHandleForHeapStart());
}
