#include "Particles.h"
#include "MathHelper.h"

Particles::Particles(DirectX::XMFLOAT3 startPosition, int particleCount) {
	m_particleCount = particleCount;
	m_startPosition = startPosition;
	m_vertices = { startPosition };
}

Particles::~Particles() {

}

void Particles::Update(float deltaTime) {
	for (int i = 0; i < ParticleCount(); ++i) {
		m_vertices[i].Pos.y += MathHelper::RandF(0.01f, 0.10f);
		
		m_vertices[i].Pos.x = 0.0f;
	}
}

const int Particles::ParticleCount() const
{
	return m_vertices.size();
}

