#include "ShapesApp.h"
#include "GeometryGenerator.h"

bool ShapesApp::init() {
	ThrowIfFailed(m_graphicsCommandList->Reset(m_commandAllocator.Get(), nullptr));

	m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_camera.SetPosition(0.0f, 2.0f, -15.0f);

	m_waves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildLandGeometry();
	BuildWavesGeometryBuffers();
	BuildBoxGeometry();
	BuildTreeSpritesGeometry();
	BuildTestSpriteGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(m_graphicsCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_graphicsCommandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void ShapesApp::resize(float aspectRatio) {
	AbstractRenderer::resize(aspectRatio);
	m_camera.SetLens(0.25f * Pi, aspectRatio, 1.0f, 1000.0f);
}

void ShapesApp::render() {
	
	auto cmdListAlloc = m_currentFrameResource->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(m_graphicsCommandList->Reset(cmdListAlloc.Get(), m_PSOs["opaque"].Get()));

	m_graphicsCommandList->RSSetViewports(1, &m_screenViewport);
	m_graphicsCommandList->RSSetScissorRects(1, &m_scissorsRect);

	m_graphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	m_graphicsCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), (float*)&m_mainPassCB.FogColor, 0, nullptr);
	m_graphicsCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	m_graphicsCommandList->OMSetRenderTargets(1, &GetCurrentBackBufferView(), true, &GetDepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvDescriptorHeap.Get() };
	m_graphicsCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	m_graphicsCommandList->SetGraphicsRootSignature(m_rootSignature.Get());

	auto passCB = m_currentFrameResource->PassCB->Resource();
	m_graphicsCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer::Opaque]);

	m_graphicsCommandList->SetPipelineState(m_PSOs["alphaTested"].Get());
	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer::AlphaTested]);

	m_graphicsCommandList->SetPipelineState(m_PSOs["treeSprites"].Get());
	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	m_graphicsCommandList->SetPipelineState(m_PSOs["testSprites"].Get());
	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer::Opaque]);

	m_graphicsCommandList->SetPipelineState(m_PSOs["transparent"].Get());
	DrawRenderItems(m_graphicsCommandList.Get(), m_renderItemLayer[(int)RenderLayer::Transparent]);

	// Indicate a state transition on the resource usage.
	m_graphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	m_graphicsCommandList->Close();

	ID3D12CommandList* cmdLists[] = { m_graphicsCommandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(m_swapChain->Present(0, 0));
	m_currentBackBuffer = (m_currentBackBuffer + 1) % m_swapChainBufferCount;

	m_currentFrameResource->Fence = ++m_currentFence;
	m_commandQueue->Signal(m_fence.Get(), m_currentFence);
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems) {
	UINT objCBByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = m_currentFrameResource->ObjectCB->Resource();
	auto matCB = m_currentFrameResource->MaterialCB->Resource();

	for (size_t i = 0; i < renderItems.size(); ++i)
	{
		auto ri = renderItems[i];

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

void ShapesApp::UpdateCamera(const GameTimer& gameTimer) {
	// Convert spherical to cartesian coordinates.
	m_eyePos.x = m_radius * sinf(m_phi) * cosf(m_theta);
	m_eyePos.z = m_radius * sinf(m_phi) * sinf(m_theta);
	m_eyePos.y = m_radius * cosf(m_phi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(m_eyePos.x, m_eyePos.y, m_eyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&m_view, view);
}

void ShapesApp::UpdateObjectCBs(const GameTimer& gameTimer) {
	auto currentObjectCB = m_currentFrameResource->ObjectCB.get();
	for (auto& e : m_allRenderItems)
	{
		// Only update the cbuffer data if the constants have changed.
		// This needs to be tracked per frame resource
		if (e->numFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currentObjectCB->CopyData(e->objCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->numFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMaterialCBs(const GameTimer& gameTimer) {
	auto currentMaterialCB = m_currentFrameResource->MaterialCB.get();

	for (auto& e : m_materials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
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

void ShapesApp::UpdateMainPassCB(const GameTimer& gameTimer) {
	XMMATRIX view = m_camera.GetView();
	XMMATRIX proj = m_camera.GetProj();

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

	m_mainPassCB.EyePosW = m_camera.GetPosition3f();
	m_mainPassCB.RenderTargetSize = XMFLOAT2((float)m_clientWidth, (float)m_clientHeight);
	m_mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / m_clientWidth, 1.0f / m_clientHeight);
	m_mainPassCB.NearZ = 1.0f;
	m_mainPassCB.FarZ = 1000.0f;
	m_mainPassCB.TotalTime = gameTimer.TotalTime();
	m_mainPassCB.DeltaTime = gameTimer.DeltaTime();
	m_mainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	m_mainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	m_mainPassCB.Lights[0].Strength = { 0.9f, 0.9f, 0.8f };
	m_mainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	m_mainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	m_mainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	m_mainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currentPassCB = m_currentFrameResource->PassCB.get();
	currentPassCB->CopyData(0, m_mainPassCB);
}

void ShapesApp::UpdateWaves(const GameTimer& gameTimer) {
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((gameTimer.TotalTime() - t_base) >= 0.25f) 
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, m_waves->RowCount() - 5);
		int j = MathHelper::Rand(4, m_waves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		m_waves->Disturb(i, j, r);
	}
	// Update the wave simulation.
	m_waves->Update(gameTimer.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = m_currentFrameResource->WavesVB.get();

	for (int i = 0; i < m_waves->VertexCount(); ++i)
	{
		Vertex2 v;
		v.Pos = m_waves->Position(i);
		v.Normal = m_waves->Normal(i);

		v.TexC.x = 0.5f + v.Pos.x / m_waves->Width();
		v.TexC.y = 0.5f - v.Pos.z / m_waves->Depth();

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	m_wavesRenderItem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void ShapesApp::update(GameTimer& gameTimer) {
	OnKeyboardInput(gameTimer);
	UpdateCamera(gameTimer);

	// Cycle through the circular frame resource array.
	m_currentFrameResourceIndex = (m_currentFrameResourceIndex + 1) % gNumFrameResources;
	m_currentFrameResource = m_frameResources[m_currentFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (m_currentFrameResource->Fence != 0 && m_fence->GetCompletedValue() < m_currentFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gameTimer);
	UpdateObjectCBs(gameTimer);
	UpdateMaterialCBs(gameTimer);
	UpdateMainPassCB(gameTimer);
	UpdateWaves(gameTimer);
}

void ShapesApp::AnimateMaterials(const GameTimer& gameTimer) {
	auto waterMat = m_materials["water"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gameTimer.DeltaTime();
	tv += 0.02f * gameTimer.DeltaTime();

	if (tu >= 1.0f) tu -= 1.0f;
	if (tv >= 1.0f) tv -= 1.0f;
	
	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	waterMat->NumFramesDirty = gNumFrameResources;
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt) {
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState(VK_LEFT) & 0x8000)
		m_sunTheta -= 1.0f * dt;

	if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
		m_sunTheta += 1.0f * dt;

	if (GetAsyncKeyState(VK_UP) & 0x8000)
		m_sunPhi -= 1.0f * dt;

	if (GetAsyncKeyState(VK_DOWN) & 0x8000)
		m_sunPhi += 1.0f * dt;

	m_sunPhi = MathHelper::Clamp(m_sunPhi, 0.1f, XM_PIDIV2);

	if (GetAsyncKeyState('W') & 0x8000)
		m_camera.Walk(10.f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		m_camera.Walk(-10.f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		m_camera.Strafe(-10.f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		m_camera.Strafe(10.f * dt);

	m_camera.UpdateViewMatrix();
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y) {
	m_lastMousePos.x = x;
	m_lastMousePos.y = y;

	SetCapture(outputWindow);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y) {
	//if ((btnState & MK_LBUTTON) != 0) {
	//	// Make each pixel correspond to a quarter of a degree.
	//	float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePos.x));
	//	float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePos.y));

	//	// Update angles based on input to orbit camera around box.
	//	m_theta += dx;
	//	m_phi += dy;

	//	// Restrict the angle m_phi
	//	m_phi = MathHelper::Clamp(m_phi, 0.1f, Pi - 0.1f);
	//}
	//else if ((btnState & MK_RBUTTON) != 0) {
	//	// Make each pixel correspond to 0.005 unit in the scene.
	//	float dx = 0.005f * static_cast<float>(x - m_lastMousePos.x);
	//	float dy = 0.005f * static_cast<float>(y - m_lastMousePos.y);

	//	// Update the camera radius based on input.
	//	m_radius += dx - dy;

	//	// Restrict the radius.
	//	m_radius = MathHelper::Clamp(m_radius, 3.0f, 15.0f);
	//}

	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePos.y));

		m_camera.Pitch(dy);
		m_camera.RotateY(dx);
	}

	m_lastMousePos.x = x;
	m_lastMousePos.y = y;
}

void ShapesApp::LoadTextures() {
	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
		m_device.Get(), 
		m_graphicsCommandList.Get(), 
		grassTex->Filename.c_str(), 
		grassTex->Resource, 
		grassTex->UploadHeap
	));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
		m_device.Get(),
		m_graphicsCommandList.Get(),
		waterTex->Filename.c_str(),
		waterTex->Resource,
		waterTex->UploadHeap
	));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"Textures/treeArray2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
		m_device.Get(), 
		m_graphicsCommandList.Get(), 
		treeArrayTex->Filename.c_str(), 
		treeArrayTex->Resource, 
		treeArrayTex->UploadHeap
	));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"Textures/WireFence.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
		m_device.Get(),
		m_graphicsCommandList.Get(),
		fenceTex->Filename.c_str(),
		fenceTex->Resource,
		fenceTex->UploadHeap
	));

	auto testTree = std::make_unique<Texture>();
	testTree->Name = "testTreeTex";
	testTree->Filename = L"Textures/tree01S.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
		m_device.Get(),
		m_graphicsCommandList.Get(),
		testTree->Filename.c_str(),
		testTree->Resource,
		testTree->UploadHeap
	));

	m_textures[grassTex->Name] = std::move(grassTex);
	m_textures[waterTex->Name] = std::move(waterTex);
	m_textures[fenceTex->Name] = std::move(fenceTex);
	m_textures[treeArrayTex->Name] = std::move(treeArrayTex);
	m_textures[testTree->Name] = std::move(testTree);
}

void ShapesApp::BuildRootSignature() {

	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// Root parameter can be a table, a root descriptor or a root constant
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Order from most frequent to least frequent for performance
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);
	slotRootParameter[4].InitAsConstantBufferView(3);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// Create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	ThrowIfFailed(hr);

	ThrowIfFailed(m_device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.GetAddressOf()
	)));
}

void ShapesApp::BuildDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 5;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto grassTex = m_textures["grassTex"]->Resource;
	auto waterTex = m_textures["waterTex"]->Resource;
	auto fenceTex = m_textures["fenceTex"]->Resource;
	auto testTreeTex = m_textures["testTreeTex"]->Resource;
	auto treeArrayTex = m_textures["treeArrayTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	m_device->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, m_cbvSrvDescriptorSize);
	srvDesc.Format = waterTex->GetDesc().Format;
	m_device->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, m_cbvSrvDescriptorSize);
	srvDesc.Format = fenceTex->GetDesc().Format;
	m_device->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, m_cbvSrvDescriptorSize);
	srvDesc.Format = testTreeTex->GetDesc().Format;
	m_device->CreateShaderResourceView(testTreeTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, m_cbvSrvUavDescriptorSize);

	auto desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeArrayTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
	m_device->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);
}

void ShapesApp::BuildShadersAndInputLayout() {

	const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_shaders["standardVS"] = CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	m_shaders["opaquePS"] = CompileShader(L"Shaders\\Default.hlsl", defines, "PS", "ps_5_0");
	m_shaders["alphaTestedPS"] = CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_0");

	m_shaders["treeSpriteVS"] = CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_0");
	m_shaders["treeSpriteGS"] = CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_0");
	m_shaders["treeSpritePS"] = CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_0");

	m_shaders["testTreeSpriteVS"] = CompileShader(L"Shaders\\TestSprite.hlsl", nullptr, "VS", "vs_5_0");
	m_shaders["testTreeSpritePS"] = CompileShader(L"Shaders\\TestSprite.hlsl", nullptr, "PS", "ps_5_0");

	m_inputLayout = {
		{"POSITION", 0 , DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	m_treeSpriteInputLayout = 
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

float ShapesApp::GetHillsHeight(float x, float z) const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 ShapesApp::GetHillsNormal(float x, float z) const {
	XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z)
	);

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}

void ShapesApp::BuildLandGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

	std::vector<Vertex2> vertices(grid.Vertices.size());

	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		auto& p = grid.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
		vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex2);

	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex2);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;
	m_geometries["landGeo"] = std::move(geo);
}

void ShapesApp::BuildWavesGeometryBuffers() {
	std::vector<std::uint16_t> indices(3 * m_waves->TriangleCount()); // 3 indices per face
	assert(m_waves->VertexCount() < 0x0000ffff);

	// Iterate over each quad.
	int m = m_waves->RowCount();
	int n = m_waves->ColumnCount();
	int k = 0;
	for (int i = 0; i < m - 1; ++i)
	{
		for (int j = 0; j < n - 1; ++j)
		{
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;

			k += 6; // next quad
		}
	}

	UINT vbByteSize = m_waves->VertexCount() * sizeof(Vertex2);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex2);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	m_geometries["waterGeo"] = std::move(geo);
}

void ShapesApp::BuildBoxGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);

	std::vector<Vertex2> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); i++)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex2);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex2);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;
	m_geometries["boxGeo"] = std::move(geo);
}

void ShapesApp::BuildTreeSpritesGeometry() {
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	static const int treeCount = 16;
	std::array<TreeSpriteVertex, 16> vertices;
	for (UINT i = 0; i < treeCount; ++i)
	{
		float x = MathHelper::RandF(-45.0f, 45.0f);
		float z = MathHelper::RandF(-45.0f, 45.0f);
		float y = GetHillsHeight(x, z);

		// Move slightly above land
		y += 8.0f;

		vertices[i].Pos = XMFLOAT3(x, y, z);
		vertices[i].Size = XMFLOAT2(20.f, 20.f);
	}

	std::array<std::uint16_t, 16> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeSpritesGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_graphicsCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = CreateDefaultBuffer(m_device.Get(),m_graphicsCommandList.Get(), indices.data(), ibByteSize,geo->IndexBufferUploader);
	
	geo->VertexByteStride = sizeof(TreeSpriteVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;
	m_geometries["treeSpriteGeo"] = std::move(geo);
}

void ShapesApp::BuildTestSpriteGeometry() {
	struct TestSpriteVertex {
		XMFLOAT3 Pos;
		XMFLOAT2 
	};
}

void ShapesApp::BuildMaterials() {
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	water->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;

	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 3;
	treeSprites->DiffuseSrvHeapIndex = 3;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	m_materials["grass"] = std::move(grass);
	m_materials["water"] = std::move(water);
	m_materials["wirefence"] = std::move(wirefence);
	m_materials["treeSprites"] = std::move(treeSprites);
}

void ShapesApp::BuildRenderItems() {
	auto wavesRenderItem = std::make_unique<RenderItem>();
	wavesRenderItem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&wavesRenderItem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRenderItem->objCBIndex = 0;
	wavesRenderItem->Mat = m_materials["water"].get();
	wavesRenderItem->Geo = m_geometries["waterGeo"].get();
	wavesRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRenderItem->IndexCount = wavesRenderItem->Geo->DrawArgs["grid"].IndexCount;
	wavesRenderItem->StartIndexLocation = wavesRenderItem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRenderItem->BaseVertexLocation = wavesRenderItem->Geo->DrawArgs["grid"].BaseVertexLocation;

	m_wavesRenderItem = wavesRenderItem.get();

	m_renderItemLayer[(int)RenderLayer::Transparent].push_back(wavesRenderItem.get());

	auto gridRenderItem = std::make_unique<RenderItem>();
	gridRenderItem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRenderItem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRenderItem->objCBIndex = 1;
	gridRenderItem->Mat = m_materials["grass"].get();
	gridRenderItem->Geo = m_geometries["landGeo"].get();
	gridRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRenderItem->IndexCount = gridRenderItem->Geo->DrawArgs["grid"].IndexCount;
	gridRenderItem->StartIndexLocation = gridRenderItem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRenderItem->BaseVertexLocation = gridRenderItem->Geo->DrawArgs["grid"].BaseVertexLocation;

	m_renderItemLayer[(int)RenderLayer::Opaque].push_back(gridRenderItem.get());

	auto boxRenderItem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRenderItem->World, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
	boxRenderItem->objCBIndex = 2;
	boxRenderItem->Mat = m_materials["wirefence"].get();
	boxRenderItem->Geo = m_geometries["boxGeo"].get();
	boxRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRenderItem->IndexCount = boxRenderItem->Geo->DrawArgs["box"].IndexCount;
	boxRenderItem->StartIndexLocation = boxRenderItem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRenderItem->BaseVertexLocation = boxRenderItem->Geo->DrawArgs["box"].BaseVertexLocation;

	m_renderItemLayer[(int)RenderLayer::AlphaTested].push_back(boxRenderItem.get());

	auto treeSpritesRenderItem = std::make_unique<RenderItem>();
	treeSpritesRenderItem->World = MathHelper::Identity4x4();
	treeSpritesRenderItem->objCBIndex = 3;
	treeSpritesRenderItem->Mat = m_materials["treeSprites"].get();
	treeSpritesRenderItem->Geo = m_geometries["treeSpriteGeo"].get();
	treeSpritesRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRenderItem->IndexCount = treeSpritesRenderItem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRenderItem->StartIndexLocation = treeSpritesRenderItem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRenderItem->BaseVertexLocation = treeSpritesRenderItem->Geo->DrawArgs["points"].BaseVertexLocation;

	m_renderItemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRenderItem.get());
	
	m_allRenderItems.push_back(std::move(wavesRenderItem));
	m_allRenderItems.push_back(std::move(gridRenderItem));
	m_allRenderItems.push_back(std::move(boxRenderItem));
	m_allRenderItems.push_back(std::move(treeSpritesRenderItem));
}

void ShapesApp::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		m_frameResources.push_back(std::make_unique<FrameResource>(m_device.Get(),
			1, (UINT)m_allRenderItems.size(), (UINT)m_materials.size(), m_waves->VertexCount()));
	}
}

void ShapesApp::BuildPSOs() { 
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

	// PSO for alpha tested objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS = {
		reinterpret_cast<BYTE*>(m_shaders["alphaTestedPS"]->GetBufferPointer()),
		m_shaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&m_PSOs["alphaTested"])));

	// PSO for tree sprites
	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
	treeSpritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_shaders["treeSpriteVS"]->GetBufferPointer()),
		m_shaders["treeSpriteVS"]->GetBufferSize()
	};
	treeSpritePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(m_shaders["treeSpriteGS"]->GetBufferPointer()),
		m_shaders["treeSpriteGS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["treeSpritePS"]->GetBufferPointer()),
		m_shaders["treeSpritePS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { m_treeSpriteInputLayout.data(), (UINT)m_treeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&m_PSOs["treeSprites"])));
}