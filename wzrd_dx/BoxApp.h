#pragma once
#include "App.h"
#include "GameTimer.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "MeshGeometry.h"

class BoxApp : public AbstractRenderer {
public:
	bool init();
	void update(GameTimer& m_gameTimer);
	void render();

private:
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildPSO();

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

	std::unique_ptr<MeshGeometry> m_boxGeometry;
};