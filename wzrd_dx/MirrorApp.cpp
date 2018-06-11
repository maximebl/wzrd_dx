#include "MirrorApp.h"
#include "GeometryGenerator.h"

bool MirrorApp::init() {
	ThrowIfFailed(m_graphicsCommandList->Reset(m_commandAllocator.Get(), nullptr));
	m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShaderAndInputLayout();
	BuildRoomGeometry();
	BuildSkullGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	ThrowIfFailed(m_graphicsCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_graphicsCommandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	return true;
}

void MirrorApp::update(GameTimer& gameTimer) {
	OnKeyboardInput(gameTimer);
	UpdateCamera(gameTimer);

	m_currentFrameResourceIndex = (m_currentFrameResourceIndex + 1) % gNumFrameResources;
	m_currentFrameResource = m_frameResources[m_currentFrameResourceIndex].get();

	// Check if GPU has finished processing the commands up to the current frame resource
	if (m_currentFrameResource->Fence != 0 && m_fence->GetCompletedValue() < m_currentFrameResource->Fence)
	{
		// If not, wait until the GPU has completed commands up to this fence point.
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gameTimer);
	UpdateMaterialsCBs(gameTimer);
	UpdateMainPassCB(gameTimer);
	UpdateReflectedPassCB(gameTimer);
}

void MirrorApp::OnKeyboardInput(GameTimer& gameTimer) {

	//
	// Allow user to move skull.
	//

	const float dt = gameTimer.DeltaTime();

	if (GetAsyncKeyState('A') & 0x8000)
		m_skullTranslation.x -= 1.0f*dt;

	if (GetAsyncKeyState('D') & 0x8000)
		m_skullTranslation.x += 1.0f*dt;

	if (GetAsyncKeyState('W') & 0x8000)
		m_skullTranslation.y += 1.0f*dt;

	if (GetAsyncKeyState('S') & 0x8000)
		m_skullTranslation.y -= 1.0f*dt;

	// Don't let user move below ground plane.
	m_skullTranslation.y = MathHelper::Max(m_skullTranslation.y, 0.0f);

	// Update the new world matrix.
	XMMATRIX skullRotate = XMMatrixRotationY(0.5f*Pi);
	XMMATRIX skullScale = XMMatrixScaling(0.45f, 0.45f, 0.45f);
	XMMATRIX skullOffset = XMMatrixTranslation(m_skullTranslation.x, m_skullTranslation.y, m_skullTranslation.z);
	XMMATRIX skullWorld = skullRotate * skullScale*skullOffset;
	XMStoreFloat4x4(&m_skullRenderItem->World, skullWorld);

	// Update reflection world matrix.
	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX R = XMMatrixReflect(mirrorPlane);
	XMStoreFloat4x4(&m_reflectedSkullRenderItem->World, skullWorld * R);

	// Update shadow world matrix.
	XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
	XMVECTOR toMainLight = -XMLoadFloat3(&m_mainPassCB.Lights[0].Direction);
	XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
	XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
	XMStoreFloat4x4(&m_shadowedSkullRenderItem->World, skullWorld * S * shadowOffsetY);

	m_skullRenderItem->numFramesDirty = gNumFrameResources;
	m_reflectedSkullRenderItem->numFramesDirty = gNumFrameResources;
	m_shadowedSkullRenderItem->numFramesDirty = gNumFrameResources;
}

void MirrorApp::UpdateCamera(GameTimer& gameTimer) {
	// Convert from Spherical to Cartesian coordinates.
	m_eyePos.x = m_radius * sinf(m_phi) * cosf(m_theta);
	m_eyePos.z = m_radius * sinf(m_phi) * sinf(m_theta);
	m_eyePos.y = m_radius * cosf(m_phi);
			
	// Build the view matrix
	XMVECTOR pos = XMVectorSet(m_eyePos.x, m_eyePos.y, m_eyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&m_view, view);
}

void MirrorApp::UpdateObjectCBs(GameTimer& gameTimer) {
	auto currentObjectCB = m_currentFrameResource->ObjectCB.get();
	for (auto& e: m_allRenderItems)
	{
		if (e->numFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currentObjectCB->CopyData(e->objCBIndex, objConstants);

			e->numFramesDirty--;
		}
	}
}

void MirrorApp::UpdateMaterialsCBs(GameTimer& gameTimer) {
	auto currentMaterialCB = m_currentFrameResource->MaterialCB.get();
	for (auto& e : m_materials) {
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTansform, XMMatrixTranspose(matTransform));

			currentMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			mat->NumFramesDirty--;
		}
	}
}

void MirrorApp::UpdateMainPassCB(GameTimer& gameTimer) {
	XMMATRIX view = XMLoadFloat4x4(&m_view);
	XMMATRIX proj = XMLoadFloat4x4(&m_proj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&m_mainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_mainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&m_mainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_mainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&m_mainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&m_mainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	m_mainPassCB.EyePosW = m_eyePos;
	m_mainPassCB.RenderTargetSize = XMFLOAT2((float)m_clientWidth, (float)m_clientHeight);
	m_mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / m_clientWidth, 1.0f / m_clientHeight);
	m_mainPassCB.NearZ = 1.0f;
	m_mainPassCB.FarZ = 1000.0f;
	m_mainPassCB.TotalTime = gameTimer.TotalTime();
	m_mainPassCB.DeltaTime = gameTimer.DeltaTime();
	m_mainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	m_mainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	m_mainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	m_mainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	m_mainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	m_mainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	m_mainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	// Main pass stored in index 2
	auto currPassCB = m_currentFrameResource->PassCB.get();
	currPassCB->CopyData(0, m_mainPassCB);
}

void MirrorApp::UpdateReflectedPassCB(GameTimer& gameTimer) {
	m_reflectedPassCB = m_mainPassCB;

	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMMATRIX R = XMMatrixReflect(mirrorPlane);

	// Reflect the lighting
	for (int i = 0; i < 3; ++i) {
		XMVECTOR lightDir = XMLoadFloat3(&m_mainPassCB.Lights[i].Direction);
		XMVECTOR reflectedLightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&m_reflectedPassCB.Lights[i].Direction, reflectedLightDir);
	}

	auto currentPassCB = m_currentFrameResource->PassCB.get();
	currentPassCB->CopyData(1, m_reflectedPassCB);
}

void MirrorApp::render() {
	auto cmdListAlloc = m_currentFrameResource->CmdListAlloc;
	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(m_graphicsCommandList->Reset(cmdListAlloc.Get(), m_PSOs["opaque"].Get()));

	m_graphicsCommandList->RSSetViewports(1, &m_screenViewport);
	m_graphicsCommandList->RSSetScissorRects(1, &m_scissorsRect);

	m_graphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	m_graphicsCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), (float*)&m_mainPassCB.FogColor, 0, nullptr);
	m_graphicsCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_graphicsCommandList->OMSetRenderTargets(1, &GetCurrentBackBufferView(), true, &GetDepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvDescriptorHeap.Get() };
	m_graphicsCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	m_graphicsCommandList->SetGraphicsRootSignature(m_rootSignature.Get());

	UINT passCBByteSize = CalcConstantBufferByteSize(sizeof(PassConstants));

	// Draw opaque items (floors, walls, skull)
	auto passCB = m_currentFrameResource->PassCB->Resource();
	m_graphicsCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());
	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer2::Opaque]);

	// Mark the visible mirror pixels in the stencil buffer with the value 1
	m_graphicsCommandList->OMSetStencilRef(1);
	m_graphicsCommandList->SetPipelineState(m_PSOs["markStencilMirrors"].Get());
	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer2::Mirrors]);

	// Draw the reflection into the mirror only (only for pixels where the stencil buffer is 1).
	// Note that we must supply a different per-pass constant buffer -- one with the lights reflected.
	m_graphicsCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress() + 1 * passCBByteSize);
	m_graphicsCommandList->SetPipelineState(m_PSOs["drawStencilReflections"].Get());
	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer2::Reflected]);

	// Restore main pass constants and stencil ref.
	m_graphicsCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());
	m_graphicsCommandList->OMSetStencilRef(0);

	// Draw mirror transparency so reflection blends through.
	m_graphicsCommandList->SetPipelineState(m_PSOs["transparent"].Get());
	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer2::Transparent]);

	// Draw shadows
	m_graphicsCommandList->SetPipelineState(m_PSOs["shadow"].Get());
	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer2::Shadow]);

	m_graphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	ThrowIfFailed(m_graphicsCommandList->Close());

	ID3D12CommandList* cmdLists[] = { m_graphicsCommandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(m_swapChain->Present(0, 0));
	m_currentBackBuffer = (m_currentBackBuffer + 1) % m_swapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	m_currentFrameResource->Fence = ++m_currentFence;
	// Notify the fence when the GPU completes commands up to this fence point.
	m_commandQueue->Signal(m_fence.Get(), m_currentFence);
}

void MirrorApp::LoadTextures() {
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"Textures/bricks3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device.Get(), m_graphicsCommandList.Get(), bricksTex->Filename.c_str(), bricksTex->Resource, bricksTex->UploadHeap));

	auto checkboardTex = std::make_unique<Texture>();
	checkboardTex->Name = "checkboardTex";
	checkboardTex->Filename = L"Textures/checkboard.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device.Get(), m_graphicsCommandList.Get(), checkboardTex->Filename.c_str(), checkboardTex->Resource, checkboardTex->UploadHeap));

	auto iceTex = std::make_unique<Texture>();
	iceTex->Name = "iceTex";
	iceTex->Filename = L"Textures/ice.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device.Get(), m_graphicsCommandList.Get(), iceTex->Filename.c_str(), iceTex->Resource, iceTex->UploadHeap));

	auto whiteTex = std::make_unique<Texture>();
	whiteTex->Name = "whiteTex";
	whiteTex->Filename = L"Textures/white1x1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device.Get(), m_graphicsCommandList.Get(), whiteTex->Filename.c_str(), whiteTex->Resource, whiteTex->UploadHeap));

	m_textures[bricksTex->Name] = std::move(bricksTex);
	m_textures[checkboardTex->Name] = std::move(checkboardTex);
	m_textures[iceTex->Name] = std::move(iceTex);
	m_textures[whiteTex->Name] = std::move(whiteTex);
}

void MirrorApp::BuildRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(m_device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.GetAddressOf()))
	);
}

void MirrorApp::BuildDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 4;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = m_textures["bricksTex"]->Resource;
	auto checkboardTex = m_textures["checkboardTex"]->Resource;
	auto iceTex = m_textures["iceTex"]->Resource;
	auto whiteTex = m_textures["whiteTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	m_device->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, m_cbvSrvDescriptorSize);
	srvDesc.Format = checkboardTex->GetDesc().Format;
	m_device->CreateShaderResourceView(checkboardTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, m_cbvSrvDescriptorSize);
	srvDesc.Format = iceTex->GetDesc().Format;
	m_device->CreateShaderResourceView(iceTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, m_cbvSrvDescriptorSize);
	srvDesc.Format = whiteTex->GetDesc().Format;
	m_device->CreateShaderResourceView(whiteTex.Get(), &srvDesc, hDescriptor);
}

void MirrorApp::BuildShaderAndInputLayout() {
	const D3D_SHADER_MACRO defines[] = { "FOG", "1", NULL, NULL };
	const D3D_SHADER_MACRO alphaTestDefines[] = { "FOG", "1", "ALPHA_TEST", "1", NULL, NULL };

	m_shaders["standardVS"] = CompileShader(L"Shaders\\MirrorApp.hlsl", nullptr, "VS", "vs_5_0");
	m_shaders["opaquePS"] = CompileShader(L"Shaders\\MirrorApp.hlsl", defines, "PS", "ps_5_0");
	m_shaders["alphaTestedPS"] = CompileShader(L"Shaders\\MirrorApp.hlsl", alphaTestDefines, "PS", "ps_5_0");

	m_inputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void MirrorApp::BuildRoomGeometry() {
	std::array<Vertex3, 20> vertices =
	{
		// Floor: Observe we tile texture coordinates.
		Vertex3(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
		Vertex3(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		Vertex3(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
		Vertex3(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

		// Wall: Observe we tile texture coordinates, and that we
		// leave a gap in the middle for the mirror.
		Vertex3(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
		Vertex3(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex3(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
		Vertex3(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

		Vertex3(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
		Vertex3(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex3(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
		Vertex3(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

		Vertex3(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
		Vertex3(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex3(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
		Vertex3(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

		// Mirror
		Vertex3(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
		Vertex3(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex3(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		Vertex3(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
	};

	std::array<std::int16_t, 30> indices =
	{
		// Floor
		0, 1, 2,
		0, 2, 3,

		// Walls
		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// Mirror
		16, 17, 18,
		16, 18, 19
	};

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = 6;
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.IndexCount = 18;
	wallSubmesh.StartIndexLocation = 6;
	wallSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.IndexCount = 6;
	mirrorSubmesh.StartIndexLocation = 24;
	mirrorSubmesh.BaseVertexLocation = 0;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex3);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "roomGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex3);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["floor"] = floorSubmesh;
	geo->DrawArgs["wall"] = wallSubmesh;
	geo->DrawArgs["mirror"] = mirrorSubmesh;

	m_geometries[geo->Name] = std::move(geo);
}

void MirrorApp::BuildSkullGeometry() {
	std::ifstream fin("Models/skull.txt");

	if (!fin) {
		MessageBox(0, L"Models/skull.txt not found", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex3> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
		vertices[i].TexC = { 0.0f, 0.0f };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex3);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex3);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = submesh;

	m_geometries[geo->Name] = std::move(geo);
}

void MirrorApp::BuildMaterials() {
	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrvHeapIndex = 0;
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto checkerTile = std::make_unique<Material>(); 
	checkerTile->Name = "checkertile";
	checkerTile->MatCBIndex = 1;
	checkerTile->DiffuseSrvHeapIndex = 1;
	checkerTile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	checkerTile->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	checkerTile->Roughness = 0.3f;

	auto icemirror = std::make_unique<Material>();
	icemirror->Name = "icemirror";
	icemirror->MatCBIndex = 2;
	icemirror->DiffuseSrvHeapIndex = 2;
	icemirror->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
	icemirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	icemirror->Roughness = 0.5f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 3;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;

	auto shadowMat = std::make_unique<Material>();
	shadowMat->Name = "shadowMat";
	shadowMat->MatCBIndex = 4;
	shadowMat->DiffuseSrvHeapIndex = 3;
	shadowMat->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
	shadowMat->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
	shadowMat->Roughness = 0.0f;


	m_materials["bricks"] = std::move(bricks);
	m_materials["checkertile"] = std::move(checkerTile);
	m_materials["icemirror"] = std::move(icemirror);
	m_materials["skullMat"] = std::move(skullMat);
	m_materials["shadowMat"] = std::move(shadowMat);
}

void MirrorApp::BuildRenderItems() {
	auto floorRenderItem = std::make_unique<RenderItem2>();
	floorRenderItem->World = MathHelper::Identity4x4();
	floorRenderItem->TexTransform = MathHelper::Identity4x4();
	floorRenderItem->objCBIndex = 0;
	floorRenderItem->Mat = m_materials["checkertile"].get();
	floorRenderItem->Geo = m_geometries["roomGeo"].get();
	floorRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRenderItem->IndexCount = floorRenderItem->Geo->DrawArgs["floor"].IndexCount;
	floorRenderItem->StartIndexLocation = floorRenderItem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRenderItem->BaseVertexLocation = floorRenderItem->Geo->DrawArgs["floor"].BaseVertexLocation;
	m_renderItemLayer[(int)RenderLayer2::Opaque].push_back(floorRenderItem.get());

	auto wallsRenderItem = std::make_unique<RenderItem2>();
	wallsRenderItem->World = MathHelper::Identity4x4();
	wallsRenderItem->TexTransform = MathHelper::Identity4x4();
	wallsRenderItem->objCBIndex = 1;
	wallsRenderItem->Mat = m_materials["bricks"].get();
	wallsRenderItem->Geo = m_geometries["roomGeo"].get();
	wallsRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRenderItem->IndexCount = wallsRenderItem->Geo->DrawArgs["wall"].IndexCount;
	wallsRenderItem->StartIndexLocation = wallsRenderItem->Geo->DrawArgs["wall"].StartIndexLocation;
	wallsRenderItem->BaseVertexLocation = wallsRenderItem->Geo->DrawArgs["wall"].BaseVertexLocation;
	m_renderItemLayer[(int)RenderLayer2::Opaque].push_back(wallsRenderItem.get());

	auto skullRenderItem = std::make_unique<RenderItem2>();
	skullRenderItem->World = MathHelper::Identity4x4();
	skullRenderItem->TexTransform = MathHelper::Identity4x4();
	skullRenderItem->objCBIndex = 2;
	skullRenderItem->Mat = m_materials["skullMat"].get();
	skullRenderItem->Geo = m_geometries["skullGeo"].get();
	skullRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRenderItem->IndexCount = skullRenderItem->Geo->DrawArgs["skull"].IndexCount;
	skullRenderItem->StartIndexLocation = skullRenderItem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRenderItem->BaseVertexLocation = skullRenderItem->Geo->DrawArgs["skull"].BaseVertexLocation;
	m_skullRenderItem = skullRenderItem.get();
	m_renderItemLayer[(int)RenderLayer2::Opaque].push_back(skullRenderItem.get());

	auto reflectedSkullRenderItem = std::make_unique<RenderItem2>();
	*reflectedSkullRenderItem = *skullRenderItem;
	reflectedSkullRenderItem->objCBIndex = 3;
	m_reflectedSkullRenderItem = reflectedSkullRenderItem.get();
	m_renderItemLayer[(int)RenderLayer2::Reflected].push_back(reflectedSkullRenderItem.get());
	
	auto shadowedSkullRenderItem = std::make_unique<RenderItem2>();
	*shadowedSkullRenderItem = *skullRenderItem;
	shadowedSkullRenderItem->objCBIndex = 4;
	shadowedSkullRenderItem->Mat = m_materials["shadowMat"].get();
	m_shadowedSkullRenderItem = shadowedSkullRenderItem.get();
	m_renderItemLayer[(int)RenderLayer2::Shadow].push_back(shadowedSkullRenderItem.get());

	auto mirrorRenderItem = std::make_unique<RenderItem2>();
	mirrorRenderItem->World = MathHelper::Identity4x4();
	mirrorRenderItem->TexTransform = MathHelper::Identity4x4();
	mirrorRenderItem->objCBIndex = 5;
	mirrorRenderItem->Mat = m_materials["icemirror"].get();
	mirrorRenderItem->Geo = m_geometries["roomGeo"].get();
	mirrorRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mirrorRenderItem->IndexCount = mirrorRenderItem->Geo->DrawArgs["mirror"].IndexCount;
	mirrorRenderItem->StartIndexLocation = mirrorRenderItem->Geo->DrawArgs["mirror"].StartIndexLocation;
	mirrorRenderItem->BaseVertexLocation = mirrorRenderItem->Geo->DrawArgs["mirror"].BaseVertexLocation;
	m_renderItemLayer[(int)RenderLayer2::Mirrors].push_back(mirrorRenderItem.get());
	m_renderItemLayer[(int)RenderLayer2::Transparent].push_back(mirrorRenderItem.get());

	m_allRenderItems.push_back(std::move(floorRenderItem));
	m_allRenderItems.push_back(std::move(wallsRenderItem));
	m_allRenderItems.push_back(std::move(skullRenderItem));
	m_allRenderItems.push_back(std::move(reflectedSkullRenderItem));
	m_allRenderItems.push_back(std::move(shadowedSkullRenderItem));
	m_allRenderItems.push_back(std::move(mirrorRenderItem));
}

void MirrorApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem2*>& renderItem) {
	UINT objCBByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = CalcConstantBufferByteSize(sizeof(MaterialConstants)); 

	auto objectCB = m_currentFrameResource->ObjectCB->Resource();
	auto matCB = m_currentFrameResource->MaterialCB->Resource();

	for (size_t i = 0; i < renderItem.size(); i++)
	{
		auto ri = renderItem[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, m_cbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->objCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void MirrorApp::BuildFrameResources() {
	for (size_t i = 0; i < gNumFrameResources; ++i)
	{
		m_frameResources.push_back(std::make_unique<FrameResource>(m_device.Get(), 2, (UINT)m_allRenderItems.size(), (UINT)m_materials.size()));
	}
}

void MirrorApp::BuildPSOs() {

	// PSO for opaque objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
	opaquePsoDesc.pRootSignature = m_rootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_shaders["standardVS"]->GetBufferPointer()), 
		m_shaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["opaquePS"]->GetBufferPointer()),
		m_shaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = m_backBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = m_depthStencilFormat;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&m_PSOs["opaque"])));
	
	// PSO for transparent objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	
	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&m_PSOs["transparent"])));

	// PSO for marking stencil mirrors.
	CD3DX12_BLEND_DESC mirrorBlendState(D3D12_DEFAULT);
	mirrorBlendState.RenderTarget[0].RenderTargetWriteMask = 0;

	D3D12_DEPTH_STENCIL_DESC mirrorDepthStencilDesc;
	mirrorDepthStencilDesc.DepthEnable = true;
	mirrorDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	mirrorDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	mirrorDepthStencilDesc.StencilEnable = true;
	mirrorDepthStencilDesc.StencilReadMask = 0xff;
	mirrorDepthStencilDesc.StencilWriteMask = 0xff;

	mirrorDepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	mirrorDepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	// We are not rendering backfacing polygons, so these settings do not matter.
	mirrorDepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	mirrorDepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC markMirrorsPsoDesc = opaquePsoDesc;
	markMirrorsPsoDesc.BlendState = mirrorBlendState;
	markMirrorsPsoDesc.DepthStencilState = mirrorDepthStencilDesc;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&markMirrorsPsoDesc, IID_PPV_ARGS(&m_PSOs["markStencilMirrors"])));

	// PSO for stencil reflections.
	D3D12_DEPTH_STENCIL_DESC reflectionsDepthStencilDesc;
	reflectionsDepthStencilDesc.DepthEnable = true;
	reflectionsDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	reflectionsDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	reflectionsDepthStencilDesc.StencilEnable = true;
	reflectionsDepthStencilDesc.StencilReadMask = 0xff;
	reflectionsDepthStencilDesc.StencilWriteMask = 0xff;

	reflectionsDepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	// We are not rendering backfacing polygons, so these settings do not matter.
	reflectionsDepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawReflectionsPsoDesc = opaquePsoDesc;
	drawReflectionsPsoDesc.DepthStencilState = reflectionsDepthStencilDesc;
	drawReflectionsPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	drawReflectionsPsoDesc.RasterizerState.FrontCounterClockwise = true;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&drawReflectionsPsoDesc, IID_PPV_ARGS(&m_PSOs["drawStencilReflections"])));

	// PSO for shadow objects.

	D3D12_DEPTH_STENCIL_DESC shadowDepthStencilDesc;
	shadowDepthStencilDesc.DepthEnable = true;
	shadowDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	shadowDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	shadowDepthStencilDesc.StencilEnable = true;
	shadowDepthStencilDesc.StencilReadMask = 0xff;
	shadowDepthStencilDesc.StencilWriteMask = 0xff;

	shadowDepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	// We are not rendering backfacing polygons, so these settings do not matter.
	shadowDepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = transparentPsoDesc;
	shadowPsoDesc.DepthStencilState = shadowDepthStencilDesc;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&m_PSOs["shadow"])));
}

void MirrorApp::OnMouseDown(WPARAM btnState, int x, int y) {
	m_lastMousePos.x = x;
	m_lastMousePos.y = y;

	SetCapture(outputWindow);
}

void MirrorApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void MirrorApp::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePos.y));

		// Update angles based on input to orbit camera around box.
		m_theta += dx;
		m_phi += dy;

		// Restrict the angle m_phi
		m_phi = MathHelper::Clamp(m_phi, 0.1f, Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0) {
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(x - m_lastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - m_lastMousePos.y);

		// Update the camera radius based on input.
		m_radius += dx - dy;

		// Restrict the radius.
		m_radius = MathHelper::Clamp(m_radius, 3.0f, 15.0f);
	}
	m_lastMousePos.x = x;
	m_lastMousePos.y = y;
}