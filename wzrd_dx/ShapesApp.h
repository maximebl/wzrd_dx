#pragma once
#include "App.h"
#include "MeshGeometry.h"
#include "GameTimer.h"
#include "Waves.h"
#include "FrameResource.h"
#include "Material.h"
#include "Texture.h"
#include "DDSTextureLoader.h"
#include "Camera.h"

using Microsoft::WRL::ComPtr;


enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	AlphaTestedTestSprites,
	Count
};

struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	int numFramesDirty = gNumFrameResources;
	UINT objCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

class ShapesApp : public AbstractRenderer {
public:
	bool init();
	void update(GameTimer& m_gameTimer);
	void render();
	virtual void resize(float aspectRatio);
	void LoadTextures();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildLandGeometry();
	void BuildBoxGeometry();
	void BuildWavesGeometryBuffers();
	void BuildDescriptorHeaps();
	void BuildMaterials();
	void BuildTreeSpritesGeometry();
	void BuildTestSpriteGeometry();

	float GetHillsHeight(float x, float y) const;
	DirectX::XMFLOAT3 GetHillsNormal(float x, float z) const;

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems);
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildPSOs();

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	void OnKeyboardInput(const GameTimer& gt);

	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	GameTimer m_timer;
	
	ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap = nullptr;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> m_shaders;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;
	std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;
	std::unordered_map<std::string, std::unique_ptr<Material>> m_materials;

	std::vector<std::unique_ptr<FrameResource>> m_frameResources;
	
	RenderItem* m_wavesRenderItem = nullptr;
	std::vector<RenderItem*> m_renderItemLayer[(int)RenderLayer::Count];
	std::unique_ptr<Waves> m_waves;
	std::vector<std::unique_ptr<RenderItem>> m_allRenderItems;

	Camera m_camera;
private:
	int m_currentFrameResourceIndex = 0;
	FrameResource* m_currentFrameResource = nullptr;
	PassConstants m_mainPassCB;
	UINT m_cbvSrvDescriptorSize = 0;

	float m_sunTheta = 1.25f * XM_PI;
	float m_sunPhi = XM_PIDIV4;
};