#pragma once
#ifndef _CBUFFER_H_
#define _CBUFFER_H_

#include "pch.h"

template <class T>
struct CBuffer
{
public:
	void Update() {
		if (ptr != nullptr) {
			memcpy(ptr, &value, sizeof(value));
		}
	}

	T value;
	UINT8* ptr = nullptr;
	ComPtr<ID3D12Resource> resource = NULL;
};

#endif // !_CBUFFER_H_
