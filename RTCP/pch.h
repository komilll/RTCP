#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>

#ifndef _DEBUG
#define _DEBUG
#endif // !_DEBUG

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#if defined(CreateWindow)
#undef CreateWindow
#endif

#include <wrl.h>
using namespace Microsoft::WRL;

#pragma comment(lib,"d3d12.lib")
#include <d3d12.h>

#pragma comment(lib,"dxgi.lib")
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment(lib,"dxguid.lib")
#include "d3dx12.h"

#include <algorithm>
#include <cassert>
#include <chrono>

#include "Helpers.h"