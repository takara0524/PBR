#include "stdafx.h"
#include "DXSampleHelper.h"
#include "Model12.h"
#include <vector>
#include <fstream>

Model::Model()
{

}

Model::~Model()
{

}

//
HRESULT Model::Load(const char* filename, ID3D12Device* device)
{
	HRESULT hr = S_OK;

	struct VBO
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 textureCoordinates;
	};

	std::ifstream vboFile(filename, std::ifstream::in | std::ifstream::binary);
	if (!vboFile.is_open())
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

	vboFile.read(reinterpret_cast<char*>(&m_numVertices), sizeof(uint32_t));
	if (!m_numVertices)
		return E_FAIL;

	vboFile.read(reinterpret_cast<char*>(&m_numIndices), sizeof(uint32_t));
	if (!m_numIndices)
		return E_FAIL;

	std::vector<VBO> vboChache;
	vboChache.resize(m_numVertices);
	vboFile.read(reinterpret_cast<char*>(&vboChache.front()), sizeof(VBO) * m_numVertices);

	std::vector<uint16_t> indices;
	indices.resize(m_numIndices);
	vboFile.read(reinterpret_cast<char*>(&indices.front()), sizeof(uint16_t) * m_numIndices);

	vboFile.close();

	const size_t vertexBufferSize = static_cast<size_t>(sizeof(Vertex)*m_numVertices);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer)));
	
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);	
	ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, &vboChache[0], vertexBufferSize);
	m_vertexBuffer->Unmap(0, nullptr);

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferSize);

	const size_t indexBufferSize = static_cast<size_t>(sizeof(uint16_t) * m_numIndices);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)));


	UINT8* pIndexDataBegin;
	ThrowIfFailed(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, &indices[0], indexBufferSize);
	m_indexBuffer->Unmap(0, nullptr);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_indexBufferView.SizeInBytes = static_cast<UINT>(indexBufferSize);

	return hr;
}

//
void Model::DrawModel(ID3D12GraphicsCommandList* commandList, const UINT instanceCount)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	commandList->IASetIndexBuffer(&m_indexBufferView);

	commandList->DrawIndexedInstanced(m_numIndices, instanceCount, 0, 0, 0);
}