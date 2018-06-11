#pragma once

#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GameTimer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

static const int gNumFrameResources = 3;

class AbstractRenderer { 
public:
	struct ObjectConstants {
		XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
		XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	};

	struct Vertex
	{
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	struct Vertex1
	{
		XMFLOAT3 Pos;
		XMFLOAT3 Normal;
	};

	struct Vertex2
	{
		XMFLOAT3 Pos;
		XMFLOAT3 Normal;
		XMFLOAT2 TexC;
	};

	struct Vertex3
	{
		Vertex3() = default;
		Vertex3(float x, float y, float z, float nx, float ny, float nz, float u, float v) : 
			Pos(x, y, z), 
			Normal(nx, ny, nz), 
			TexC(u, v){}

		XMFLOAT3 Pos;
		XMFLOAT3 Normal;
		XMFLOAT2 TexC;
	};

public:

	virtual bool init() = 0;
	virtual void render() = 0;
	virtual void update(GameTimer& m_gameTimer) = 0;
	virtual void OnMouseDown(WPARAM btnState, int x, int y) = 0;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) = 0;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) = 0;

	void CreateDepthStencilBufferAndView();
	ID3D12Resource* GetCurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;

	float m_theta = 1.5f * XM_PI;
	float m_phi = XM_PIDIV2 - 0.1f;
	float m_radius = 50.0f;

	XMFLOAT4X4 m_world = MathHelper::Identity4x4();
	XMFLOAT4X4 m_view = MathHelper::Identity4x4();
	XMFLOAT4X4 m_proj = MathHelper::Identity4x4();
	XMFLOAT3 m_eyePos = { 0.0f, 0.0f, 0.0f };

	int m_clientWidth = 1600;
	int m_clientHeight = 1200;

	D3D12_VIEWPORT m_screenViewport;
	D3D12_RECT m_scissorsRect;

	static const int m_swapChainBufferCount = 2;
	int m_currentBackBuffer = 0;

	void FlushCommandQueue();
	bool InitDirect3D();
	void resize(float aspectRatio);
	void EnableDebugLayer();
	void CheckMsaaQualitySupport();
	void CreateCommandObjects();
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	ComPtr<IDXGISwapChain> m_swapChain;
	ComPtr<ID3D12Resource> m_swapChainBuffer[m_swapChainBufferCount];
	ComPtr<ID3D12Resource> m_depthStencilBuffer;

	HWND outputWindow = nullptr;
	POINT m_lastMousePos;

	UINT64 m_currentFence = 0;
	ComPtr<ID3D12Fence> m_fence;
	ComPtr<IDXGIFactory4> m_dxgiFactory;
	ComPtr<ID3D12Device> m_device;

	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ComPtr<ID3D12GraphicsCommandList> m_graphicsCommandList;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;

	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	ComPtr<ID3D12PipelineState> m_PSO = nullptr;
	bool m_isWireframe = false;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;

	std::unique_ptr<UploadBuffer<ObjectConstants>> m_objectCB = nullptr;

	// Shader stuff
	ComPtr<ID3DBlob> m_vsByteCode = nullptr;
	ComPtr<ID3DBlob> m_psByteCode = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_treeSpriteInputLayout;

	// Set true to use 4X MSAA (§4.1.8).  The default is false.
	bool m_4xMsaaState = false;    // 4X MSAA enabled
	UINT m_4xMsaaQuality = 0;      // quality level of 4X MSAA

	UINT m_rtvDescriptorSize = 0;
	UINT m_dsvDescriptorSize = 0;
	UINT m_cbvSrvUavDescriptorSize = 0;
};


