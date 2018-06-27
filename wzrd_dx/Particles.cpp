#include "Particles.h"

void Particles::Particles(float startPosition, float size) {
	
}

void Particles::Update(float deltaTime) {
	for (int i = 0; i < ParticleCount(); ++i) {
		m_currentPositions[i].y += 0.05f;
	}
}

const int Particles::ParticleCount() const
{
	return m_currentPositions.size();
}

