#pragma once
#ifndef _TEST_STACK_OVERFLOW_H_
#define _TEST_STACK_OVERFLOW_H_

typedef struct _foo
{
public:
	XMFLOAT4 a;
	XMFLOAT3 b;
	XMFLOAT2 c;
} Foo;

template <class T>
struct CB
{
public:
	T value;
	UINT8* ptr = nullptr;
	ComPtr<ID3D12Resource> resource = NULL;
};

template <typename T>
void CreateRaytracingPipeline(CB<T> cb);

class TestStackOverflow
{
public:
	TestStackOverflow()
	{
		CB<Foo> cb{};
		CreateRaytracingPipeline(cb);
	}
};

#endif // !_TEST_STACK_OVERFLOW_H_

template<typename T>
inline void CreateRaytracingPipeline(CB<T> cb)
{
}
