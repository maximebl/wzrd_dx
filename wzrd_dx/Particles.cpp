#include "Particles.h"

Particles::Particles(DirectX::XMFLOAT3 startPosition, int particleCount) {
	m_particleCount = particleCount;
	m_startPosition = startPosition;
	m_vertices = { startPosition };
}

Particles::~Particles() {

}

void Particles::Update(float deltaTime) {
	for (int i = 0; i < ParticleCount(); ++i) {
		m_vertices[i].Pos.y += 0.05f;
	}
}

const int Particles::ParticleCount() const
{
	return m_vertices.size();
}

