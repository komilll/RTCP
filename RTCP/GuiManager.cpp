#include "GuiManager.h"

void GuiManager::Render(ID3D12GraphicsCommandList* commandList)
{
    ID3D12DescriptorHeap* ppHeaps[] = { m_imGuiHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Hello, world!");
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
        if (ImGui::SliderFloat("AO fNumber", &m_renderer->m_aoBuffer.value.fNumber, 1e-4f, 64.0f, "%.2f")) {
            m_renderer->m_resetFrameAO = true;
        }
        if (ImGui::SliderFloat("AO Focal length", &m_renderer->m_aoBuffer.value.focalLength, 1e-4f, 10.0f, "%.2f")) {
            m_renderer->m_resetFrameAO = true;
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

GuiManager::GuiManager(ID3D12Device* device, Renderer* renderer)
{
    m_renderer = renderer;
    assert(renderer != NULL && "Renderer is NULL");
    assert(device != NULL && "Device is NULL");

    // Initialize ImGui SRV heap
    D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV , 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imGuiHeap)));
    ImGui_ImplDX12_Init(device, 2, DXGI_FORMAT_R8G8B8A8_UNORM, m_imGuiHeap.Get(), m_imGuiHeap->GetCPUDescriptorHandleForHeapStart(), m_imGuiHeap->GetGPUDescriptorHandleForHeapStart());
}
