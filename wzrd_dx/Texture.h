#pragma once

#include "d3dx12.h"
#include <DirectXMath.h>
#include <string>
#include <wrl.h>

struct Texture {
	std::string Name;
	std::wstring Filename;
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};