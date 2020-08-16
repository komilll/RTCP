#pragma once
#ifndef _GUI_MANAGER_H_
#define _GUI_MANAGER_H_

#include "pch.h"
#include "Renderer.h"

#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#define IMGUI_IMPL_WIN32_DISABLE_LINKING_XINPUT
#define DIRECTINPUT_VERSION 0x0800

#include "External/imgui/imgui.h"
#include "External/imgui/imgui_impl_win32.h"
#include "External/imgui/imgui_impl_dx12.h"

class GuiManager
{
public:
	GuiManager(ID3D12Device* device, Renderer* renderer);

	void Render(ID3D12GraphicsCommandList* commandList);

private:
	ComPtr<ID3D12DescriptorHeap> m_imGuiHeap;
	Renderer* m_renderer;
};

#endif // !_GUI_MANAGER_H_
