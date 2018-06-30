#pragma once
#include <vector>
#include <DirectXMath.h>
#include <array>
#include "MathHelper.h"

// required in order to use XMVector overloaded operators
using namespace DirectX;

struct TestSpriteVertex {
	DirectX::XMFLOAT3 Pos = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT2 Size = {1.0f, 1.0f};
};

class Particles {
public:
	Particles(DirectX::XMFLOAT3 startPosition, int particleCount, std::vector<TestSpriteVertex> vertices );
	~Particles();

	void Update(float deltaTime);
	const DirectX::XMFLOAT3& Position(int i)const { return m_vertices[i].Pos; }
	const int ParticleCount()const;

private:
	DirectX::XMFLOAT3 m_location = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_velocity = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_acceleration = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_startPosition = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT2 m_startSize = { 1.0f, 1.0f };
	int m_particleCount = 0;

	std::vector<TestSpriteVertex> m_vertices;
	std::vector<std::uint16_t> m_indices;
};