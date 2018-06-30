#include "Particles.h"

Particles::Particles(DirectX::XMFLOAT3 startPosition, int particleCount, std::vector<TestSpriteVertex> vertices) {
	m_particleCount = particleCount;
	m_startPosition = startPosition;
	m_vertices = vertices;
	m_acceleration = { 0.0f, 1.0f, 0.0f };
}

Particles::~Particles() {

}

void Particles::Update(float deltaTime) {
	for (int i = 0; i < ParticleCount(); ++i) {

		// Convert to XMVectors to take advantage of SIMD 
		DirectX::XMVECTOR location = DirectX::XMLoadFloat3(&m_vertices[i].Pos);
		DirectX::XMVECTOR velocity = DirectX::XMLoadFloat3(&m_velocity);
		DirectX::XMVECTOR acceleration = DirectX::XMLoadFloat3(&m_acceleration);

		// Do the math 
		velocity += acceleration;
		location += velocity * deltaTime;

		// Store back into XMFloats
		DirectX::XMStoreFloat3(&m_vertices[i].Pos, location);
	}
}

const int Particles::ParticleCount() const
{
	return m_vertices.size();
}

