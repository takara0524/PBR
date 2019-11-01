#pragma once

#include <d3d12.h>
#include <DirectXMath.h>

class SphereMesh
{
	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
	};

	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	uint32_t m_vertexSize;
	uint32_t m_indexSize;

	void InitVertex(ID3D12Device* pDevice, const uint32_t slices, const uint32_t stacks, const bool isReverse);

public:
	SphereMesh();
	~SphereMesh();

	void Init(ID3D12Device* pDevice, const uint32_t slices, const uint32_t stacks, const bool isReverse = false);
	HRESULT DrawMesh(ID3D12GraphicsCommandList* pCommandList);
};