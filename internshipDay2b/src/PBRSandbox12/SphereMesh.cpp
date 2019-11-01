#include "stdafx.h"
#include "DXSample.h"
#include "DXSampleHelper.h"

#include "SphereMesh.h"
#include <vector>

SphereMesh::SphereMesh() : 
	m_vertexSize(0),
	m_indexSize(0)
{

}

SphereMesh::~SphereMesh()
{

}

void SphereMesh::InitVertex(ID3D12Device* pDevice, const uint32_t slices, const uint32_t stacks, const bool isReverse)
{
	std::vector<Vertex> vertexArray;

	for (uint32_t j = 0; j <= stacks; ++j) {
		float ph = 3.141593f * static_cast<float>(j) / static_cast<float>(stacks);
		float y = cosf(ph);
		float r = sinf(ph);

		for (uint32_t i = 0; i <= slices; ++i) {
			Vertex v;

			float th = 2.0f * 3.141593f * static_cast<float>(i) / static_cast<float>(slices);
			float x = r * cosf(th);
			float z = r * sinf(th);

			v.position = DirectX::XMFLOAT3(x, y, z);

			DirectX::XMVECTOR vec = DirectX::XMLoadFloat3(&v.position);
			DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(vec));

			if (isReverse)
			{
				v.normal.x *= -1.0f;
				v.normal.y *= -1.0f;
				v.normal.z *= -1.0f;
			}

			v.uv = DirectX::XMFLOAT2(static_cast<float>(i) / static_cast<float>(slices - 1),
				static_cast<float>(j) / static_cast<float>(stacks - 1));

			vertexArray.push_back(v);
		}
	}

	m_vertexSize = static_cast<uint32_t>(vertexArray.size());
	uint32_t vertexBufferSize = sizeof(Vertex) * m_vertexSize;

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer)));

	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, &vertexArray[0], sizeof(Vertex)*m_vertexSize);
	m_vertexBuffer->Unmap(0, nullptr);

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_vertexBufferView.SizeInBytes = vertexBufferSize;

	std::vector<uint32_t> indexArray;

	for (uint32_t j = 0; j < stacks; ++j) {
		for (uint32_t i = 0; i < slices; ++i) {
			uint32_t count = (slices + 1) * j + i;

			indexArray.push_back(count);
			indexArray.push_back(count + 1);
			indexArray.push_back(count + slices + 2);

			indexArray.push_back(count);
			indexArray.push_back(count + slices + 2);
			indexArray.push_back(count + slices + 1);
		}
	}

	m_indexSize = static_cast<uint32_t>(indexArray.size());
	uint32_t indexBufferSize =  sizeof(uint32_t) * m_indexSize;

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)));

	UINT8* pIndexDataBegin;
	ThrowIfFailed(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, &indexArray[0], sizeof(uint32_t)*m_indexSize);
	m_indexBuffer->Unmap(0, nullptr);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_indexBufferView.SizeInBytes = indexBufferSize;
}

void SphereMesh::Init(ID3D12Device* pDevice, const uint32_t slices, const uint32_t stacks, const bool isReverse)
{
	InitVertex(pDevice,slices,stacks,isReverse);
}

HRESULT SphereMesh::DrawMesh(ID3D12GraphicsCommandList* pCommandList)
{
	HRESULT hr = S_OK;

	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	pCommandList->IASetIndexBuffer(&m_indexBufferView);
	pCommandList->DrawIndexedInstanced(m_indexSize, 1, 0, 0, 0);

	return hr;
}