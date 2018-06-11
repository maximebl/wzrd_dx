#pragma once

#include "Utilities.h"
#include "GameTimer.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "MeshGeometry.h"

using namespace Microsoft::WRL;
using namespace DirectX;
using namespace DirectX::PackedVector;



class WZRDRenderer {

public:
	WZRDRenderer();
	bool InitDirect3D();
	bool InitBoxApp();
	void Resize(float aspectRatio);
	void Render();
	void Update(GameTimer& m_gameTimer);
	HWND outputWindow = nullptr;
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

private:

	struct ObjectConstants {
		XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
	};

	struct Vertex
	{
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	void FlushCommandQueue();
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildPSO();

	XMFLOAT4X4 m_world = MathHelper::Identity4x4();
	XMFLOAT4X4 m_view = MathHelper::Identity4x4();
	XMFLOAT4X4 m_proj = MathHelper::Identity4x4();

	float m_theta = 1.5f * XM_PI;
	float m_phi = XM_PIDIV4;
	float m_radius = 5.0f;

	POINT m_lastMousePos;

	ID3D12Resource* GetCurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;

	ComPtr<IDXGIFactory4> m_dxgiFactory;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_currentFence = 0;

	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_graphicsCommandList;

	ComPtr<IDXGISwapChain> m_swapChain;
	static const int m_swapChainBufferCount = 2;
	int m_currentBackBuffer = 0;
	ComPtr<ID3D12Resource> m_swapChainBuffer[m_swapChainBufferCount];
	ComPtr<ID3D12Resource> m_depthStencilBuffer;

	ComPtr<ID3D12PipelineState> m_PSO = nullptr;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
	std::unique_ptr<UploadBuffer<ObjectConstants>> m_objectCB = nullptr;
	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

	D3D12_VIEWPORT m_screenViewport;
	D3D12_RECT m_scissorsRect;

	// Shader stuff
	ComPtr<ID3DBlob> m_vsByteCode = nullptr;
	ComPtr<ID3DBlob> m_psByteCode = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

	// Set true to use 4X MSAA (§4.1.8).  The default is false.
	bool m_4xMsaaState = false;    // 4X MSAA enabled
	UINT m_4xMsaaQuality = 0;      // quality level of 4X MSAA

	int m_clientWidth = 1600;
	int m_clientHeight = 1200;

	UINT m_rtvDescriptorSize = 0;
	UINT m_dsvDescriptorSize = 0;
	UINT m_cbvSrvUavDescriptorSize = 0;

	void EnableDebugLayer();
	void CheckMsaaQualitySupport();

	void CreateCommandObjects();
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateDepthStencilBufferAndView();

	// Box
	std::unique_ptr<MeshGeometry> m_boxGeometry;
};