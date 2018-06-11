#pragma once

#include "Utilities.h"

using namespace Microsoft::WRL;

template<typename T>
class UploadBuffer {
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) :
		m_isConstantBuffer(isConstantBuffer)
	{
		m_elementByteSize = sizeof(T);

		// Constant buffer elements need to be multiples of 256 bytes.
		//
		// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
		//		UINT64 OffsetInBytes;
		//		UINT SizeInBytes;
		// } D3D12_CONSTANT_BUFFER_VIEW_DESC

		if (isConstantBuffer) {
			m_elementByteSize = CalcConstantBufferByteSize(sizeof(T));
		}

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(m_elementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_uploadBuffer)
		));

		ThrowIfFailed(m_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedData)));
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;

	UploadBuffer& operator =(const UploadBuffer& rhs) = delete;

	~UploadBuffer() {
		if (m_uploadBuffer != nullptr)
			m_uploadBuffer->Unmap(0, nullptr);

		m_mappedData = nullptr;
	}

	ID3D12Resource* Resource()const {
		return m_uploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data) {
		memcpy(&m_mappedData[elementIndex*m_elementByteSize], &data, sizeof(T));
	}

private:
	ComPtr<ID3D12Resource> m_uploadBuffer;
	BYTE* m_mappedData = nullptr;

	UINT m_elementByteSize = 0;
	bool m_isConstantBuffer = false;
};