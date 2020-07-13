#pragma once

#include "pch.h"
#include "DeviceManager.h"
#include "Render.h"

class Main
{
public:
	HRESULT InitializeWindow(HINSTANCE hInstance, std::shared_ptr<DeviceManager> deviceManager);
	HRESULT Run(std::shared_ptr<DeviceManager> deviceManager, std::shared_ptr<Renderer> renderer);
	HWND GetWindowHandle() const { return m_hwnd; };

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName);
	HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInst, const wchar_t* windowTitle, int width, int height);
	void EnableDebugLayer();

	void Update();
	void ToggleFullscreen();

private:
	HWND m_hwnd = NULL;
	RECT m_windowRect;

	bool m_fullscreen = false;
};

static std::shared_ptr<DeviceManager> m_deviceManager;
static std::shared_ptr<Renderer> m_renderer;
static bool m_isInitialized;
static Main* self;
