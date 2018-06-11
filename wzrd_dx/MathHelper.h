#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <cstdlib>

class MathHelper
{
public:
	static DirectX::XMFLOAT4X4 Identity4x4() {
		static DirectX::XMFLOAT4X4 I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);
		return I;
	}
	//static const float Pi;

	template<typename T>
	static T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	template<typename T>
	static T Clamp(const T& x, const T& low, const T& high) {
		return x < low ? low : (x > high ? high : x);
	}

	static int Rand(int a, int b) {
		return a + rand() % ((b - a) + 1);
	}

	static float RandF() {
		return (float)(rand()) / (float)RAND_MAX;
	}

	static float RandF(float a, float b) {
		return a + RandF() * (b - a);
	}

	static DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi) {
		return DirectX::XMVectorSet(
			radius*sinf(phi)*cosf(theta),
			radius*cosf(phi),
			radius*sinf(phi)*sinf(theta),
			1.0f
		);
	}
};

static const float Pi = 3.1415926535f;
