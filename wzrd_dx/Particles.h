#pragma once
#include <vector>
#include <DirectXMath.h>

class Particles {
public:
	void Update(float deltaTime);
	const DirectX::XMFLOAT3& Position(int i)const { return m_currentPositions[i]; }
	const int ParticleCount()const;

private:

	std::vector<DirectX::XMFLOAT3> m_currentPositions;
};