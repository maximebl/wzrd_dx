#pragma once
#include "App.h"
#include "GameTimer.h"
#include "FrameResource.h"
#include "MeshGeometry.h"
#include "DDSTextureLoader.h"
#include "Texture.h"

using Microsoft::WRL::ComPtr;

enum class RenderLayer2 : int
{
	Opaque = 0,
	Mirrors,
	Reflected,
	Transparent,
	Shadow,
	Count
};

struct RenderItem2
{
	RenderItem2() = default;

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

class MirrorApp : public AbstractRenderer {
public:
	bool init();
	void update(GameTimer& m_gameTimer);
	void render();

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

private:
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShaderAndInputLayout();
	void BuildRoomGeometry();
	void BuildSkullGeometry();
	void BuildMaterials();
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildPSOs();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem2*>& renderItem);
	void UpdateObjectCBs(GameTimer& gameTimer);
	void UpdateMaterialsCBs(GameTimer& gameTimer);
	void UpdateMainPassCB(GameTimer& gameTimer);
	void UpdateReflectedPassCB(GameTimer& gameTimer);
	void OnKeyboardInput(GameTimer& gameTimer);
	void UpdateCamera(GameTimer& gameTimer);

	std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> m_shaders;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> m_materials;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	std::vector<RenderItem2*> m_renderItemLayer[(int)RenderLayer2::Count];

	RenderItem2* m_skullRenderItem = nullptr;
	RenderItem2* m_reflectedSkullRenderItem = nullptr;
	RenderItem2* m_shadowedSkullRenderItem = nullptr;
	std::vector<std::unique_ptr<RenderItem2>> m_allRenderItems;

	std::vector<std::unique_ptr<FrameResource>> m_frameResources;
	FrameResource* m_currentFrameResource = nullptr;
	int m_currentFrameResourceIndex = 0;

	PassConstants m_mainPassCB;
	PassConstants m_reflectedPassCB;

	UINT m_cbvSrvDescriptorSize = 0;

	ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap = nullptr;

	XMFLOAT3 m_skullTranslation = { 0.0f, 1.0f, -5.0f };

};