#pragma once

#include <d3d12.h>
#include <DirectXMath.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Model
{
	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 textureCoordinates;
	};

	uint32_t m_numVertices;
	uint16_t m_numIndices;

	// App resources.
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

public:
	Model();
	~Model();

	HRESULT Load(const char* filename, ID3D12Device* device);
	void DrawModel(ID3D12GraphicsCommandList* commandList, const UINT instanceCount = 1);
};